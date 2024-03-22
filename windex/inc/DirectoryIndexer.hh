#include <common/qcDB.hh>
#include <database/INDEX.hh>
#include <database/DIRECTORYPATH.hh>
#include <database/FILENAME.hh>
#include <string>

RETCODE IndexDirectory(const std::string& directory, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& fileDatabase ,long long& totalNumFiles);