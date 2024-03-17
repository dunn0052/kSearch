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

#endif