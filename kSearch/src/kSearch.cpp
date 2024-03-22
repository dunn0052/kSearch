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
            if(std::string(searchObject->PATH).find(pattern) != std::string::npos)
            {
                DIRECTORYPATH directory = {0};
                directoryDatabase.ReadObject(searchObject->DIRECTORY, directory);
                std::string fullPath = std::string(directory.PATH) + searchObject->PATH;
                LOG_INFO(fullPath);

                return true;
            }

            return false;
            //return std::regex_search(searchObject->PATH, regexPattern);
        },
        matches
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("Error while finding matches: ", retcode);
    }

#if 0
    for(FILENAME& match: matches)
    {
        DIRECTORYPATH directory = {0};
        directoryDatabase.ReadObject(match.DIRECTORY, directory);
        std::string fullPath = std::string(directory.PATH) + match.PATH;
        LOG_INFO(fullPath);
    }
#endif
    LOG_INFO("Found: ", matches.size(), " files");

    return retcode;
}