class dbInterface
{
public:
    dbInterface(const std::string& dbPath, size_t windowSize) :
        m_IsOpen(false), m_Size(0), m_DBAddress(nullptr), m_NumRecords(0), m_WindowSize(windowSize), m_CurrentOffset(0)
    {
        int fd = open(dbPath.c_str(), O_RDWR);
        if (INVALID_FD > fd)
        {
            return;
        }

        struct stat statbuf;
        int error = fstat(fd, &statbuf);
        if (0 > error)
        {
            close(fd);
            return;
        }

        m_Size = statbuf.st_size;
        m_NumRecords = m_Size / sizeof(object);

        // Map the initial window
        MapWindow(fd, 0);

        close(fd);
        m_IsOpen = true;
    }

    ~dbInterface(void)
    {
        if (m_DBAddress != nullptr)
        {
            munmap(m_DBAddress, m_WindowSize);
        }
    }

    char* Get(const size_t record)
    {
        if (record >= m_NumRecords)
        {
            return nullptr;
        }

        size_t byteIndex = sizeof(DBHeader) + sizeof(object) * record;

        // Check if the record is outside the current mapped window
        if (byteIndex < m_CurrentOffset || byteIndex >= m_CurrentOffset + m_WindowSize)
        {
            size_t newOffset = (byteIndex / m_WindowSize) * m_WindowSize;
            RemapWindow(newOffset);
        }

        return m_DBAddress + (byteIndex - m_CurrentOffset);
    }

private:
    void MapWindow(int fd, size_t offset)
    {
        m_DBAddress = static_cast<char*>(mmap(nullptr, m_WindowSize,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset));
        if (MAP_FAILED == m_DBAddress)
        {
            m_DBAddress = nullptr;
            return;
        }

        m_CurrentOffset = offset;
    }

    void RemapWindow(size_t newOffset)
    {
        if (m_DBAddress != nullptr)
        {
            munmap(m_DBAddress, m_WindowSize);
        }

        int fd = open(m_DBPath.c_str(), O_RDWR);
        if (INVALID_FD > fd)
        {
            return;
        }

        MapWindow(fd, newOffset);
        close(fd);
    }

    bool m_IsOpen;
    size_t m_Size;
    size_t m_NumRecords;
    size_t m_WindowSize;       // Size of the mapped window
    size_t m_CurrentOffset;    // Current offset of the mapped window
    char* m_DBAddress;
    std::string m_DBPath;
};

bool ResizeDatabase(size_t newNumRecords)
{
    size_t newSize = sizeof(DBHeader) + newNumRecords * sizeof(object);

    // Resize the file
    if (ftruncate(m_FileDescriptor, newSize) != 0)
    {
        std::cerr << "Failed to resize the database file." << std::endl;
        return false;
    }

    // Unmap the current memory region
    if (munmap(m_DBAddress, m_Size) != 0)
    {
        std::cerr << "Failed to unmap the database memory." << std::endl;
        return false;
    }

    // Remap the file with the new size
    m_DBAddress = static_cast<char*>(mmap(nullptr, newSize,
        PROT_READ | PROT_WRITE, MAP_SHARED, m_FileDescriptor, 0));
    if (m_DBAddress == MAP_FAILED)
    {
        std::cerr << "Failed to remap the database memory." << std::endl;
        return false;
    }

    // Update metadata
    m_Size = newSize;
    reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords = newNumRecords;

    return true;
}

bool ResizeDatabase(size_t newNumRecords)
{
    size_t newSize = sizeof(DBHeader) + newNumRecords * sizeof(object);

    // Resize the file
    LARGE_INTEGER newFileSize;
    newFileSize.QuadPart = newSize;

    HANDLE hFile = CreateFileA(
        m_DBPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open the database file for resizing." << std::endl;
        return false;
    }

    if (!SetFilePointerEx(hFile, newFileSize, NULL, FILE_BEGIN) ||
        !SetEndOfFile(hFile))
    {
        std::cerr << "Failed to resize the database file." << std::endl;
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);

    // Unmap the current memory region
    if (!UnmapViewOfFile(m_DBAddress))
    {
        std::cerr << "Failed to unmap the database memory." << std::endl;
        return false;
    }

    // Remap the file with the new size
    HANDLE hMapFile = CreateFileMappingA(
        hFile, NULL, PAGE_READWRITE, 0, newSize, NULL);
    if (hMapFile == NULL)
    {
        std::cerr << "Failed to create file mapping." << std::endl;
        return false;
    }

    m_DBAddress = static_cast<char*>(MapViewOfFile(
        hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, newSize));
    if (m_DBAddress == nullptr)
    {
        std::cerr << "Failed to remap the database memory." << std::endl;
        CloseHandle(hMapFile);
        return false;
    }

    CloseHandle(hMapFile);

    // Update metadata
    m_Size = newSize;
    reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords = newNumRecords;

    return true;
}

