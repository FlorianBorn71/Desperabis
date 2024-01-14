#pragma once
#include <stdint.h>


class Palette
{
public:
	struct PaletteEntry
	{
		void Set(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255)
		{
			r = R;
			g = G;
			b = B;
			a = A;
		}
		float GetError(uint8_t R, uint8_t G, uint8_t B) const;
		uint8_t r, g, b, a;
	};

	bool Load(const char* fileName);
	int FindBestEntry(uint8_t R, uint8_t G, uint8_t B) const;

	int m_numValidEntries = 256;
	PaletteEntry m_palette[256] = {0};
};
