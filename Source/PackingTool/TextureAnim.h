#pragma once
#include "Texture.h"

class TextureAnim
{
public:
	bool Load(const char* fileName);
	bool ReplaceWithTGAs(const char* tgaBaseName, const Palette& palette, bool retainOffsets = true);

	bool Save(const char* outFile) const;
	bool SaveTGAs(const char* tgaBaseName, const Palette& palette) const;

	int m_countX = 0;
	int m_countY = 0;
	Texture m_frames[6][5];
};

