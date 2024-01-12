#include "DatFile.h"
#include "FileLists.h"

#include <cassert>


using namespace std;


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
	FILE* fIn = fopen(n.c_str(), "rb");
	if (fIn == nullptr)
		return false;

	fseek(fIn, 0, SEEK_END);
	m_fileLength = ftell(fIn);
	fseek(fIn, 0, SEEK_SET);
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
