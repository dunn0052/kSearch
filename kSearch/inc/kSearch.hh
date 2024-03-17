#ifndef __K_SEARCH
#define __K_SEARCH

#include <common/Retcode.hh>
#include <common/qcDB.hh>
#include <database/INDEX.hh>
#include <string>

RETCODE SearchPattern(const std::string& pattern, qcDB::dbInterface<INDEX>& database);

#endif