RETCODE WriteObject(size_t record, object& objectWrite)
{
    if (record >= reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords)
    {
        // Resize the database to accommodate the new record
        if (!ResizeDatabase(record + 1))
        {
            return RTN_RESIZE_FAIL;
        }
    }

    // Proceed with writing the object
    char* p_object = Get(record);
    if (nullptr == p_object)
    {
        return RTN_NULL_OBJ;
    }

    RETCODE retcode = LockDB();
    if (RTN_OK != retcode)
    {
        return retcode;
    }

    memcpy(p_object, &objectWrite, sizeof(object));

    retcode = UnlockDB();
    if (RTN_OK != retcode)
    {
        return retcode;
    }

    return RTN_OK;
}

bool ResizeDatabaseProactively()
{
    // Double the current number of records
    size_t currentNumRecords = reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords;
    size_t newNumRecords = currentNumRecords * 2;

    // Calculate the new size
    size_t newSize = sizeof(DBHeader) + newNumRecords * sizeof(object);

    // Resize the file
#ifdef WINDOWS_PLATFORM
    HANDLE hFile = CreateFileA(
        m_DBPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open the database file for resizing." << std::endl;
        return false;
    }

    LARGE_INTEGER newFileSize;
    newFileSize.QuadPart = newSize;

    if (!SetFilePointerEx(hFile, newFileSize, NULL, FILE_BEGIN) || !SetEndOfFile(hFile))
    {
        std::cerr << "Failed to resize the database file." << std::endl;
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);

    // Unmap and remap the memory
    if (!UnmapViewOfFile(m_DBAddress))
    {
        std::cerr << "Failed to unmap the database memory." << std::endl;
        return false;
    }

    HANDLE hMapFile = CreateFileMappingA(
        hFile, NULL, PAGE_READWRITE, 0, newSize, NULL);
    if (hMapFile == NULL)
    {
        std::cerr << "Failed to create file mapping." << std::endl;
        return false;
    }

    m_DBAddress = static_cast<char*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, newSize));
    CloseHandle(hMapFile);

    if (nullptr == m_DBAddress)
    {
        std::cerr << "Failed to remap the database memory." << std::endl;
        return false;
    }
#else
    if (ftruncate(m_FileDescriptor, newSize) != 0)
    {
        std::cerr << "Failed to resize the database file." << std::endl;
        return false;
    }

    if (munmap(m_DBAddress, m_Size) != 0)
    {
        std::cerr << "Failed to unmap the database memory." << std::endl;
        return false;
    }

    m_DBAddress = static_cast<char*>(mmap(nullptr, newSize,
        PROT_READ | PROT_WRITE, MAP_SHARED, m_FileDescriptor, 0));
    if (m_DBAddress == MAP_FAILED)
    {
        std::cerr << "Failed to remap the database memory." << std::endl;
        return false;
    }
#endif

    // Update metadata
    m_Size = newSize;
    reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords = newNumRecords;

    return true;
}

RETCODE WriteObject(size_t record, object& objectWrite)
{
    if (record >= reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords)
    {
        // Proactively resize the database
        if (!ResizeDatabaseProactively())
        {
            return RTN_RESIZE_FAIL;
        }
    }

    // Proceed with writing the object
    char* p_object = Get(record);
    if (nullptr == p_object)
    {
        return RTN_NULL_OBJ;
    }

    RETCODE retcode = LockDB();
    if (RTN_OK != retcode)
    {
        return retcode;
    }

    memcpy(p_object, &objectWrite, sizeof(object));

    retcode = UnlockDB();
    if (RTN_OK != retcode)
    {
        return retcode;
    }

    return RTN_OK;
}