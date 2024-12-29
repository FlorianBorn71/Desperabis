#pragma once
#include <DesperabisCommon.h>

class AssetStream
{
public:
    AssetStream(const std::vector<byte>& buffer)
        : m_rawBuffer(buffer)
    {

    }

    int Read(char* dest, int count);

    template<typename T>
    T Read() 
    { 
        T obj; 
        Read((char *)&obj, sizeof(T)); 
        return obj; 
    }

    template<typename T>
    int Read(std::vector<T>& dest, int numElements)
    {
        int oldSize = (int)dest.size();
        dest.resize(oldSize + numElements);
        T* data = dest.data() + oldSize;
        return Read((char*)data, numElements * sizeof(T));
    }

    void Read(std::string& dest)
    {
        int len = Read<byte>(); // seems to be the case for short strings
        char str[4096];
        Read(str, len); 
        str[len] = 0;
        dest = str;
    }

    void ReadText(std::string& dest)
    {
        dest.assign((const char*)m_rawBuffer.data(), m_rawBuffer.size());
    }

private:
    int m_currentPos = 0;
    const std::vector<byte>& m_rawBuffer;
};

class D_IMPEXP AssetBase
{
public:
    AssetBase() = default;
    virtual ~AssetBase() = default;

    inline bool IsLoaded() const { return m_loaded; }

    virtual void Read(AssetStream& stream) = 0;

    virtual void CreateRuntimes(Renderer& renderer) {}

    virtual void Reset() {}

    inline uint32_t GetAssetId() const {return m_assetId;}

    void SetDebugName(std::wstring_view name)
    {
#ifdef _DEBUG
        m_debugName = name;
#endif
    }
protected:
    friend class AssetManager;
    friend class Renderer;

    bool m_loaded = false;
    uint32_t m_assetId = 0;
#ifdef _DEBUG
    std::wstring m_debugName;
#endif
};


