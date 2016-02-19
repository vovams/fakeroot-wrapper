#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <tuple>
#include <regex>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define APP_NAME    "fakeroot-wrapper"
#define APP_VERSION "0.1"

enum class Success
{
    OK,
    FAIL
};

static std::pair<int, Success> runFakeroot(const std::string& envFilename, bool load, bool save,
                                           const char* const* args, int argCount);
static Success convertToNative(const std::string& envFilename, const std::string& directory,
                               const std::string& nativeFilename);
static Success convertFromNative(const std::string& nativeFilename, const std::string& directory,
                                 const std::string& envFilename);
static void printHelp();

static std::pair<std::map<std::pair<dev_t, ino_t>, std::string>, Success> readNativeEnvFile(
        const std::string& filename);

static std::string dirname(const std::string& path);
static std::string basename(const std::string& path);

int main(int argc, char** argv)
{
    std::string directoryPath = ".";
    std::string envFilename = ".fakerootenv";
    bool envFilenameSet = false;
    bool createHiddenEnvFile = false;
    bool updateEnvFile = true;
    bool convertInput = true;
    
    while (1)
    {
        switch (getopt(argc, argv, "+d:f:hric"))
        {
            case 'd':
                directoryPath = optarg;
                if (directoryPath.empty())
                {
                    std::cerr << APP_NAME ": invalid -d argument; see -h" << std::endl;
                    return 1;
                }
                
                if (!envFilenameSet)
                {
                    std::string dir = basename(directoryPath);
                    if (dir == "/")
                        envFilename = "/.fakerootenv";
                    else if (dir == "." || dir == "..")
                        envFilename = directoryPath + "/.fakerootenv";
                    else
                        envFilename = dirname(directoryPath) + '/' + dir + ".fakerootenv";
                }
                break;
            
            case 'f':
                envFilename = optarg;
                envFilenameSet = true;
                
                if (envFilename.empty())
                {
                    std::cerr << APP_NAME ": invalid -f argument; see -h" << std::endl;
                    return 1;
                }
                break;
            
            case 'r':
                updateEnvFile = false;
                break;
            
            case 'i':
                convertInput = false;
                break;
            
            case 'c':
                createHiddenEnvFile = true;
                break;
            
            case 'h':
                printHelp();
                return 0;
            
            case '?':
                std::cerr << APP_NAME ": try -h" << std::endl;
                return 1;
            
            case -1:
                goto break_while;
        }
    }
break_while:

    const char* const* fakerootArgs = argv + optind;
    int fakerootArgCount = argc - optind;
    
    std::string nativeEnvFilename = ( convertInput ? envFilename + ".tmp" : envFilename );
    
#if 0
    std::cout << "dir = " << directoryPath << '\n'
              << "env = " << envFilename << '\n'
              << "tmp = " << nativeEnvFilename << '\n'
              << "upd = " << updateEnvFile << '\n'
              << "cnv = " << convertInput << '\n'
              << "pss = " << ( *fakerootArgs ?: "(null)" ) << " (" << fakerootArgCount << ")\n"
              << std::flush;
#endif
    
    bool loadEnvFile = true;
    if (access(envFilename.c_str(), F_OK) == -1 && errno == ENOENT)
    {
        if (basename(envFilename)[0] == '.' && !createHiddenEnvFile && !envFilenameSet)
        {
            std::cerr << APP_NAME ": " << envFilename
                      << " does not exist, pass -c to create (also try -h)" << std::endl;
            return 2;
        }
        
        nativeEnvFilename = envFilename;
        convertInput = false;
        loadEnvFile = false;
        
        if (!updateEnvFile)
        {
            std::cerr << APP_NAME ": warning: " << envFilename
                      << " does not exist and -r given; " APP_NAME " will be a no-op" << std::endl;
        }
    }
    
    if (convertInput)
    {
        if (convertToNative(envFilename, directoryPath, nativeEnvFilename) != Success::OK)
            return 2;
    }
    
    int exitCode;
    Success fakerootSuccess;
    std::tie(exitCode, fakerootSuccess) = runFakeroot(nativeEnvFilename, loadEnvFile, updateEnvFile,
                                                      fakerootArgs, fakerootArgCount);
    
    if (fakerootSuccess == Success::FAIL)
        exitCode = 3;
    
    if (updateEnvFile && fakerootSuccess == Success::OK)
    {
        if (convertFromNative(nativeEnvFilename, directoryPath, envFilename) != Success::OK)
            return 4;
    }
    
    if (convertInput)
    {
        if (std::remove(nativeEnvFilename.c_str()) != 0)
        {
            std::cerr << APP_NAME ": can not remove " << nativeEnvFilename << std::strerror(errno)
                      << std::endl;
        }
    }
    
    return exitCode;
}

