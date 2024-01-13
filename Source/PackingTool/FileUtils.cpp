#include "FileUtils.h"
#include <iostream>
#include <filesystem>

using namespace std;

static vector<filesystem::path> g_inputDirectories;
filesystem::path g_outputDirectory;

void FileUtils::AddInputDirectory(std::filesystem::path path)
{
	path = filesystem::absolute(path);
	cout << "Added input data directory " << path << endl;
	g_inputDirectories.push_back(std::move(path));
}

void FileUtils::SetOutputDirectory(std::filesystem::path path)
{
	path = filesystem::absolute(path);
	cout << "Set output data directory " << path << endl;
	if (!g_outputDirectory.empty())
	{
		cerr << "Warning: An output directory was already set." << endl;
	}
	g_outputDirectory = std::move(path);
}

FILE* FileUtils::OpenFile(const std::filesystem::path& path, std::string* out_fullPath)
{
	for (const filesystem::path& p : g_inputDirectories)
	{
		string s = (p / path).string();
		FILE* fIn = fopen(s.c_str(), "rb");
		if (fIn != nullptr)
		{
			if (out_fullPath != nullptr)
				*out_fullPath = s;
			return fIn;
		}
	}

	cerr << "File not found: " << path << endl;
	return nullptr;
}


FILE* FileUtils::CreateFile(const std::filesystem::path& path, std::string* out_fullPath)
{
	if (g_outputDirectory.empty())
	{
		cerr << "Tried to create a file without an output directory." << endl;
		return nullptr;
	}
	filesystem::path full = (g_outputDirectory / path).lexically_normal();
	filesystem::path pathOnly = full.parent_path();

	// create the full directory:
	try
	{
		filesystem::create_directories(pathOnly);
	}
	catch (const std::exception& e) 
	{
		cerr << "Error creating directory: " << e.what() << std::endl;
		return nullptr;
	}

	string s = full.string();
	FILE* fOut = fopen(s.c_str(), "wb");
	if (fOut == nullptr)
	{
		cerr << "failed to write file " << path << endl;
	}
	if (out_fullPath != nullptr)
		*out_fullPath = s;
	return fOut;
}

bool FileUtils::FileRead(FILE* fIn, void* buffer, int count)
{
	int actual = (int)fread(buffer, 1, count, fIn);
	if (actual != count)
	{
		cerr << "Failed to load " << count << " bytes." << endl;
		return false;
	}
	return true;
}

void FileUtils::SkipBytes(FILE* fIn, int bytes)
{
	char dummy;
	for (int i = 0; i < bytes; i++)
		fread(&dummy, 1, 1, fIn);
}

int FileUtils::GetFileSize(FILE* fIn)
{
	fseek(fIn, 0, SEEK_END);
	int fileLength = ftell(fIn);
	fseek(fIn, 0, SEEK_SET);
	return fileLength;
}