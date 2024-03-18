#include <common/CLI.hh>
#include <common/Logger.hh>
#include <windex/inc/DirectoryIndexer.hh>

int main(int argc, char* argv[])
{
    CLI_StringArgument directoryArg("--directory", "The directory to start watching", true);
    CLI_StringArgument databaseArg("-d", "The path to the INDEX database", true);

    Parser parser = Parser("kIndexer",
        "Index and watch for file changes in a given directory")
        .AddArg(directoryArg)
        .AddArg(databaseArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if (RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    if (!directoryArg.IsInUse())
    {
        parser.Usage();
        return retcode;
    }

    std::string directory = directoryArg.GetValue();
    qcDB::dbInterface<INDEX> database(databaseArg.GetValue());

    LOG_INFO("Clearning database: ", databaseArg.GetValue());
    database.Clear();
    LOG_INFO("Done clearing database: ", databaseArg.GetValue());

    LOG_INFO("Indexing: ", directoryArg.GetValue());

    // We add another \ in IndexDirectory() so make the printouts look pretty
    if('\\' == directory.back())
    {
        directory.pop_back();
    }

    long long totalNumFiles = 0;

    retcode = IndexDirectory(directory, database, totalNumFiles);
    std::cout << "\n";
    LOG_INFO("Completed: ", directoryArg.GetValue(), " with ", totalNumFiles, " files");

    return retcode;
}