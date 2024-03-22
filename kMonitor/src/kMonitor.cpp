#include <kMonitor/inc/kMonitor.hh>
#include <common/UtilityFunctions.hh>
#include <common/Logger.hh>

#include <locale>
#include <codecvt>

#include <signal.h>

constexpr size_t FILE_CHANGE_BUFFER_SIZE = 4096;

static bool g_IsMonitoring = true;

static void quitSignal(int sig)
{
    LOG_INFO("Requesting monitor stop");
    g_IsMonitoring = false;
}

void CALLBACK DirectoryChangeCallback(DWORD errorCode, DWORD bytesTransferred, LPOVERLAPPED overlapped)
{

    if (ERROR_SUCCESS != errorCode)
    {
        LOG_WARN("Error retrieving notifications due to: ", ErrorString(errorCode));
        return;
    }

    if(nullptr == overlapped)
    {
        return;
    }

    FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(overlapped);

    while (0 != pInfo->NextEntryOffset)
    {
        switch (pInfo->Action)
        {
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

        pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(pInfo) + pInfo->NextEntryOffset);
    }
}

RETCODE MonitorDirectory(const std::string& directory, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase)
{
    LOG_INFO("Monitoring directory: ", directory);
    signal(SIGBREAK, quitSignal);
    signal(SIGINT, quitSignal);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wDirectory =  converter.from_bytes(directory);

    HANDLE hDir = CreateFileW(
        static_cast<LPCWSTR>(wDirectory.c_str()),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);

    if (INVALID_HANDLE_VALUE == hDir) {
        LOG_FATAL("Error opening directory: ",
            directory,
            " due to error: ",
            ErrorString(GetLastError()));

        CloseHandle(hDir);
        return RTN_NOT_FOUND;
    }

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    BYTE buffer[FILE_CHANGE_BUFFER_SIZE] = {0};
    DWORD bytesReturned = 0;

    BOOL result = ReadDirectoryChangesW(
        hDir,
        buffer,
        FILE_CHANGE_BUFFER_SIZE,
        TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
        &bytesReturned,
        &overlapped,
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

    while (g_IsMonitoring)
    {
#if 0
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
#endif
        DWORD dw;
        // Wait for the completion event to be signaled
        DWORD dwWaitResult = WaitForSingleObjectEx(overlapped.hEvent, INFINITE, TRUE);
        if (WAIT_OBJECT_0 == dwWaitResult)
        {
            GetOverlappedResult(hDir, &overlapped, &dw, FALSE);

            FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);

            while (0 != pInfo->NextEntryOffset)
            {
                switch (pInfo->Action)
                {
                case FILE_ACTION_ADDED:
                    std::wcout << L"File added: " << pInfo->FileName << L"\r\n";
                    break;
                case FILE_ACTION_REMOVED:
                    std::wcout << L"File deleted: " << pInfo->FileName << L"\r\n";
                    break;
                default:
                    // Other actions can be handled here
                    break;
                }

                pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(pInfo) + pInfo->NextEntryOffset);
            }

            ResetEvent(overlapped.hEvent);

            BOOL result = ReadDirectoryChangesW(
                hDir,
                buffer,
                FILE_CHANGE_BUFFER_SIZE,
                TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
                &bytesReturned,
                &overlapped,
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
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    CloseHandle(hDir);

    return RTN_OK;
}