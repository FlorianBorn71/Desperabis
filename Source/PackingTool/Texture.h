#pragma once
#include <vector>

struct Texture
{
	int width = 0, height = 0;
	std::vector<unsigned char> m_data;
	bool hasAlpha = false;

	void Read(FILE* fIn);
	bool Load(const char* fileName);

	bool Save(const char* fileName) const;
	bool ToText(const char* outFile) const;
	bool ToMissionText(const char* outFile) const;

	bool FromText(const char* fileName);
	bool FromMissionText(const char* fileName);
};

