#include <searcher/inc/kSearch.hh>

#include <common/qcDB.hh>
#include <common/Logger.hh>

#include <database/INDEX.hh>

#include <regex>

RETCODE SearchPattern(const std::string& pattern)
{
    std::string databasePath = "/home/osboxes/Documents/Projects/kSearch/database/INDEX.qcdb";
    qcDB::dbInterface<INDEX> database(databasePath);

    std::vector<INDEX> matches;

    std::regex regexPattern(pattern);
    RETCODE retcode = database.FindObjects
    (
        [&](const INDEX* searchObject) -> bool
        {
            //return std::string(searchObject->PATH).find(pattern) != std::string::npos;
            return std::regex_search(searchObject->PATH, regexPattern);
        },
        matches
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("Error while finding matches: ", retcode);
    }

    for(INDEX& match: matches)
    {
        LOG_INFO(match.PATH);
    }

    LOG_INFO("Found: ", matches.size(), " records");

    return retcode;
}