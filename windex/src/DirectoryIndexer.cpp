#include <windex/inc/DirectoryIndexer.hh>
#include <common/Logger.hh>

RETCODE IndexDirectory(const std::string& directory, qcDB::dbInterface<INDEX>& database, long long& totalNumFiles)
{
    WIN32_FIND_DATAA findData = { 0 };
    std::string searchPath = directory + "\\*.*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    std::vector<INDEX> files;

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
                if(RTN_OK != IndexDirectory(subDirectory, database, totalNumFiles));
                {
                    LOG_WARN("Error indexing directory: ", subDirectory);
                }
                continue;
            }

            INDEX fileEntry = {0};
            std::string fullPath = directory + '\\' + findData.cFileName;
            errno_t error =  strncpy_s(fileEntry.PATH, fullPath.c_str(), fullPath.length());
            if(error)
            {
                LOG_WARN("Error copying file path to database entry: ", findData.cFileName);
            }

            files.push_back(fileEntry);
            totalNumFiles++;


        } while (FindNextFileA(hFind, &findData) != 0);

        FindClose(hFind);
    }
    else
    {
        LOG_WARN("Could not find files in directory: ",
            searchPath,
            " due to: ",
            ErrorString(GetLastError()));
        return RTN_NOT_FOUND;
    }

    database.WriteObjects(files);

    std::cout << "\r" << totalNumFiles;

    return RTN_OK;
}