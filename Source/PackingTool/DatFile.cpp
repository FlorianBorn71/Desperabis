#include "DatFile.h"
#include "FileLists.h"
#include "FileUtils.h"

#include <cassert>
#include <iostream>
#include <iosfwd>


using namespace std;


//////////////////////////////////////////////////////////////////////////////

const DatFile DatFile::Game("GAME.DAT", g_FilenamesGAME, std::size(g_FilenamesGAME));
const DatFile DatFile::Sounds("SOUNDS.DAT", g_FilenamesSOUNDS, std::size(g_FilenamesSOUNDS));
const DatFile DatFile::Music("MUSIC.DAT", g_FilenamesMUSIC, std::size(g_FilenamesMUSIC));

//////////////////////////////////////////////////////////////////////////////


void DatFile::DirEntry::SetName(string_view n)
{
	m_length = static_cast<unsigned char>(n.length());
	memset(m_name, 0, sizeof(m_name));
	memcpy(m_name, n.data(), m_length);
}

void DatFile::DirEntry::ReadEntry(FILE* fIn, __int16 magicXor)
{
	fread(this, 1, g_DirEntrySize, fIn);

	unsigned __int16* n2 = (unsigned __int16*)this;

	for (int x = 0; x < g_DirEntrySize / 2; x++)
		n2[x] = n2[x] ^ magicXor;

	assert(m_length < 19);
	m_name[m_length] = 0;
}

void DatFile::DirEntry::WriteEntry(FILE* fOut, __int16 magicXor) const
{
	__int16 temp[g_DirEntrySize / 2];
	memcpy(temp, this, g_DirEntrySize);
	for (int x = 0; x < g_DirEntrySize / 2; x++)
		temp[x] = temp[x] ^ magicXor;

	fwrite(temp, g_DirEntrySize, 1, fOut);
}


bool DatFile::DirEntry::LoadContent()
{
	string n(Filename());
	string absPath;
	OPEN_OR_RETURN(n.c_str(), &absPath);

	auto fileSizeControl = std::filesystem::file_size(absPath);
	m_fileLength = FileUtils::GetFileSize(fIn);
	if (m_fileLength == 0 || m_fileLength != fileSizeControl)
	{
		cerr << "Content of file '" << absPath << "' reported to have invalid file size." << endl;
		return false;
	}
	m_content.resize(m_fileLength);
	fread(m_content.data(), 1, m_fileLength, fIn);
	fclose(fIn);
	return true;
}

void DatFile::DirEntry::WriteContent(FILE* fOut) const
{
	fwrite(m_content.data(), m_fileLength, 1, fOut);
}


bool DatFile::DirEntry::HasExtension(const char* ext) const
{
	const char* extension = strchr(m_name, '.');
	assert(extension != nullptr);
	extension++;

	return _stricmp(extension, ext) == 0;
}

bool DatFile::DirEntry::HasName(const char* name) const
{
	return _stricmp(m_name, name) == 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////


DatFile::DatFile(string_view name, const string_view* fileList, size_t numFiles)
{
	m_outName = name;
	m_fileList = fileList;
	m_numFiles = static_cast<int>(numFiles);
}

bool DatFile::PackageAndSave(string* out_path) const
{
	vector<DatFile::DirEntry> entries;
	entries.reserve(m_numFiles);

	// load content of all files
	DatFile::DirEntry entry = { 0 };
	entry.m_fileOfs = 4 + g_DirEntrySize * m_numFiles;
	for (int i = 0; i < m_numFiles; i++)
	{
		string_view name = m_fileList[i];
		entry.SetName(name);
		if (!entry.LoadContent())
		{
			cerr << "Failed to load file '" << name << "'" << endl;
			return false;
		}
		entries.push_back(entry);
		entry.m_fileOfs += entry.m_fileLength;
	}


	CREATE_OR_RETURN(m_outName.c_str(), out_path);

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


std::string_view DatFile::GetFileAtOffset(int ofs, int& ofsWithin) const
{
	ofsWithin = 0;
	return "<file not found>";
}

bool DatFile::ExploreFilesInDAT(const char* fileIn, vector<DatFile::DirEntry>& entries)
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


std::string DatFile::BuildFileList(const char* DATFile, const char* postFix)
{
	std::ostringstream oss;
	vector<DatFile::DirEntry> entries;
	if (!ExploreFilesInDAT(DATFile, entries))
		return "<error>";

	oss << "static const std::string_view g_Filenames"<< postFix<< "[] = {\n";
	for (const DatFile::DirEntry& entry : entries)
	{
		std::string_view sw(entry.m_name, entry.m_length);
		oss << "    \"" << entry.Filename() << "\"," << std::endl;
	}
	oss << "};";
	return oss.str();
}
