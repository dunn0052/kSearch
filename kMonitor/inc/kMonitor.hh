#include <common/qcDB.hh>
#include <database/DIRECTORYPATH.hh>
#include <database/FILENAME.hh>

RETCODE MonitorDirectory(const std::string& directory, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase, size_t waitTime);