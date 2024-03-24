#include <kMonitor/inc/kMonitor.hh>
#include <common/UtilityFunctions.hh>
#include <common/Logger.hh>
#include <common/StringConversion.hh>

#include <locale>
#include <codecvt>

#include <signal.h>

constexpr size_t FILE_CHANGE_BUFFER_SIZE = 32 * 1024;

static bool g_IsMonitoring = true;

static void quitSignal(int sig)
{
    LOG_INFO("Requesting monitor stop");
    g_IsMonitoring = false;
}

RETCODE SplitFilePath(const std::string& path, std::string& directory, std::string& fileName)
{
    size_t splitIndex = path.find_last_of('\\');
    if(splitIndex == std::string::npos)
    {
        return RTN_NOT_FOUND;
    }

    directory = path.substr(0, splitIndex);
    fileName = path.substr(splitIndex + 1);
    return RTN_OK;
}

static RETCODE AddFile(const std::string& path, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase)
{
    RETCODE retcode = RTN_OK;
    std::string fileName;
    std::string fileDirectory;

    retcode = SplitFilePath(path, fileDirectory, fileName);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not get path components for path: ", path);
        return retcode;
    }

    size_t record = 0;
    retcode = dirDatabase.FindFirstOf
    (
        [&](const DIRECTORYPATH *directory) -> bool
        {
            return !strcmp(directory->PATH, fileDirectory.c_str());
        },
        record
    );

    if(RTN_NOT_FOUND == retcode)
    {
        DIRECTORYPATH directoryObject = {0};
        errno_t error = strcpy_s(directoryObject.PATH, fileDirectory.c_str());
        if(error)
        {
            LOG_WARN("Could not copy: ",
                fileDirectory,
                " to directory object due to error: ",
                ErrorString((error)));
            return RTN_BAD_ARG;
        }

        retcode = dirDatabase.WriteObject(directoryObject);
        if(RTN_OK != retcode)
        {
            LOG_WARN("Could not write: ",
                fileDirectory,
                " to directory database due to error: ",
                retcode);
            return retcode;
        }

        record = dirDatabase.LastWrittenRecord();

        LOG_INFO("Added new directory: ",
            fileDirectory,
            " to add file: ",
            fileName);
    }

    FILENAME filenameObject = {0};
    errno_t error = strcpy_s(filenameObject.PATH, fileName.c_str());
    if(error)
    {
        LOG_WARN("Could not write: ", fileName, " to file name database");
        return RTN_BAD_ARG;
    }

    filenameObject.DIRECTORY_RECORD = record;
    retcode = filenameDatabase.WriteObject(filenameObject);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not write: ",
            fileName,
            " to file name database due to error: ",
            retcode);
        return retcode;
    }

    return RTN_OK;
}

static RETCODE RemoveFile(const std::string& path, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& fileDatabase)
{
    RETCODE retcode = RTN_OK;
    std::string fileName;
    std::string fileDirectory;

    retcode = SplitFilePath(path, fileDirectory, fileName);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not get path components for path: ", path);
        return retcode;
    }

    size_t dirRecord = 0;
    retcode = dirDatabase.FindFirstOf
    (
        [&](const DIRECTORYPATH *directory) -> bool
        {
            return !strcmp(directory->PATH, fileDirectory.c_str());
        },
        dirRecord
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("Directory: ",
            fileDirectory,
            " not found for deletion due to error: ",
            retcode);
        return retcode;
    }

    size_t fileRecord = 0;
    retcode = fileDatabase.FindFirstOf
    (
        [&](const FILENAME *filename) -> bool
        {
            return !strcmp(filename->PATH, fileName.c_str()) && filename->DIRECTORY_RECORD == dirRecord;
        },
        fileRecord
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("File: ",
            path,
            " with directory record: ",
            dirRecord,
            " not found for deletion due to error: ",
            retcode);
        return retcode;
    }

    retcode = fileDatabase.DeleteObject(fileRecord);
    if(RTN_OK != retcode)
    {
        LOG_WARN("File: ",
            path,
            " could not be deleted due to error: ",
            retcode);
        return retcode;
    }

    return RTN_OK;
}

RETCODE GetRenameRecord(const std::string& path, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& fileDatabase, size_t& record)
{
    RETCODE retcode = RTN_OK;
    std::string fileName;
    std::string fileDirectory;

    record = 0;

    retcode = SplitFilePath(path, fileDirectory, fileName);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not get path components for path: ", path);
        return retcode;
    }

    size_t dirRecord = 0;
    retcode = dirDatabase.FindFirstOf
    (
        [&](const DIRECTORYPATH *directory) -> bool
        {
            return !strcmp(directory->PATH, fileDirectory.c_str());
        },
        dirRecord
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("Directory: ",
            fileDirectory,
            " not found for rename due to error: ",
            retcode);
        return retcode;
    }

    size_t fileRecord = 0;
    retcode = fileDatabase.FindFirstOf
    (
        [&](const FILENAME *filename) -> bool
        {
            return !strcmp(filename->PATH, fileName.c_str()) && filename->DIRECTORY_RECORD == dirRecord;
        },
        fileRecord
    );

    if(RTN_OK != retcode)
    {
        LOG_WARN("File: ",
            path,
            " with directory record: ",
            dirRecord,
            " not found for rename due to error: ",
            retcode);
        return retcode;
    }

    record = fileRecord;

    return RTN_OK;
}