void printHelp()
{
    std::cout << APP_NAME " version " APP_VERSION "\n"
                 "\n"
                 "Usage: " APP_NAME " [ options ] [ -- ] [ fakeroot args ... ]\n"
                 "\n"
                 "Options:\n"
                 "\n"
                 "-d directory\n"
                 "    Preserve file attributes in specified directory. Defaults to current\n"
                 "    working directory (\".\").\n"
                 "\n"
                 "-f fakerootenv_file\n"
                 "    Specify fakeroot environment file path. Defaults to \".fakerootenv\" if\n"
                 "    no -d option is specified, or \"dirname(path)/basename(path).fakerootenv\"\n"
                 "    otherwise (see dirname and basename shell commands for more info).\n"
                 "\n"
                 "-r\n"
                 "    Read only. Do not save changes of file attributes.\n"
                 "\n"
                 "-i\n"
                 "    Import. File specified with -f (or the default file) is a file generated\n"
                 "    with fakeroot's -s option. It will be passed to fakeroot without changes\n"
                 "    and overwritten when fakeroot exits.\n"
                 "\n"
                 "-c\n"
                 "    Create. If no -f option is specified, an autogenerated fakeroot\n"
                 "    environment file name may be a hidden file (begin with a dot). If that\n"
                 "    file does not exist already, running " APP_NAME " will create it,\n"
                 "    which may be counterintuitive. Therefore " APP_NAME " will fail\n"
                 "    instead. Pass this option to disable this check.\n"
                 "\n"
                 "-h\n"
                 "    Guess what it does.\n"
                 "\n"
                 "--\n"
                 "    Marks the end of options for " APP_NAME " and the beginning of\n"
                 "    fakeroot's arguments. Additionally the first non-option argument does\n"
                 "    the same.\n"
              << std::flush;
}

Success convertToNative(const std::string& envFilename, const std::string& directory,
                        const std::string& nativeFilename)
{
    std::ifstream envFile (envFilename);
    if (!envFile)
    {
        std::cerr << APP_NAME << ": can not open " << envFilename << ": " << std::strerror(errno)
                  << std::endl;
        return Success::FAIL;
    }
    
    std::ofstream nativeFile (nativeFilename, std::ofstream::trunc);
    if (!nativeFile)
    {
        std::cerr << APP_NAME << ": can not open " << nativeFilename << ": "
                  << std::strerror(errno) << std::endl;
        return Success::FAIL;
    }
    
    int envLineNumber = 0;
    std::string envLine;
    while (std::getline(envFile, envLine))
    {
        envLineNumber++;
        
        size_t dataEnd = envLine.find(';');
        if (dataEnd == std::string::npos)
        {
            std::cerr << APP_NAME ": bad line format in " << envFilename << " at line "
                      << envLineNumber << std::endl;
            continue;
        }
        
        std::string filename = directory + '/' + envLine.substr(dataEnd + 1);
        std::string data = envLine.substr(0, dataEnd);
        
        struct stat fileInfo;
        if (lstat(filename.c_str(), &fileInfo) == -1)
        {
            std::cerr << APP_NAME ": failed to stat " << filename << ": " << std::strerror(errno)
                      << " (" << envFilename << " line " << envLineNumber << ')' << std::endl;
            continue;
        }
        
        nativeFile << "dev=" << std::hex << fileInfo.st_dev
                   << ",ino=" << std::dec << fileInfo.st_ino
                   << ',' << data << '\n';
    }
    
    return Success::OK;
}

std::pair<int, Success> runFakeroot(const std::string& envFilename, bool load, bool save,
                                    const char* const* args, int argCount)
{
    pid_t childPid = fork();
    if (childPid == -1)
    {
        std::perror(APP_NAME ": fork");
        return { -1, Success::FAIL };
    }
    
    if (childPid == 0)
    {
        std::vector<char*> argv;
        
        char fakeroot[] = "fakeroot";
        argv.push_back(fakeroot);
        
        char i[] = "-i";
        if (load)
        {
            argv.push_back(i);
            argv.push_back((char*) envFilename.c_str());
        }
        
        char s[] = "-s";
        if (save)
        {
            argv.push_back(s);
            argv.push_back((char*) envFilename.c_str());
        }
        
        for (int i = 0; i < argCount; i++)
            argv.push_back((char*) args[i]);
        
        argv.push_back(nullptr);
        execvp(fakeroot, argv.data());
        
        std::perror(APP_NAME ": failed to run fakeroot");
        std::exit(255);
    }
    
    int status;
    while (waitpid(childPid, &status, 0) == -1)
    {
        if (errno != EINTR)
        {
            std::perror(APP_NAME ": failed to wait for fakeroot");
            return { -1, Success::FAIL };
        }
    }
    
    if (WIFEXITED(status))
    {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 255)
            return { exitCode, Success::FAIL };
        return { exitCode, Success::OK };
    }
    return { -1, Success::OK };
}

