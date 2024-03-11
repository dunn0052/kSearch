#ifndef __OS_DEFINES_HH
#define __OS_DEFINES_HH

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))
#define WINDOWS_PLATFORM
#elif defined(__APPLE__)
#define MACOS_PLATFORM
#endif

#endif