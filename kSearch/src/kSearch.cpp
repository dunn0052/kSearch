#include <kSearch/inc/kSearch.hh>

#include <common/qcDB.hh>
#include <common/Logger.hh>

#include <database/INDEX.hh>

#include <regex>

RETCODE SearchPattern(const std::string& pattern, qcDB::dbInterface<DIRECTORYPATH>& directoryDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase)
{
    std::vector<FILENAME> matches;

    std::regex regexPattern(pattern);
    RETCODE retcode = filenameDatabase.FindObjects
    (
        [&](const FILENAME* searchObject) -> bool
        {
            return std::string(searchObject->PATH).find(pattern) != std::string::npos;
            //return std::regex_search(searchObject->PATH, regexPattern);
        },
        matches
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("Error while finding matches: ", retcode);
    }

    size_t lastDirRecord = 0;
    DIRECTORYPATH directory = {0};
    for(FILENAME& match: matches)
    {
        if(match.DIRECTORY == lastDirRecord)
        {
            lastDirRecord = match.DIRECTORY;
            directoryDatabase.ReadObject(match.DIRECTORY, directory);
        }

        std::string fullPath = std::string(directory.PATH) + match.PATH;
        //LOG_INFO(fullPath);
    }

    LOG_INFO("Found: ", matches.size(), " files");

    return retcode;
}