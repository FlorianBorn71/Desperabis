#include "PatchedEXE.h"
#include "FileUtils.h"
#include <iostream>

using namespace std;

struct PatchEntryDef
{
	string_view key;
	int byteOffset;
	int maxCharacters;
	string_view prefixString;
	string_view postfixString;
	bool fillwithSpaces;
};


const PatchEntryDef defs[] =
{
"$PressAnyKey",0xad50,17,"... ","", false,
"$SoundblasterNotFound",0xad62,27,"","", false,
"$IOError",0xad7e,18,""," :", false,
"$InvalidSaveGame",0xad91,25,""," STAND", false,
"$InvalidVersionNumber",0xadb0,22,"","", false,
"$File",0xadc7,6,""," ", false,
"$Missing",0xadce,6," ","", false,
"$Off",0xb1e5,4,""," ", false,
"$On",0xb1ea,4,""," ", false,
"$Off", 0xb1ef, 13, "", "", true,
"$VolumeSlider", 0xb1fd, 14, "", "~\"", true,
"$Keyboard", 0xb20c, 9, "", "", true,
"$Joystick", 0xb216, 9, "", "", true,
"$Mouse", 0xb220, 9, "","", true,
"$Fullscreen", 0xb22a, 10, "","", true,
"$FullscreenWithUI", 0xb235, 10, "","", true,
"$Meters", 0xb240, 6, " ","", false,
"$Gamma.Dark", 0xb247, 6, "","", true,
"$Gamma.Medium", 0xb24e, 6, "", "", true,
"$Gamma.Bright", 0xb255, 5, "","", true,

"$CalibrateJoystick.1", 0xb69e, 17, "", "", false,
"$CalibrateJoystick.2", 0xb6b0, 16,"","", false,
"$CalibrateJoystick.3", 0xb6c1, 17, "... ", "", false,
"$CalibrateJoystick.4", 0xb70e, 20, "","", false,

"$Key.Return", 0xb83d, 6, "","", false,
"$Key.Backspace", 0xb844, 3, "","", false,
"$Key.Tab", 0xb848, 3, "","", false,
"$Key.Space", 0xb84c, 5, "","", false,
"$Key.CursorUp", 0xb852, 11, "","", false,
"$Key.CursorDown", 0xb85e, 13, "","", false,
"$Key.CursorLeft", 0xb86c, 12, "","", false,
"$Key.CursorRight", 0xb879, 13, "","", false,
"$Key.PgDn", 0xb887, 11, "","", false,
"$Key.PgUp", 0xb893, 9, "","", false,
"$Key.Pos1", 0xb89d, 5, "","", false,
"$Key.End", 0xb8a3, 4, "","", false,
"$Key.Insert", 0xb8a8, 5, "","", false,
"$Key.Del", 0xb8ae, 4, "","", false,
"$Key.ShiftRight", 0xb8c5, 12, "","", false,
"$Key.ShiftLeft", 0xb8d2, 11, "","", false,
"$Key.CTRL", 0xb8de, 4, "","", false,
"$Key.Alt", 0xb8e3, 3, "","", false,
"$Menu.Keymapping", 0xbbf9, 16, ""," :", false,

// the following menu items can probably be max 15, because they from an array? (See offset diffs)
"$Menu.Sounds", 0x31750, 6, "", "", false,
"$Menu.Ambient", 0x31760, 9, "", "", false,
"$Menu.Control", 0x31770, 7, "", "", false,
"$Menu.Screen", 0x31780, 10, "", "", false,
"$Menu.RedTint", 0x31790, 10, "", "", false,
"$Menu.ViewRange", 0x317a0, 10, "", "", false,
"$Menu.Brightness", 0x317b0, 10, "", "", false,
"$Menu.Done", 0x317c0, 6, "", "", false,

"$Menu.LoadASavegameHeadline", 0xcc29, 16, "", "", false,
"$Menu.SaveASavegameHeadline", 0xcde7, 20, "", "", false,
"$Menu.ReturnToGame", 0xd233,22, "", "", false,
"$Menu.NewGame", 0xd24a,11, "", "", false,
"$Menu.LoadGame", 0xd256,16, "", "", false,
"$Menu.SaveGame", 0xd267,20, "", "", false,
"$Menu.Settings", 0xd27c,13, "", "", false,
"$Menu.Configuration", 0xd28a,13, "", "", false,
"$Menu.BackToDOS", 0xd298,13, "", "", false,

"$SoundOn", 0xf6c3, 9, "","", false,
"$SoundOff", 0xf6cd, 9, "", "", false,
"$AmbientOn", 0xf6d7, 13, "", "", false,
"$AmbientOff", 0xf6e5, 13, "", "", false,
"$Keyboard", 0xf6f3, 8, "", "", true,
"$Joystick", 0xf6fc, 8 , "", "", true,
"$Mouse", 0xf705, 8, "", "", true,
"$AmbientVolume", 0xf70e, 15, "", " ", false,

"$IO.ERROR", 0x11901, 8, "", " : ", false,
"$IO.ReadError", 0x1190a, 24, "", " ", false,
"$IO.WriteProtection", 0x11923, 16, ". ", "?", false,
"$IO.XMS", 0x11934, 24, "", "", false,
"$IO.XMS.Further", 0x1194d, 8,  "", " ", false,
"$IO.XMS.Required", 0x11956, 15, "K ", "", false,
"$IO.XMS.UnknownError", 0x11966, 23, "", " ", false,

"$Inventory.Health", 0x12276, 11, "", ":", false,
"$Inventory.Gold", 0x12282, 5, "", ":", false,

"$Door.Locked", 0x175ad, 13, "", "", false,
"$Door.WrongKey", 0x175bb, 18, "", "", false,

// the following menu items can probably be at least 20, because they from an array? (See offset diffs)
"$Move.Forward", 0x317d8, 3, "", "", false,
"$Move.Back", 0x317ed, 6, "", "", false,
"$Move.RotateLeft", 0x31802, 12, "", "", false,
"$Move.RotateRight", 0x31817, 13, "", "", false,
"$Move.StrafeLeft", 0x3182c, 11, "", "", false,
"$Move.StrafeRight", 0x31841, 12, "", "", false,
"$Move.Crouch", 0x31856, 6, "", "", false,
"$Move.Jump", 0x3186b, 8, "", "", false,
"$Move.LookUp", 0x31880, 11, "", "", false,
"$Move.LookDown", 0x31895, 13, "", "", false,
"$Move.LookStraight", 0x318aa, 9, "", "", false,
"$Move.UseWeapon", 0x318bf, 14, "", "", false,
"$Move.PickUp", 0x318d4, 8, "", "", false,
"$Move.SwapHands", 0x318e9, 14, "", "", false,

"$Menu.SoundCard", 0x3190e, 10, "", "", false,
"$Menu.JoystickCalib", 0x31976, 14, "", "", false,
"$Menu.Keymapping", 0x31990, 14, "", "", false,
"$Menu.ChangeKeymapping", 0x319aa, 15, "", "", false,
"$Menu.Done", 0x319c4, 6, "", "", false,
};