RETCODE RenameRecord(const std::string& path, qcDB::dbInterface<FILENAME>& fileDatabase, size_t record)
{
    RETCODE retcode = RTN_OK;

    std::string fileName;
    std::string fileDirectory;

    retcode = SplitFilePath(path, fileDirectory, fileName);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not get path components for path: ", path);
        return retcode;
    }

    FILENAME fileObject = {0};
    retcode = fileDatabase.ReadObject(record, fileObject);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not read file record: ", record, " for rename: ", path, " due to error: ", retcode);
        return retcode;
    }

    // Ensure that the filename doesn't have an residual parts of the old name
    memset(fileObject.PATH, 0, sizeof(fileObject.PATH));

    errno_t error = strcpy_s(fileObject.PATH, fileName.c_str());
    if(error)
    {
        LOG_WARN("Could not write new file name: ",
            path,
            " to file record: ",
            record,
            " due to error: ",
            ErrorString(error));
        return retcode;
    }

    retcode = fileDatabase.WriteObject(record, fileObject);
    if(RTN_OK != retcode)
    {
        LOG_WARN("Could not read file record: ", record, " for rename: ", path, " due to error: ", retcode);
        return retcode;
    }

    return RTN_OK;
}


RETCODE MonitorDirectory(const std::string& directory, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& filenameDatabase, size_t waitTime)
{
    LOG_INFO("Monitoring directory: ", directory);
    signal(SIGBREAK, quitSignal);
    signal(SIGINT, quitSignal);

    RETCODE retcode = RTN_OK;

    HANDLE hDir = CreateFileA(
        directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);

    if (INVALID_HANDLE_VALUE == hDir)
    {
        LOG_FATAL("Error opening directory: ",
            directory,
            " due to error: ",
            ErrorString(GetLastError()));

        CloseHandle(hDir);
        return RTN_NOT_FOUND;
    }

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    BYTE* buffer = new BYTE[FILE_CHANGE_BUFFER_SIZE];
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

    if(0 == waitTime)
    {
        waitTime = INFINITE;
    }

    size_t renameRecord = 0;
    std::string oldFileName;
    DWORD dw = 0;
    while (g_IsMonitoring)
    {
        DWORD dwWaitResult = WaitForSingleObjectEx(overlapped.hEvent, waitTime, FALSE);
        if (WAIT_OBJECT_0 == dwWaitResult)
        {
            GetOverlappedResult(hDir, &overlapped, &dw, FALSE);

            FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);

            while (pInfo)
            {
                switch (pInfo->Action)
                {
                case FILE_ACTION_ADDED:
                {
                    std::wstring wFileName(pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR));
                    std::string fullPath = directory + "\\" + ConvertToUTF8(wFileName);
                    retcode = AddFile(fullPath, dirDatabase, filenameDatabase);
                    if(RTN_OK == retcode)
                    {
                        LOG_INFO("File added: ", fullPath);
                    }

                    break;
                }
                case FILE_ACTION_REMOVED:
                {
                    std::wstring wFileName(pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR));
                    std::string fullPath = directory + "\\" + ConvertToUTF8(wFileName);

                    retcode = RemoveFile(fullPath, dirDatabase, filenameDatabase);
                    if(RTN_OK == retcode)
                    {
                        LOG_INFO("File deleted: ", fullPath);
                    }

                    break;
                }
                case FILE_ACTION_RENAMED_OLD_NAME:
                {
                    std::wstring wFileName(pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR));
                    oldFileName = directory + "\\" + ConvertToUTF8(wFileName);

                    retcode = GetRenameRecord(oldFileName, dirDatabase, filenameDatabase, renameRecord);
                    if(RTN_OK != retcode)
                    {
                        oldFileName = "";
                    }

                    break;
                }
                case FILE_ACTION_RENAMED_NEW_NAME:
                {
                    if(oldFileName.empty())
                    {
                        break;
                    }

                    std::wstring wFileName(pInfo->FileName, pInfo->FileNameLength / sizeof(WCHAR));
                    std::string filename = ConvertToUTF8(wFileName);

                    retcode = RenameRecord(filename, filenameDatabase, renameRecord);
                    if(RTN_OK == retcode)
                    {
                        LOG_INFO("Renamed: ", oldFileName, " to: ", directory + "\\" + filename, " for record: ", renameRecord);
                    }
                    break;
                }
                default:
                    // Other actions can be handled here
                    break;
                }

                if(0 == pInfo->NextEntryOffset)
                {
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
    }

    CloseHandle(hDir);

    return RTN_OK;
}