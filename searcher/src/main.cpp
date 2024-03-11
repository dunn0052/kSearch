#include <common/CLI.hh>
#include <common/Logger.hh>

#include <searcher/inc/kSearch.hh>

int main(int argc, char* argv[])
{
    CLI::Parser parser("kSearch", "Search for a file");

    CLI::CLI_StringArgument patternArg("-p", "The regex pattern to search");

    parser.AddArg(patternArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    retcode = SearchPattern(patternArg.GetValue());

    return retcode;
}