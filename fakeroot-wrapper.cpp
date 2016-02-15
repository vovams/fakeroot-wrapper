#include <iostream>
#include <string>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <libgen.h>

#define APP_NAME    "fakeroot-wrapper"
#define APP_VERSION "0.1"

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
    
    std::cout << "dir = " << directoryPath << '\n'
              << "env = " << envFilename << '\n'
              << "upd = " << updateEnvFile << '\n'
              << "cnv = " << convertInput << '\n'
              << "pss = " << ( *fakerootArgs ?: "(null)" ) << " (" << fakerootArgCount << ")\n"
              << std::flush;
    
    return 0;
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

std::string dirname(const std::string& path)
{
    std::vector<char> buf (path.begin(), path.end());
    return dirname(&buf[0]);
}

std::string basename(const std::string& path)
{
    std::vector<char> buf (path.begin(), path.end());
    return basename(&buf[0]);
}