Success convertFromNative(const std::string& nativeFilename, const std::string& directory,
                          const std::string& envFilename)
{
    std::map<std::pair<dev_t, ino_t>, std::string> data;
    Success nativeReadResult;
    std::tie(data, nativeReadResult) = readNativeEnvFile(nativeFilename);
    if (nativeReadResult == Success::FAIL)
        return Success::FAIL;
    
    std::ofstream envFile (envFilename, std::ofstream::trunc);
    if (!envFile)
    {
        std::cerr << APP_NAME ": failed to write " << envFilename << ": " << std::strerror(errno)
                  << std::endl;
        return Success::FAIL;
    }
    
    std::stack<std::string> dirStack;
    dirStack.push(directory);
    while (!dirStack.empty())
    {
        std::string curDir = dirStack.top();
        dirStack.pop();
        
        DIR* dirStream = opendir(curDir.c_str());
        if (!dirStream)
        {
            std::cerr << APP_NAME ": failed to open directory " << curDir << ": "
                      << std::strerror(errno) << std::endl;
            if (curDir == directory)
                return Success::FAIL;
            continue;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dirStream)) != nullptr)
        {
            if (std::strcmp(entry->d_name, "..") == 0
                || (std::strcmp(entry->d_name, ".") == 0 && curDir != directory))
                continue;
            
            std::string filename = curDir + '/' + entry->d_name;
            
            struct stat fileInfo;
            if (lstat(filename.c_str(), &fileInfo) == -1)
            {
                std::cerr << APP_NAME ": failed to stat " << filename << ": "
                            << std::strerror(errno) << "; skipping" << std::endl;
                continue;
            }
            
            auto dataEntry = data.find({ fileInfo.st_dev, fileInfo.st_ino });
            if (dataEntry != data.end())
            {
                envFile << dataEntry->second << ';'
                        << filename.substr(directory.length() + 1) << '\n';
                data.erase(dataEntry);
            }
            
            if ((fileInfo.st_mode & S_IFMT) == S_IFDIR)
                dirStack.push(filename);
        }
        
        closedir(dirStream);
    }
    
    for (const auto& d : data)
    {
        std::cerr << APP_NAME << ": entry (dev=" << std::hex << d.first.first
                  << ",ino=" << std::dec << d.first.second << ") not found in " << directory
                  << "; not preserving" << std::endl;
    }
    
    return Success::OK;
}

std::pair<std::map<std::pair<dev_t, ino_t>, std::string>, Success> readNativeEnvFile(
        const std::string& filename)
{
    std::ifstream file (filename);
    if (!file)
    {
        std::cerr << APP_NAME ": failed to open " << filename << ": " << std::strerror(errno)
                  << std::endl;
        return { {}, Success::FAIL };
    }
    
    std::map<std::pair<dev_t, ino_t>, std::string> data;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        lineNumber++;
        
        static const auto REGEX_FLAGS = std::regex::icase | std::regex::optimize;
        static const std::regex DEV_REGEX ("(?:^|,)dev=([0-9a-f]+)(?:,|$)", REGEX_FLAGS);
        static const std::regex INO_REGEX ("(?:^|,)ino=([0-9]+)(?:,|$)", REGEX_FLAGS);
        
        bool ok = true;
        std::smatch matches;
        
        dev_t dev = -1;
        if (!std::regex_search(line, matches, DEV_REGEX))
            ok = false;
        else
        {
            std::istringstream s (matches[1]);
            s >> std::hex;
            if (!(s >> dev))
                ok = false;
        }
        
        ino_t ino = -1;
        if (!std::regex_search(line, matches, INO_REGEX))
            ok = false;
        else
        {
            std::istringstream s (matches[1]);
            if (!(s >> ino))
                ok = false;
        }
        
        static const auto CLEAR_REGEXES = {
            std::make_pair(DEV_REGEX, ","),
            { INO_REGEX, "," },
            { std::regex("^,", REGEX_FLAGS), "" },
            { std::regex(",$", REGEX_FLAGS), "" }
        };
        
        std::string curData = line;
        for (const auto& r : CLEAR_REGEXES)
            curData = std::regex_replace(curData, r.first, r.second);
        
        if (ok)
            data.insert({ { dev, ino }, curData });
        else
        {
            std::cerr << APP_NAME ": failed to parse line " << lineNumber << " of " << filename
                      << std::endl;
        }
    }
    
    return { data, Success::OK };
}

std::string dirname(const std::string& path)
{
    std::vector<char> buf (path.begin(), path.end());
    buf.push_back('\0');
    return dirname(&buf[0]);
}

std::string basename(const std::string& path)
{
    std::vector<char> buf (path.begin(), path.end());
    buf.push_back('\0');
    return basename(&buf[0]);
}
