#pragma once

#include <DesperabisCommon.h>


class D_IMPEXP FileManager
{
public:
    static FileManager& Instance() 
    {
        return *g_Instance;
    }

    virtual std::wstring FilenameToNative(std::wstring_view filename) = 0;
    virtual FUTURE(std::vector<unsigned char>) ReadDataFuture(std::wstring_view filename) = 0;
    virtual bool Exists(std::wstring_view filename) = 0;

    virtual FUTURE(bool) WriteDataFuture(std::wstring_view filename, const std::vector<unsigned char>& data)
    {
        CO_RETURN false;
    };

protected:
    static FileManager* g_Instance;
};

