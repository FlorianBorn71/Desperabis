// PackingTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <format>
#include <vector>
#include <cassert>
#include <sstream>
#include <direct.h>

#include "FileLists.h"
#include "Texture.h"
#include "Sound.h"
#include "DatFile.h"
#include "FileUtils.h"

using namespace std;



bool ExploreFilesInDAT(vector<DatFile::DirEntry>& entries, const char* fileIn)
{
	OPEN_OR_RETURN(fileIn, nullptr);

	__int16 magicXor;
	FileUtils::FileRead(fIn, &magicXor, sizeof(magicXor));
	__int16 numFiles;
	FileUtils::FileRead(fIn, &numFiles, sizeof(numFiles));

	DatFile::DirEntry entry;

	int firstEntry = (int)entries.size();
	for (int i = 0; i < numFiles; i++)
	{
		entry.ReadEntry(fIn, magicXor);
		entries.push_back(entry);
	}

	fclose(fIn);
	return true;
}

void BuildFileList(vector<DatFile::DirEntry>& entries, std::ostringstream& oss)
{
	oss << "static const std::string_view g_Filenames[] = {\n";
	for (const DatFile::DirEntry& entry : entries)
	{
		std::string_view sw(entry.m_name, entry.m_length);
		oss << "    \"" << entry.Filename() << "\"," << std::endl;
	}
	oss << "};";
}

bool BuildDAT(const char* outFile, string& out_path)
{
	vector<DatFile::DirEntry> entries;

	// load content of all files
	DatFile::DirEntry entry = { 0 };
	entry.m_fileOfs = g_initialFileOfsGAME;
	for (string_view name : g_FilenamesGAME)
	{
		entry.SetName(name);
		if (!entry.LoadContent())
		{
			cerr << "Failed to load file '" << name << "'" << endl;
			return false;
		}
		entries.push_back(entry);
		entry.m_fileOfs += entry.m_fileLength;
	}


	CREATE_OR_RETURN(outFile, &out_path);

	__int16 magicXor = g_MagicXor;
	fwrite(&magicXor, sizeof(magicXor), 1, fOut);
	__int16 numFiles = static_cast<__int16>(entries.size());
	fwrite(&numFiles, sizeof(numFiles), 1, fOut);

	for (const DatFile::DirEntry& e : entries)
	{
		e.WriteEntry(fOut, magicXor);
	}

	for (const DatFile::DirEntry& e : entries)
	{
		e.WriteContent(fOut);
	}

	fclose(fOut);
	return true;
}


void ConvertTexturesToText()
{
	Texture t;
//	t.Load("GRAFIK\\TUR29.256");
//	t.ToText("LEVEL\\Ingame29.txt");
	t.Load("GRAFIK\\TUR38.256");
	t.ToText("Ingame38.txt");
	t.Load("GRAFIK\\TUR39.256");
	t.ToText("Ingame39.txt");

	t.Load("GRAFIK\\HERZ.256");
	t.ToMissionText("Missions.txt");
}

bool ConvertTextToTextures()
{
	Texture t;
	if (!t.FromText("Ingame38.txt"))
		return false;
	if (!t.Save("GRAFIK\\TUR38.256"))
		return false;
	if (!t.FromText("Ingame39.txt"))
		return false;
	if (!t.Save("GRAFIK\\TUR39.256"))
		return false;
	if (!t.FromMissionText("Missions.txt"))
		return false;
	if (!t.Save("GRAFIK\\HERZ.256"))
		return false;
	return true;
}


bool ConvertSMPToWAV()
{
	Sound smp;
	if (!smp.LoadWAV("SAMP\\MUSIC1.WAV"))
		return false;

//	if (!smp.LoadSMP("SAMP\\SOUND1.SMP"))
//		return false;
	if (!smp.SaveWAV("Sound1.wav"))
		return false;

	return true;
}


int main(int argc, char* argv[])
{
	if (argc <= 1)
	{
		std::cout << "Command line args: " << std::endl;
		std::cout << "  -s <folder> : adds one source root folder (absolute path) under which the unpacked game assets are located" << std::endl;
		std::cout << "  -d <file> : absolute file path of the destination file (.DAT)." << std::endl;
		return 0;
	}
	else
	{
//		std::cout << "Number of arguments: " << argc - 1 << std::endl;
		// Print each command line argument
		for (int i = 1; i < argc; ++i)
		{
//			std::cout << "  Argument " << i << ": " << argv[i] << std::endl;
			if (_stricmp(argv[i], "-s") == 0 && i < argc - 1)
			{
				FileUtils::AddInputDirectory(argv[i + 1]);
				i++;
			}
			if (_stricmp(argv[i], "-d") == 0 && i < argc - 1)
			{
				FileUtils::SetOutputDirectory(argv[i + 1]);
				i++;
			}
		}
	}

	const bool loadOriginal = false;
	if (loadOriginal)
	{

		vector<DatFile::DirEntry> entries;

		// extract the list of files
		//ExploreFilesInDAT(entries, "GAME.DAT");
		//std::ostringstream oss;
		//BuildFileList(entries, oss);
		//string s = oss.str();
		ConvertSMPToWAV();
		ConvertTexturesToText();
	}
	else
	{
		if (!ConvertTextToTextures())
		{
			cerr << "An error occurred during text baking." << endl;
			return 2;
		}

		string datOutFile;
		if (!BuildDAT("GAME.DAT", datOutFile))
		{
			cerr << "An error occurred during packaging." << endl;
			return 3;
		}
		cout << "Packing file was successfully written to: " << datOutFile.c_str() << endl;
	}
	return 0;
}
