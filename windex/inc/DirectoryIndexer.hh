#include <common/qcDB.hh>
#include <database/INDEX.hh>
#include <string>

RETCODE IndexDirectory(const std::string& directory, qcDB::dbInterface<INDEX>& database, long long& totalNumFiles);