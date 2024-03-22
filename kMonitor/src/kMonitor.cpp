#include <kMonitor/inc/kMonitor.hh>
#include <common/UtilityFunctions.hh>
#include <common/Logger.hh>

#include <signal.h>

constexpr size_t FILE_CHANGE_BUFFER_SIZE = 4096;

static bool g_IsMonitoring = true;

static void quitSignal(int sig)
{
    LOG_INFO("Requesting monitor stop");
    g_IsMonitoring = false;
}

RETCODE MonitorDirectory(const std::string& directory, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase)
{
    LOG_INFO("Monitoring directory: ", directory);
    signal(SIGBREAK, quitSignal);
    signal(SIGINT, quitSignal);

    HANDLE hDir = CreateFileA(
    static_cast<LPCSTR>(directory.c_str()),
    FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,
    NULL);

    if (INVALID_HANDLE_VALUE == hDir) {
        LOG_FATAL("Error opening directory: ",
            directory,
            " due to error: ",
            ErrorString(GetLastError()));

        CloseHandle(hDir);
        return RTN_NOT_FOUND;
    }

    BYTE buffer[FILE_CHANGE_BUFFER_SIZE] = {0};
    DWORD bytesReturned;

    while (g_IsMonitoring)
    {
        BOOL result = ReadDirectoryChangesW(
            hDir,
            buffer,
            FILE_CHANGE_BUFFER_SIZE,
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            &bytesReturned,
            NULL,
            NULL);

        if (!result)
        {
            LOG_FATAL("Error reading directory changess for: ",
                directory,
                " due to error: ",
                ErrorString(GetLastError()));

            CloseHandle(hDir);
            return RTN_NULL_OBJ;
        }

        FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);

        while (pInfo)
        {
            std::wstring fileName(pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR));

            switch (pInfo->Action) {
            case FILE_ACTION_ADDED:
                LOG_INFO("File added: ", pInfo->FileName);
                break;
            case FILE_ACTION_REMOVED:
                LOG_INFO("File deleted: ", pInfo->FileName);
                break;
            default:
                // Other actions can be handled here
                break;
            }

            if (pInfo->NextEntryOffset == 0) {
                break;
            }

            pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(pInfo) + pInfo->NextEntryOffset);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    CloseHandle(hDir);

    return RTN_OK;
}