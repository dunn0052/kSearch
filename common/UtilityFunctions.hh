#ifndef __UTILITY_FUNCTIONS_HH
#define __UTILITY_FUNCTIONS_HH

#include <string>
#include <iostream>
#include <common/OSdefines.hh>
static std::string ErrorString(int errornum)
{
#ifdef WINDOWS_PLATFORM
        char errmsg[94] = { 0 };
        errno_t error = strerror_s(errmsg, 94 - 1, errornum);
        if(error)
        {
            std::cerr << "Error printing errno: " << errornum << "/n";
        }
        return errmsg;
#else
        return strerror(errornum);
#endif
}

static bool isDirectory(const std::string& path)
{
#ifdef WINDOWS_PLATFORM
    DWORD attributes = GetFileAttributesA(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    // implement later for Linux
    return false;
#endif
}

#endif