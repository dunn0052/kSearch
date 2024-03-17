#ifndef _LOGGER_HH
#define _LOGGER_HH

#include <common/OSdefines.hh>

#include <fstream>
#include <sstream>
#include <chrono>
#include <time.h>
#include <iomanip>
#include <string.h>
#include <sys/types.h>
//#include <unistd.h>
#ifdef WINDOWS_PLATFORM
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <unistd.h>
#include <sys/syscall.h>
#endif
#include <iostream>

#include <common/UtilityFunctions.hh>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Template creates compile time arguments
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define LOG_TEMPLATE( LEVEL, ... ) Logger::Instance().Log(std::cout, Logger::LEVEL, #LEVEL, GetCurrentProcessId(), GetCurrentThreadId(), __FILENAME__, __LINE__, ##__VA_ARGS__ )
#else
#define LOG_TEMPLATE( LEVEL, ... ) Logger::Instance().Log(std::cout, Logger::LEVEL, #LEVEL, getpid(), syscall(__NR_gettid), __FILENAME__, __LINE__, ##__VA_ARGS__ )
#endif
#define LOG_DEBUG( ... ) LOG_TEMPLATE( DEBUG, ##__VA_ARGS__ )
#define LOG_INFO( ... ) LOG_TEMPLATE( INFO, ##__VA_ARGS__ )
#define LOG_WARN( ... ) LOG_TEMPLATE( WARN, ##__VA_ARGS__ )
#define LOG_FATAL( ... ) LOG_TEMPLATE( FATAL, ##__VA_ARGS__ )

class Logger
{
public:

        typedef unsigned int LogLevel;
        static constexpr LogLevel FATAL = 0;
        static constexpr LogLevel WARN = 1;
        static constexpr LogLevel DEBUG = 2;
        static constexpr LogLevel INFO = 3;
        static constexpr LogLevel MAX_LOG_LEVEL = INFO;

        template<typename Stream, typename... RestOfArgs>
        Stream& Log(Stream& stream, LogLevel level, const char* debugLevel, int processID, int threadID, const char* fileName, int lineNum, const RestOfArgs& ... args)
        {
            /* Internal string stream used to ensure thread safety when printing.
             * It is passed through to collect the arguments into a single string,
             * which will do a single << to the input stream at the end
             */
            std::stringstream internalStream;
            return PrependLog(stream, internalStream, level, debugLevel, processID, threadID, fileName, lineNum, args...);
        }

        // Singleton instance
        static Logger& Instance(void)
        {
            static Logger instance;
            return instance;
        }

private:

        template<typename Stream, typename... RestOfArgs>
        Stream& PrependLog(Stream& stream, std::stringstream& internalStream, LogLevel level, const char* debugLevel, int processID, int threadID, const char* fileName, int lineNum, const RestOfArgs& ... args)
        {
            if(INFO != level)
            {
                std::chrono::system_clock::time_point systemTime = std::chrono::system_clock::now();
                std::chrono::microseconds microSeconds = std::chrono::duration_cast<std::chrono::microseconds>(systemTime.time_since_epoch()) % 1000000;
                time_t time = std::chrono::system_clock::to_time_t(systemTime);
                std::tm convertedTime = { 0 };

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
                errno_t error = localtime_s(&convertedTime, &time);
                if(!error)
                {
                    // [weekday mon day year hour:min:sec.usec]
                    internalStream
                        << "["
                        << std::put_time(&convertedTime, "%a %b %d %Y %H:%M:%S.")
                        << std::setfill('0') << std::setw(6) << microSeconds.count()
                        << "]";
                }
                else
                {
                    internalStream
                        << "[Time error: "
                        << ErrorString(error)
                        << "]";
                }
#else
                convertedTime = *localtime(&time);
                // [weekday mon day year hour:min:sec.usec]
                internalStream
                    << "["
                    << std::put_time(&convertedTime, "%a %b %d %Y %H:%M:%S.")
                    << std::setfill('0') << std::setw(6) << microSeconds.count()
                    << "]";
#endif
                // <filename:linenum>
                internalStream
                    << "{"
                    << fileName
                    << ":"
                    << lineNum
                    << "}";

                // {processID:threadID}
                internalStream
                    << "<"
                    << processID
                    << ":"
                    << threadID
                    << ">";

                // (debug level)
                internalStream
                    << "("
                    << debugLevel
                    << ")";

                // Space between decorator and user text
                internalStream << " ";

            }

            return Log(stream, internalStream, args...);
        }

        // Intermediate case
        template<typename Stream, typename ThisArg, typename... RestOfArgs>
        Stream& Log(Stream& stream, std::stringstream& internalStream, const ThisArg& arg1, const RestOfArgs&... args)
        {
            internalStream << arg1;
            return Log(stream, internalStream, args...);
        }

        // Base case
        template<typename Stream, typename ThisArg>
        Stream & Log(Stream& stream, std::stringstream& internalStream, const ThisArg& arg1)
        {
            internalStream << arg1 << "\n";
            return ( stream << internalStream.str() );
        }

        Logger()
        { }


        Logger(Logger const&) = delete;
        void operator = (Logger const&) = delete;
};

#endif