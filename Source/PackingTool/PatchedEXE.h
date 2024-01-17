#pragma once
#include <vector>
#include <cstdint>
#include <string>


class Patch
{
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

	bool Extract(Patch& patch) const;
private:
	std::vector<uint8_t> m_content;
};

