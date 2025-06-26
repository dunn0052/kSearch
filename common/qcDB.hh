#ifndef __QC_DB_HH
#define __QC_DB_HH

#include <common/OSdefines.hh>

#include <string>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef WINDOWS_PLATFORM
#include <Windows.h>
#else
#include <sys/mman.h>
#endif
#include <iostream>
#include <algorithm>
#include <functional>
#include <thread>

#include <common/Retcode.hh>
#include <common/DBHeader.hh>

namespace qcDB
{
    template <class object>
    class dbInterface
    {

public:

        RETCODE ReadObject(size_t record, object& out_object)
        {
            RETCODE retcode = RTN_OK;
            char* p_object = Get(record);
            if(nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            memcpy(&out_object, p_object, sizeof(object));

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        RETCODE ReadObjects(std::vector<std::tuple<size_t, object>>& objects)
        {
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& a, const std::tuple<size_t, object>& b) {
                return std::get<0>(a) < std::get<0>(b);
                });

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }


            for(std::tuple<size_t, object> readObject : objects)
            {
                char* p_object = Get(std::get<0>(readObject));
                if(nullptr == p_object)
                {
                    return RTN_NULL_OBJ;
                }
                memcpy(&std::get<1>(readObject), p_object, sizeof(readObject));
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        RETCODE WriteObject(size_t record, object& objectWrite)
        {
            RETCODE retcode = RTN_OK;
            char* p_object = Get(record);
            if(nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
            if (reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size < record)
            {
                reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size = record;
            }

            memcpy(p_object, &objectWrite, sizeof(object));

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        RETCODE WriteObject(object& objectWrite)
        {
            RETCODE retcode = RTN_OK;
            object emptyObject = { 0 };
            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            size_t record = reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten;
            object* currentObject = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader) + record * sizeof(object));
            for (; record < NumberOfRecords(); record++)
            {
                if (std::memcmp(currentObject, &emptyObject, sizeof(object)) == 0)
                {
                    reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
                    memcpy(currentObject, &objectWrite, sizeof(object));

                    if (reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size < record)
                    {
                        reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size = record;
                    }

                    break;
                }

                currentObject++;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            if(NumberOfRecords() == record)
            {
                return RTN_NOT_FOUND;
            }

            return RTN_OK;
        }

        RETCODE WriteObjects(std::vector<std::tuple<size_t, object>>& objects)
        {

            RETCODE retcode = RTN_OK;
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& a, const std::tuple<size_t, object>& b) {
                return std::get<0>(a) < std::get<0>(b);
                });

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            for(std::tuple<size_t, object> writeObject : objects)
            {
                char* p_object = Get(std::get<0>(writeObject));
                if(nullptr == p_object)
                {
                    return RTN_NULL_OBJ;
                }
                memcpy(p_object, &std::get<1>(writeObject), sizeof(writeObject));
            }

            reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = std::get<0>(objects.back());

            if (reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size < std::get<0>(objects.back()))
            {
                reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size = std::get<0>(objects.back());
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        RETCODE WriteObjects(std::vector<object>& objects)
        {
            RETCODE retcode = RTN_OK;
            const object emptyObject = { 0 };
            auto objectsIterator = objects.begin();

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            size_t record = reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten;
            object* currentObject = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader) + record * sizeof(object));
            for (; record < NumberOfRecords(); record++)
            {
                if (std::memcmp(currentObject, &emptyObject, sizeof(object)) == 0)
                {
                    if (objectsIterator == objects.end())
                    {
                        reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
                        break;
                    }

                    memcpy(currentObject, &(*objectsIterator), sizeof(object));

                    ++objectsIterator;
                }

                currentObject++;
            }

            if (reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size < record)
            {
                reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size = record;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            if (objectsIterator != objects.end())
            {
                return RTN_EOF;
            }

            return RTN_OK;
        }

        RETCODE DeleteObject(size_t record)
        {
            RETCODE retcode = RTN_OK;
            object deletedObject = { 0 };
            char* p_object = Get(record);
            if (nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }
            size_t size = reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size;

            memset(p_object, 0, sizeof(object));

            // If the deleted record was the last one, find the next last record
            if (size == record)
            {
                for (; p_object != m_DBAddress; p_object -= sizeof(object))
                {
                    --record;
                    if (std::memcmp(p_object, &deletedObject, sizeof(object)) != 0)
                    {
                        break;
                    }
                }

                reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size = record;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        RETCODE Clear(void)
        {
            RETCODE retcode = RTN_OK;
            if (m_IsOpen)
            {
                retcode = LockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

                size_t dbSize = reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords * sizeof(object);
                object* start = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader));

                memset(start, 0, dbSize);

                reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = 0;
                reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size = 0;

                retcode = UnlockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

                return RTN_OK;
            }

            return RTN_MALLOC_FAIL;
        }

        using Predicate = std::function<bool(const object* currentObject)>;

        RETCODE FindFirstOf(Predicate predicate, size_t& out_Record)
        {
            RETCODE retcode = RTN_OK;
            bool found = false;
            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            size_t size = reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size;
            size_t record = 0;
            for (record = 0; record < size; record++)
            {
                if (predicate(currentObject))
                {
                    out_Record = record;
                    found = true;
                    break;
                }

                currentObject++;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            if(!found)
            {
                return RTN_NOT_FOUND;
            }

            return RTN_OK;
        }

        static void FinderThread(Predicate predicate, const object* currentObject, size_t numRecords, std::vector<object>& results)
        {
            for (size_t record = 0; record < numRecords; record++)
            {
                if (predicate(++currentObject))
                {
                    results.push_back(*currentObject);
                }
            }
        }

        RETCODE FindObjects(Predicate predicate, std::vector<object>& out_MatchingObjects)
        {
            RETCODE retcode = RTN_OK;
            // Number of threads /2 so we don't completely lock up the CPU
#ifdef WINDOWS_PLATFORM
            size_t numThreads = std::thread::hardware_concurrency() / 2;
#else
            size_t numThreads = sysconf(_SC_NPROCESSORS_ONLN) / 2;
#endif
            std::vector<std::thread> threads;
            std::vector<std::vector<object>> results(numThreads);

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            size_t size = reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size;
            size_t segmentSize = size / numThreads;

            for (size_t threadIndex = 0; threadIndex < numThreads - 1; threadIndex++)
            {
                threads.emplace_back(FinderThread, predicate, currentObject, segmentSize, std::ref(results[threadIndex]));
                currentObject += segmentSize;
            }

            threads.emplace_back(FinderThread, predicate, currentObject, size - ((numThreads - 1) * segmentSize), std::ref(results[numThreads - 1]));

            for (std::thread& thread : threads)
            {
                thread.join();
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            for (std::vector<object>& matches : results)
            {
                for (object& match : matches)
                {
                    out_MatchingObjects.push_back(match);
                }
            }

            return RTN_OK;
        }

        inline size_t NumberOfRecords(void)
        {
            if (m_IsOpen)
            {
                return m_NumRecords;
            }

            return 0;
        }

        inline size_t LastWrittenRecord(void)
        {
            RETCODE retcode = RTN_OK;
            size_t lastWittenRecord = 0;

            if (m_IsOpen)
            {
                retcode = LockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

                lastWittenRecord = reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten;

                retcode = UnlockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

            }

            return lastWittenRecord;
        }

        dbInterface(const std::string& dbPath) :
            m_IsOpen(false), m_Size(0),
            m_DBAddress(nullptr), m_NumRecords(0),
            m_DBFileDescriptor(INVALID_FD)
#ifdef WINDOWS_PLATFORM
            , m_DBFileHandle(INVALID_HANDLE_VALUE)
            , m_Mutex(INVALID_HANDLE_VALUE)
#endif
        {
#ifdef WINDOWS_PLATFORM
            m_DBFileHandle = CreateFileA(
                static_cast<LPCSTR>(dbPath.c_str()), // File name
                GENERIC_READ | GENERIC_WRITE,        // Access mode
                FILE_SHARE_READ | FILE_SHARE_WRITE,  // Share mode
                NULL,                                // Security attributes
                OPEN_EXISTING,                         // How to create
                FILE_ATTRIBUTE_NORMAL,               // File attributes
                NULL                                 // Handle to template file
            );

            if (INVALID_HANDLE_VALUE == m_DBFileHandle)
            {
                CloseHandle(m_DBFileHandle);
                return;
            }

            // Get the file size
            m_Size = static_cast<size_t>(GetFileSize(hFile, NULL));
            if (m_Size == INVALID_FILE_SIZE) {
                CloseHandle(m_DBFileHandle);
                return;
            }

            HANDLE hMapFile = CreateFileMappingA(
                m_DBFileHandle,                          // File handle
                NULL,                           // Security attributes
                PAGE_READWRITE,                 // Protection
                0,                              // High-order 32 bits of file size
                0,                              // Low-order 32 bits of file size
                NULL                            // Name of file-mapping object
            );

            if (hMapFile == NULL) {
                CloseHandle(m_DBFileHandle);
                return;
            }

            // Map the file to memory
            m_DBAddress = static_cast<char*>(MapViewOfFile(
                hMapFile,                       // Handle to file mapping object
                FILE_MAP_ALL_ACCESS,            // Access mode
                0,                              // High-order 32 bits of file offset
                0,                              // Low-order 32 bits of file offset
                0                               // Number of bytes to map (0 for all)
            ));

            // Close the file mapping handle
            CloseHandle(hMapFile);

            if (nullptr == m_DBAddress)
            {
                return;
            }
#else
            m_DBFileDescriptor = open(dbPath.c_str(), O_RDWR);
            if(INVALID_FD > m_DBFileDescriptor)
            {
                return;
            }

            struct stat statbuf;
            int error = fstat(m_DBFileDescriptor, &statbuf);
            if(0 > error)
            {
                close(m_DBFileDescriptor);
                return;
            }

            m_Size = statbuf.st_size;
            m_DBAddress = static_cast<char*>(mmap(nullptr, m_Size,
                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                    m_DBFileDescriptor, 0));
            if(MAP_FAILED == m_DBAddress)
            {
                close(m_DBFileDescriptor);
                return;
            }

#endif
            m_NumRecords = reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords;

            m_IsOpen = true;
        }

        ~dbInterface(void)
        {
#ifdef WINDOWS_PLATFORM
            // Unmap the file view
            BOOL error = UnmapViewOfFile(m_DBAddress);

            if (FALSE == error)
            {
                // Nothing much you can do in this case..
                m_IsOpen = false;
            }

            if(INVALID_HANDLE_VALUE != m_DBFileHandle)
            {
                CloseHandle(m_DBFileHandle);
            }
#else
            int error = munmap(m_DBAddress, m_Size);
            if(0 == error)
            {
                // Nothing much you can do in this case..
                m_IsOpen = false;
            }

            if(0 <= m_DBFileDescriptor)
            {
                close(m_DBFileDescriptor);
            }
#endif
        }

protected:

    RETCODE LockDB(void)
    {
#ifdef WINDOWS_PLATFORM
        m_Mutex = CreateMutexA(NULL, FALSE, "MutexForFileLock");
        if (nullptr == m_Mutex)
        {
            return RTN_LOCK_ERROR;
        }
#else

        int lockError = pthread_rwlock_rdlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
        if (0 != lockError)
        {
            return RTN_LOCK_ERROR;
        }
#endif

        return RTN_OK;
    }

    RETCODE UnlockDB(void)
    {
#ifdef WINDOWS_PLATFORM
        if (ReleaseMutex(m_Mutex))
        {
            return RTN_LOCK_ERROR;
        }
#else
        lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
        if (0 != lockError)
        {
            return RTN_LOCK_ERROR;
        }
#endif

    return RTN_OK;
    }
    char* Get(const size_t record)
    {
        if(NumberOfRecords() - 1 < record)
        {
            return nullptr;
        }

        if(!m_IsOpen || nullptr == m_DBAddress)
        {
            return nullptr;
        }

        // Record off by 1 adjustment
        size_t byte_index = sizeof(DBHeader) + sizeof(object) * record;
        if(m_Size < byte_index)
        {
            return nullptr;
        }

        return m_DBAddress + byte_index;
    }

    bool m_IsOpen;
    size_t m_Size;
    size_t m_NumRecords;
    char* m_DBAddress;
    int m_DBFileDescriptor;
#ifdef WINDOWS_PLATFORM
    HANDLE m_DBFileHandle;
    HANDLE m_Mutex;
#else
#endif

    static constexpr int INVALID_FD = 0;

    };
}

#endif