#pragma once
#include <filesystem>

namespace FileUtils
{
#define OPEN_OR_RETURN(_path, outPathPtr) \
  	FILE* fIn = FileUtils::OpenFile(_path, outPathPtr);\
	if (fIn == nullptr)\
	{\
		return false;\
	}

#define CREATE_OR_RETURN(_path, outPathPtr) \
  	FILE* fOut = FileUtils::CreateFile(_path, outPathPtr);\
	if (fOut == nullptr)\
	{\
		return false;\
	}


	void AddInputDirectory(std::filesystem::path path, bool insertAtTop = false);
	void SetOutputDirectory(std::filesystem::path path);
	FILE* OpenFile(const std::filesystem::path& path, std::string* out_fullPath = nullptr, bool warningOnFileNotFound = true);
	FILE* CreateFile(const std::filesystem::path& path, std::string* out_fullPath = nullptr);
	bool FileRead(FILE* f, void* buffer, int count);
	void SkipBytes(FILE* fIn, int bytes);
	int GetFileSize(FILE* fIn);

	bool PlainCopy(std::filesystem::path srcFile, std::filesystem::path destFile);
};

