#include <common/CLI.hh>
#include <common/Logger.hh>
#include <kMonitor/inc/kMonitor.hh>

#include <Windows.h>

int main(int argc, char* argv[])
{
    Parser parser("kMonitor", "Monitor for file additions and deletions");
    CLI_StringArgument directoryDatabaseArg("-d", "The path to the DIRECTORYPATH database (DIRECTORYPATH.qcdb)", true);
    CLI_StringArgument filenameDatabaseArg("-f", "The path to the FILENAME database (FILENAME.qcdb)", true);
    CLI_IntArgument waitTimeArg("-w", "Interval to wait for file changes in milliseconds. Use 0 for infinite.", false);

    parser
        .AddArg(directoryDatabaseArg)
        .AddArg(filenameDatabaseArg)
        .AddArg(waitTimeArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    qcDB::dbInterface<DIRECTORYPATH> dirDatabase(directoryDatabaseArg.GetValue());
    qcDB::dbInterface<FILENAME> filenameDatabase(filenameDatabaseArg.GetValue());
    DIRECTORYPATH rootDirectory = {0};
    retcode = dirDatabase.ReadObject(0, rootDirectory);
    if(RTN_OK != retcode)
    {
        LOG_FATAL("Error reading root monitor directory for database: ",
            directoryDatabaseArg.GetValue());
        return RTN_NOT_FOUND;
    }

    size_t waitTime = 0;
    if(waitTimeArg.IsInUse())
    {
        waitTime = waitTimeArg.GetValue();
    }

    retcode = MonitorDirectory(rootDirectory.PATH, dirDatabase, filenameDatabase, waitTime);

    return retcode;

}