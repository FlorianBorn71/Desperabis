#include "PatchedEXE.h"
#include "FileUtils.h"

using namespace std;

struct PatchEntryDef
{
	string_view key;
	int byteOffset;
	int maxCharacters;
	int prefixSpaces, postfixSpaces;
};


const PatchEntryDef defs[] =
{
"$PressAnyKey",0xad54,13,0,0,
"$SoundblasterNotFound",0xad62,27,0,0,
"$IOError",0xad7e,18,0,0,
"$InvalidSaveGame",0xad91,25,0,0,
"$InvalidVersionNumber",0xadb0,22,0,0,
"$File",0xadc7,6,0,1,
"$Missing",0xadce,6,1,0,
"$Off",0xb1e5,4,0,1,
"$On",0xb1ea,4,0,1,
};


class PatchDefinition
{
public:
	PatchDefinition()
		: m_entries(std::begin(defs), std::end(defs))
	{
		
	}

	bool Extract(const vector<uint8_t>& content, Patch& patch) const;

private:
	vector<PatchEntryDef> m_entries;
};


bool PatchDefinition::Extract(const vector<uint8_t>& content, Patch& patch) const
{

	return true;
}


bool PatchedEXE::Load(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);
	int size = FileUtils::GetFileSize(fIn);
	m_content.resize(size);
	FileUtils::FileRead(fIn, m_content.data(), size);
	fclose(fIn);
	return true;
}



bool PatchedEXE::Extract(Patch& patch) const
{
	PatchDefinition def;

	return true;
}
