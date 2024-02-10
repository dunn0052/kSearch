#ifndef __DIRECTORY_WATCHER_HH
#define __DIRECTORY_WATCHER_HH

#include <common_inc/retcode.hh>
#include <string>

RETCODE StartWatchingDirectory(const std::string& directory);

#endif