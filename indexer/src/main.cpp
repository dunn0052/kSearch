#include <common_inc/CLI.hh>
#include <common_inc/Logger.hh>
#include <indexer/inc/DirectoryWatcher.hh>

int main(int argc, char* argv[])
{
    CLI::CLI_StringArgument directoryArg("--directory", "The directory to start watching");

    CLI::Parser parser = CLI::Parser("kIndexer",
        "Index and watch for file changes in a given directory")
        .AddArg(directoryArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    if(!directoryArg.IsInUse())
    {
        parser.Usage();
        return retcode;
    }

    std::string directory = directoryArg.GetValue();
    if('/' == directory.back())
    {
        directory.pop_back();
    }

    retcode = StartWatchingDirectory(directory);

    return retcode;
}