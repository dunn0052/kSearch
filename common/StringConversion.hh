#ifndef __STRING_CONVERSION_HH
#define __STRING_CONVERSION_HH

#include <common/OSdefines.hh>

#ifdef WINDOWS_PLATFORM
#include <string>
#include <sstream>
#include <codecvt>
#include <locale>

#include <iostream>

static std::string ConvertToUTF8(const std::wstring& text)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf8_conv;
    try
    {
        return utf8_conv.to_bytes(text);
    }
    catch (std::range_error& error)
    {
        std::wstringstream errorText;
        errorText
            << L"Could not convert: "
            << text
            << L" to UTF8 from wide string: "
            << error.what();
        std::wcout << errorText.str();

        return std::string();
    }
}

static std::wstring ConvertToWideString(const std::string& text)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wchar_convert;

    try
    {
        return wchar_convert.from_bytes(text);
    }
    catch (std::range_error& error)
    {
        std::stringstream errorText;
        errorText
            << "Could not convert: "
            << text
            << " to wide string from UTF8: "
            << error.what();
        std::cout << errorText.str();

        return std::wstring();
    }
}
#endif

#endif