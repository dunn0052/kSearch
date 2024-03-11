#include <indexer/inc/DirectoryWatcher.hh>
#include <common/Logger.hh>
#include <common/qcDB.hh>

#include <database/INDEX.hh>

#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <signal.h>
#include <thread>
#include <unordered_map>
#include <vector>

constexpr size_t EVENT_SIZE = sizeof(struct inotify_event);
constexpr size_t BUFFER_SIZE = 1024 * (EVENT_SIZE + 16);

bool g_watchingForChanges = true;

std::unordered_map<int, std::string> g_watcherMap;

static void quitSignal(int sig)
{
    LOG_INFO("Requesting stop");
    g_watchingForChanges = false;
}

static RETCODE MonitorDirectory(qcDB::dbInterface<INDEX>& database, const std::string& directory, int fd, size_t& numWatchers)
{
    RETCODE retcode = RTN_OK;

    int wd = inotify_add_watch(fd, directory.c_str(), IN_CREATE | IN_DELETE | IN_MOVE| IN_ONLYDIR);
    if(0 > wd)
    {
        error_t error = errno;
        if(ENOSPC == error)
        {
            LOG_WARN("Directory: ",
                directory,
                " too large to monitor after ",
                numWatchers,
                " watchers were used. Increase fs.inotify.max_user_watches to add more.");
        }
        else
        {
            LOG_ERROR("Could not add directory to watch list: ",
                directory,
                " to watch list due to error: ",
                strerror(error));
        }

        return RTN_FAIL;
    }

    g_watcherMap[wd] = directory;
    ++numWatchers;

    DIR* directoryToWatch = opendir(directory.c_str());
    if(nullptr == directoryToWatch)
    {
        error_t error = errno;
        LOG_ERROR("Could not open directory: ",
            directory,
            " due to error: ",
            strerror(error));

        return RTN_FAIL;
    }

    std::vector<INDEX> files;

    struct dirent* entry = nullptr;
    while(nullptr != (entry = readdir(directoryToWatch)))
    {
        if(DT_DIR == entry->d_type)
        {
            if('.' != entry->d_name[0])
            {
                retcode |= MonitorDirectory(database, directory + "/" + entry->d_name, fd, numWatchers);
            }
        }
        else if(DT_REG == entry->d_type)
        {
            INDEX file = { 0 };
            strncpy(file.PATH, (directory + "/" + entry->d_name).c_str(), sizeof(file.PATH));
            files.push_back(file);
        }
    }

    if(0 > closedir(directoryToWatch))
    {
        error_t error = errno;
        LOG_WARN("Could not close directory: ",
            directory,
            " due to error ",
            strerror(error));
    }

    retcode |= database.WriteObjects(files);

    return retcode;
}

static RETCODE PollDirectories(qcDB::dbInterface<INDEX>& database, const std::string directory, int fd)
{
    LOG_INFO("Starting to monitor directory: ", directory);

    RETCODE retcode = RTN_OK;
    size_t record = 0;
    char buffer[BUFFER_SIZE] = { 0 };

    while (g_watchingForChanges)
    {
        ssize_t numBytes = read(fd, buffer, BUFFER_SIZE);
        if (numBytes < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        for (char* event = buffer; event < buffer + numBytes;)
        {
            struct inotify_event* notify_event = reinterpret_cast<struct inotify_event*>(event);
            if (IN_CREATE & notify_event->mask)
            {
                std::string filePath = g_watcherMap[notify_event->wd] + "/" + notify_event->name;
                LOG_INFO("File/directory created: ", filePath);

                INDEX newFile = { 0 };
                strcpy(newFile.PATH, filePath.c_str());
                retcode = database.WriteObject(newFile);
                if(RTN_OK == retcode)
                {
                    LOG_DEBUG("Added: ", filePath, " to index database");
                }
                else
                {
                    LOG_WARN("Error adding: ", filePath, " to index database due to error: ", retcode);
                }
            }

            if (IN_DELETE & notify_event->mask)
            {
                std::string filePath = g_watcherMap[notify_event->wd] + "/" + notify_event->name;

                LOG_INFO("File/directory deleted: ", g_watcherMap[notify_event->wd], "/", notify_event->name);

                retcode = database.FindFirstOf
                (
                    [&](const INDEX *file) -> bool
                    {
                        return !strcmp(file->PATH, filePath.c_str());
                    },
                    record
                );

                if(RTN_OK == retcode)
                {
                    retcode = database.DeleteObject(record);
                    if(RTN_OK == retcode)
                    {
                        LOG_DEBUG("Deleted: ", filePath, " from index database");
                    }
                    else
                    {
                        LOG_WARN("Error deleting: ", filePath, " to index database due to error: ", retcode);
                    }
                }
                else
                {
                    LOG_WARN("Error finding: ", filePath, " in index for deletion");
                }

            }

            event += sizeof(struct inotify_event) + notify_event->len;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("Ending monitoring of directory: ", directory);

    return RTN_OK;
}

RETCODE StartWatchingDirectory(const std::string& directory)
{
    signal(SIGQUIT, quitSignal);
    signal(SIGINT, quitSignal);

    std::string databasePath = "/home/osboxes/Documents/Projects/kSearch/database/INDEX.qcdb";
    qcDB::dbInterface<INDEX> database(databasePath);

    LOG_INFO("Reindexing directory: ", directory);
    LOG_DEBUG("Clearing index database: ", databasePath);
    RETCODE retcode = database.Clear();
    if(RTN_OK != retcode)
    {
        LOG_ERROR("Could not clear index database: ", databasePath, " due to error: ", retcode);
        return retcode;
    }
    LOG_DEBUG("Finished clearing index database: ", databasePath);

    int fd = -1;
    size_t numWatchers = 0;
    fd = inotify_init();
    if(0 > fd)
    {
        LOG_ERROR("Failed to initialize inotify");
        return RTN_FAIL;
    }

    retcode = MonitorDirectory(database, directory, fd, numWatchers);
    if(RTN_OK != retcode)
    {
        close(fd);
        LOG_WARN("Error while starting to monitor directory: ", directory);
        return retcode;
    }

    LOG_INFO("Watching: ", numWatchers, " directories");

    if(0 > fcntl(fd, F_SETFL, O_NONBLOCK))
    {
        LOG_WARN("Could not set non-blocking status for watcher fd: ", fd);
        retcode = RTN_FAIL;
    }

    retcode = PollDirectories(database, directory, fd);
    if(RTN_OK != retcode)
    {
        close(fd);
        LOG_WARN("Error polling directory: ", directory);
        return retcode;
    }

    close(fd);

    return retcode;
}