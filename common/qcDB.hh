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
#include <cstring>

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
            char* p_object = Get(record);
            if(nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            int lockError = pthread_rwlock_rdlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            memcpy(&out_object, p_object, sizeof(object));

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }

        RETCODE ReadObjects(std::vector<std::tuple<size_t, object>>& objects)
        {
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& a, const std::tuple<size_t, object>& b) {
                return std::get<0>(a) < std::get<0>(b);
                });

            int lockError = pthread_rwlock_rdlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
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

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }

        RETCODE WriteObject(size_t record, object& objectWrite)
        {
            char* p_object = Get(record);
            if(nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            int lockError = pthread_rwlock_wrlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
            memcpy(p_object, &objectWrite, sizeof(object));

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }


        RETCODE WriteObject(object& objectWrite)
        {

            object emptyObject = { 0 };
            int lockError = pthread_rwlock_wrlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            object* currentObject = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader));
            for(size_t record = 0; record < NumberOfRecords(); record++)
            {
                if(std::memcmp(currentObject, &emptyObject, sizeof(object)) == 0)
                {
                    reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
                    memcpy(currentObject, &objectWrite, sizeof(object));
                }

                currentObject++;
            }

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }

        RETCODE WriteObjects(std::vector<std::tuple<size_t, object>>& objects)
        {
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& a, const std::tuple<size_t, object>& b) {
                return std::get<0>(a) < std::get<0>(b);
                });

            int lockError = pthread_rwlock_wrlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
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

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }

        RETCODE WriteObjects(std::vector<object>& objects)
        {
            const object emptyObject = { 0 };
            auto objectsIterator = objects.begin();

            int lockError = pthread_rwlock_wrlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            object* currentObject = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader));
            for(size_t record = 0; record < NumberOfRecords(); record++)
            {
                if(std::memcmp(currentObject, &emptyObject, sizeof(object)) == 0)
                {
                    if(objectsIterator == objects.end())
                    {
                        break;
                    }

                    reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
                    memcpy(currentObject, &(*objectsIterator), sizeof(object));

                    ++objectsIterator;
                }

                currentObject++;
            }

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            if(objectsIterator != objects.end())
            {
                return RTN_EOF;
            }

            return RTN_OK;
        }

        RETCODE DeleteObject(size_t record)
        {
            object deletedObject = { 0 };
            return WriteObject(record, deletedObject);
        }

        RETCODE Clear(void)
        {
            if(m_IsOpen)
            {
                int lockError = pthread_rwlock_wrlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
                if(0 != lockError)
                {
                    return RTN_LOCK_ERROR;
                }

                size_t dbSize = reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords * sizeof(object);
                object* start = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader));

                memset(start, 0, dbSize);

                reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = 0;

                lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
                if(0 != lockError)
                {
                    return RTN_LOCK_ERROR;
                }

                return RTN_OK;
            }

            return RTN_MALLOC_FAIL;
        }

        using Predicate = std::function<bool(const object* currentObject)>;

        RETCODE FindFirstOf(Predicate predicate, size_t& out_Record)
        {
            int lockError = pthread_rwlock_rdlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            for(size_t record = 0; record < NumberOfRecords(); record++)
            {
                if(predicate(currentObject))
                {
                    out_Record = record;
                    break;
                }

                currentObject++;
            }

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }

        RETCODE FindObjects(Predicate predicate, std::vector<size_t>& out_MatchingObjects)
        {
            int lockError = pthread_rwlock_rdlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            for(size_t record = 0; record < NumberOfRecords(); record++)
            {
                if(predicate(currentObject))
                {
                    out_MatchingObjects.push_back(record);
                }

                currentObject++;
            }

            lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
            if(0 != lockError)
            {
                return RTN_LOCK_ERROR;
            }

            return RTN_OK;
        }

        inline size_t NumberOfRecords(void)
        {
            if(m_IsOpen)
            {
                return m_NumRecords;
            }

            return 0;
        }

        inline size_t LastWrittenRecord(void)
        {
            size_t lastWittenRecord = 0;

            if(m_IsOpen)
            {
                int lockError = pthread_rwlock_wrlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
                if(0 != lockError)
                {
                    return lastWittenRecord;
                }

                lastWittenRecord = reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten;

                lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
                if(0 != lockError)
                {
                    return lastWittenRecord;
                }
            }

            return lastWittenRecord;
        }

        dbInterface(const std::string& dbPath) :
            m_IsOpen(false), m_Size(0),
            m_DBAddress(nullptr)
        {
            int fd = open(dbPath.c_str(), O_RDWR);
            if(INVALID_FD > fd)
            {
                return;
            }

            struct stat statbuf;
            int error = fstat(fd, &statbuf);
            if(0 > error)
            {
                return;
            }

            m_Size = statbuf.st_size;
            m_DBAddress = static_cast<char*>(mmap(nullptr, m_Size,
                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                    fd, 0));
            if(MAP_FAILED == m_DBAddress)
            {
                return;
            }

            m_NumRecords = reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords;

            m_IsOpen = true;
        }

        ~dbInterface(void)
        {
            int error = munmap(m_DBAddress, m_Size);
            if(0 == error)
            {
                // Nothing much you can do in this case..
                m_IsOpen = false;
            }
        }

protected:

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

    static constexpr int INVALID_FD = 0;

    };
}

#endif