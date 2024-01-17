#pragma once
#include <vector>
#include <cstdint>
#include <string>


class Patch
{
public:
	bool Load(const char* fileName);
	bool Save(const char* fileName) const;
	int FindEntryByKey(std::string_view key) const;

	struct PatchEntry
	{
		std::string m_key;
		std::string m_value;
	};

	std::vector<PatchEntry> m_entries;
};


class PatchedEXE
{
public:
	bool Load(const char* fileName);
	bool Save(const char* fileName) const;
	bool IsEqual(const PatchedEXE& other) const;
	bool Extract(Patch& patch) const;
	bool Apply(const Patch& patch);
private:
	std::vector<uint8_t> m_content;
};

