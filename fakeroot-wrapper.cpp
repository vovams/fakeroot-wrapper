#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <tuple>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <libgen.h>
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

static std::pair<int, Success> runFakeroot(const std::string& envFilename, bool save,
                                           const char* const* args, int argCount);
static Success convertToNative(const std::string& envFilename, const std::string& directory,
                               const std::string nativeFilename);
static void printHelp();

static std::string dirname(const std::string& path);
static std::string basename(const std::string& path);

int main(int argc, char** argv)
{
    std::string directoryPath = ".";
    std::string envFilename = ".fakerootenv";
    bool envFilenameSet = false;
    bool updateEnvFile = true;
    bool convertInput = true;
    
    while (1)
    {
        switch (getopt(argc, argv, "+d:f:hri"))
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
    
    if (convertInput)
    {
        if (convertToNative(envFilename, directoryPath, nativeEnvFilename) != Success::OK)
            return 2;
    }
    
    int exitCode;
    Success fakerootSuccess;
    std::tie(exitCode, fakerootSuccess) = runFakeroot(nativeEnvFilename, updateEnvFile,
                                                      fakerootArgs, fakerootArgCount);
    
    if (fakerootSuccess == Success::FAIL)
        exitCode = 3;
    
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
                 "    Read only. Do not save changes to file attributes.\n"
                 "\n"
                 "-i\n"
                 "    Import. File specified with -f (or the defautl file) is a file generated\n"
                 "    with fakeroot's -s option. It will be passed to fakeroot without changes\n"
                 "    and overwritten when fakeroot exits.\n"
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
                        const std::string nativeFilename)
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
        if (stat(filename.c_str(), &fileInfo) == -1)
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

std::pair<int, Success> runFakeroot(const std::string& envFilename, bool save,
                                    const char* const* args, int argCount)
{
    pid_t childPid = fork();
    if (childPid == -1)
    {
        std::perror(APP_NAME ": fork");
        return std::make_pair(-1, Success::FAIL);
    }
    
    if (childPid == 0)
    {
        std::vector<char*> argv;
        
        char fakeroot[] = "fakeroot";
        argv.push_back(fakeroot);
        
        char i[] = "-i";
        argv.push_back(i);
        argv.push_back((char*) envFilename.c_str());
        
        if (save)
        {
            char s[] = "-s";
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
            return std::make_pair(-1, Success::FAIL);
        }
    }
    
    if (WIFEXITED(status))
    {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 255)
            return std::make_pair(exitCode, Success::FAIL);
        return std::make_pair(exitCode, Success::OK);
    }
    return std::make_pair(-1, Success::OK);
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
