#pragma once
#include <vector>
#include "Palette.h"

struct Texture
{
	int width = 0, height = 0;
	int offsetX = 0, offsetY = 0;
	std::vector<unsigned char> m_data;
	bool hasAlpha = false;

	void Read(FILE* fIn);
	bool Load(const char* fileName);
	bool LoadTGA(const char* fileName, const Palette& palette);

	bool Save(const char* outFile) const;
	bool SaveTGA(const char* outFile, const Palette& palette) const;
	bool ToText(const char* outFile) const;
	bool ToMissionText(const char* outFile) const;

	bool FromText(const char* fileName);
	bool FromMissionText(const char* fileName);
};

