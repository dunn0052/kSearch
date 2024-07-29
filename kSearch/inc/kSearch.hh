#ifndef __K_SEARCH
#define __K_SEARCH

#include <common/Retcode.hh>
#include <common/qcDB.hh>
#include <database/DIRECTORYPATH.hh>
#include <database/FILENAME.hh>
#include <string>

RETCODE SearchPattern(const std::string& pattern, qcDB::dbInterface<DIRECTORYPATH>& directoryDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase);

#endif