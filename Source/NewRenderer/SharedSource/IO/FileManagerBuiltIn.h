#pragma once
#include <IO\FileManager.h>

class FileManagerBuiltIn : public FileManager
{
public:
    FileManagerBuiltIn() = default;
    virtual ~FileManagerBuiltIn() = default;

    void Initialize();
    void Deinitialize();

    virtual std::wstring FilenameToNative(std::wstring_view filename) override;
    virtual FUTURE(std::vector<unsigned char>) ReadDataFuture(std::wstring_view filename) override;
	virtual bool Exists(std::wstring_view filename) override;

	void AddExternalDataDir(std::filesystem::path path);

public:
    static void RegisterFile(std::wstring_view name, const unsigned char* data, size_t dataSize);

private:
    FILE* OpenFile(const std::filesystem::path& path);
	std::vector<std::filesystem::path> m_externalDirs;
};
