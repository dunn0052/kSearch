#include <common/CLI.hh>
#include <common/Logger.hh>

#include <kSearch/inc/kSearch.hh>

int main(int argc, char* argv[])
{
    Parser parser("kSearch", "Search for a file");

    CLI_StringArgument patternArg("-p", "The regex pattern to search", true);
    CLI_StringArgument databaseArg("-d", "The INDEX database to search", true);

    parser
        .AddArg(patternArg)
        .AddArg(databaseArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    qcDB::dbInterface<INDEX> database(databaseArg.GetValue());
    retcode = SearchPattern(patternArg.GetValue(), database);

    return retcode;
}