class PatchDefinition
{
public:
	PatchDefinition()
		: m_entries(std::begin(defs), std::end(defs))
	{
		
	}

	const PatchEntryDef* FindEntryByKey(string_view key) const;
	bool Extract(const vector<uint8_t>& content, Patch& patch) const;
	bool Apply(vector<uint8_t>& content, const Patch& patch) const;
	bool ApplyEntry(vector<uint8_t>& content, const Patch::PatchEntry& entry) const;
private:
	vector<PatchEntryDef> m_entries;
};


const PatchEntryDef* PatchDefinition::FindEntryByKey(string_view key) const
{
	for (int i = 0; i < m_entries.size(); i++)
	{
		if (m_entries[i].key == key)
			return &m_entries[i];
	}
	return nullptr;
}

bool PatchDefinition::Extract(const vector<uint8_t>& content, Patch& patch) const
{
	patch.m_entries.clear();
	patch.m_entries.reserve(m_entries.size());

	const char* data = (const char*)content.data();
	for (int i = 0; i < m_entries.size(); i++)
	{
		const PatchEntryDef& def = m_entries[i];
		/*
		if (data[def.byteOffset] != def.maxCharacters)
		{
			// This is only an error if we extract from the original file
			cerr << "byte that indicates string length does not match max length. key: " << def.key << endl;
			return false;
		}
		*/
		const char* stringStart = &data[def.byteOffset + 1 + def.prefixString.length()];
		size_t availableLen = def.maxCharacters - def.prefixString.length();
		if (def.fillwithSpaces)
		{
			while (availableLen > 0 && stringStart[availableLen - 1] == 0x20)
				availableLen--;
		}
		if (availableLen <= def.postfixString.length())
		{
			// This is only an error if we extract from the original file
			cerr << "Broken string for key: " << def.key << endl;
			return false;
		}
		availableLen -= def.postfixString.length();
		string_view val(stringStart, availableLen);
		int existingEntry = patch.FindEntryByKey(def.key);
		if (existingEntry >= 0)
		{
			// if there is a value in the patch for the same key, don't add it again, but make sure, it's value is identical
			/*
			if (patch.m_entries[existingEntry].m_value != val)
			{
				cerr << "Values for the same key must be consistent: Was '" << patch.m_entries[existingEntry].m_value << "' but should be '" << val << "'" << endl;
				return false;
			}
			*/
			continue;
		}

		Patch::PatchEntry entry;
		entry.m_key = def.key;
		entry.m_value = val;
		patch.m_entries.push_back(std::move(entry));
	}
	return true;
}

