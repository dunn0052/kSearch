#include <searcher/inc/kSearch.hh>

#include <common/qcDB.hh>
#include <common/Logger.hh>

#include <database/INDEX.hh>

#include <regex>

RETCODE SearchPattern(const std::string& pattern)
{
    std::string databasePath = "/home/osboxes/Documents/Projects/kSearch/database/INDEX.qcdb";
    qcDB::dbInterface<INDEX> database(databasePath);

    std::vector<size_t> matchRecords;
    std::vector<INDEX> matchObjects;

    std::regex regexPattern(pattern);
    RETCODE retcode = database.FindObjects
    (
        [&](const INDEX* searchObject) -> bool
        {
            if(std::regex_search(searchObject->PATH, regexPattern))
            {
                matchObjects.push_back(*searchObject);
            }

            return false;
        },
        matchRecords
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("Error while finding matches: ", retcode);
    }

    for(INDEX& match: matchObjects)
    {
        LOG_INFO(match.PATH);
    }

    return retcode;
}