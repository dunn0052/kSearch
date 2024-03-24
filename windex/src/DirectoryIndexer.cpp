#include <windex/inc/DirectoryIndexer.hh>
#include <common/Logger.hh>

RETCODE IndexDirectory(const std::string& directory, qcDB::dbInterface<DIRECTORYPATH>& dirDatabase, qcDB::dbInterface<FILENAME>& fileDatabase ,long long& totalNumFiles)
{
    RETCODE retcode = RTN_OK;
    WIN32_FIND_DATAA findData = { 0 };
    std::string searchPath = directory + "\\*.*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    std::vector<FILENAME> files;

    DIRECTORYPATH directoryObject = {0};
    errno_t error =  strncpy_s(directoryObject.PATH, directory.c_str(), sizeof(directoryObject.PATH));
    if(error)
    {
        LOG_WARN("Error copying file path to database entry: ", findData.cFileName);
        return retcode;
    }
    retcode = dirDatabase.WriteObject(directoryObject);
    size_t directoryRecord = dirDatabase.LastWrittenRecord();

    if (INVALID_HANDLE_VALUE != hFind)
    {
        do {
            // Skip "." and ".." files and directories
            if('.' == findData.cFileName[0])
            {
                continue;
            }

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                std::string subDirectory = directory + '\\' + findData.cFileName;
                IndexDirectory(subDirectory, dirDatabase, fileDatabase, totalNumFiles);
                continue;
            }

            FILENAME fileEntry = {0};
            errno_t error =  strncpy_s(fileEntry.PATH, findData.cFileName, sizeof(fileEntry.PATH));
            if(error)
            {
                LOG_WARN("Error copying file path to database entry: ", findData.cFileName);
                return retcode;
            }
            fileEntry.DIRECTORY_RECORD = directoryRecord;

            files.push_back(fileEntry);
            totalNumFiles++;

        } while (FindNextFileA(hFind, &findData) != 0);

        FindClose(hFind);
    }
    else
    {
        #if 0
        LOG_WARN("Could not find files in directory: ",
            searchPath,
            " due to: ",
            ErrorString(GetLastError()));
        #endif
    }

    if(!files.empty())
    {
        retcode = fileDatabase.WriteObjects(files);
        if(RTN_OK != retcode)
        {
            LOG_WARN("Could not write files for directory: ", directory);
            return retcode;
        }
        std::cout << "\rFiles indexed: " << totalNumFiles;
    }

    return RTN_OK;
}