bool PatchDefinition::ApplyEntry(vector<uint8_t>& content, const Patch::PatchEntry& entry) const
{
	int found = 0;
	// apply to all keys (can be multiple in definition class)
	for (const PatchEntryDef& def : m_entries)
	{
		if (def.key != entry.m_key)
			continue;

		int availableMax = static_cast<int>(def.maxCharacters - def.prefixString.length() - def.postfixString.length());
		int valLength = static_cast<int>(entry.m_value.length());
		if (availableMax < valLength)
		{
			cerr << "Value for entry with key '" << entry.m_key << "' exceeds maximum length of " << availableMax << endl;
			return false;
		}

		char* data = (char*)&content.data()[def.byteOffset];
		int writtenLength = def.fillwithSpaces ? def.maxCharacters : static_cast<int>(valLength + def.prefixString.length() + def.postfixString.length());
		*data++ = writtenLength;// Turbo Pascal string length

		auto WritBytes = [&data](string_view s) -> int
			{
				int len = static_cast<int>(s.length());
				if (len > 0)
				{
					memcpy(data, s.data(), len);
					data += len;
				}
				return len;
			};
		int written = WritBytes(def.prefixString);
		written += WritBytes(entry.m_value);
		written += WritBytes(def.postfixString);
		for (int i = written; i < def.maxCharacters; i++, data++)
			*data = 0x20;
		found++;
	}
	
	if (found == 0)
	{
		cerr << "Entry with key '" << entry.m_key << "' not found " << endl;
		return false;
	}
	return true;
}

bool PatchDefinition::Apply(vector<uint8_t>& content, const Patch& patch) const
{
	const char* data = (const char*)content.data();
	for (const Patch::PatchEntry& entry : patch.m_entries)
	{
		if (!ApplyEntry(content, entry))
		{
			return false;
		}
	}
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


// Save the patched .EXE
bool PatchedEXE::Save(const char* fileName) const
{
	CREATE_OR_RETURN(fileName, nullptr);

	fwrite(m_content.data(), m_content.size(), 1, fOut);

	fclose(fOut);
	return true;
}

bool PatchedEXE::IsEqual(const PatchedEXE& other) const
{
	if (m_content.size() != other.m_content.size())
	{
		return false;
	}
	for (size_t i = 0; i < m_content.size(); ++i) 
	{
		if (m_content[i] != other.m_content[i])
		{
			return false; 
		}
	}
	return true;
}

bool PatchedEXE::Extract(Patch& patch) const
{
	PatchDefinition def;
	return def.Extract(m_content, patch);
}

bool PatchedEXE::Apply(const Patch& patch)
{
	PatchDefinition def;
	return def.Apply(m_content, patch);
}


// Load a patch file
bool Patch::Load(const char* fileName)
{
	string absFile;
	OPEN_OR_RETURN(fileName, &absFile);
	char buffer[1024];
	// read line by line
	while (fgets(buffer, sizeof(buffer), fIn) != nullptr)
	{
		int len = static_cast<int>(strlen(buffer));
		string_view line(buffer, len);
		if (line.starts_with("$"))
		{
			size_t pos = line.find(':');
			if (pos == std::string::npos)
			{
				cerr << "Key/value must be separated by ':' line:" << line << endl;
				return false;
			}
			// Trim whitespaces at the end
			while (len > pos && std::isspace(buffer[len - 1]))
			{
				len--;
			}
			string_view key(buffer, pos);
			string_view value(buffer + pos + 1, len - pos - 1);

			PatchEntry entry;
			entry.m_key = key;
			entry.m_value = value;

			m_entries.push_back(std::move(entry));
		}
		// ignore all other lines
	}

	fclose(fIn);
	return true;
}

// Load a patch file
bool Patch::Save(const char* fileName) const
{
	CREATE_OR_RETURN(fileName, nullptr);

	PatchDefinition defTable;
	std::ostringstream oss;
	for (const Patch::PatchEntry& entry : m_entries)
	{
		if (const PatchEntryDef* def = defTable.FindEntryByKey(entry.m_key))
		{
			int availableMax = static_cast<int>(def->maxCharacters - def->prefixString.length() - def->postfixString.length());
			oss << "# Maximum length : " << availableMax << " characters." << endl;

		}
		else
		{
			oss << "# Error: key  '"<< entry.m_key << "' does not exist in definition." << endl;
		}
		oss << entry.m_key << ":" << entry.m_value << endl;
	}
	string s = oss.str();
	fwrite(s.data(), s.size(), 1, fOut);
	fclose(fOut);
	return true;
}

int Patch::FindEntryByKey(string_view key) const
{
	for (int i = 0; i < m_entries.size(); i++)
	{
		if (m_entries[i].m_key == key)
			return i;
	}
	return -1;
}
