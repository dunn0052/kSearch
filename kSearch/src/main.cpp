#include <common/CLI.hh>
#include <common/Logger.hh>

#include <kSearch/inc/kSearch.hh>

int main(int argc, char* argv[])
{
    Parser parser("kSearch", "Search for a file");

    CLI_StringArgument patternArg("-p", "The regex pattern to search", true);
    CLI_StringArgument directoryDatabaseArg("-d", "The path to the DIRECTORYPATH database (DIRECTORYPATH.qcdb)");
    CLI_StringArgument filenameDatabaseArg("-f", "The path to the FILENAME database (FILENAME.qcdb)");

    parser
        .AddArg(patternArg)
        .AddArg(directoryDatabaseArg)
        .AddArg(filenameDatabaseArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    qcDB::dbInterface<DIRECTORYPATH> directoryDatabase(directoryDatabaseArg.GetValue());
    qcDB::dbInterface<FILENAME> filenameDatabase(filenameDatabaseArg.GetValue());
    retcode = SearchPattern(patternArg.GetValue(), directoryDatabase, filenameDatabase);

    return retcode;
}