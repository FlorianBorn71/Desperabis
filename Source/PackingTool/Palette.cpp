#include "Palette.h"
#include "FileUtils.h"

bool Palette::Load(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);

	m_numValidEntries = 256;
	for (int i = 0; i < m_numValidEntries; i++)
	{
		uint8_t val[3];
		fread(val, 3, 1, fIn);
		PaletteEntry& pal = m_palette[i];
		pal.r = static_cast<uint8_t>((static_cast<int>(val[0]) * 255 / 63));
		pal.g = static_cast<uint8_t>((static_cast<int>(val[1]) * 255 / 63));
		pal.b = static_cast<uint8_t>((static_cast<int>(val[2]) * 255 / 63));
		pal.a = (i == 0) ? 0 : 255;
	}

	fclose(fIn);
	return true;

}


float Palette::PaletteEntry::GetError(uint8_t R, uint8_t G, uint8_t B) const
{
	auto ComponentDiff = [](uint8_t c1, uint8_t c2) -> float
		{
			float diff = static_cast<float>(c1) - static_cast<float>(c2);
			return diff * diff;
		};
	return ComponentDiff(R, r) + ComponentDiff(G, g) + ComponentDiff(B, b) * 0.5f; // lesser blue contribution
}


int Palette::FindBestEntry(uint8_t R, uint8_t G, uint8_t B) const
{
	float bestError = FLT_MAX;
	int bestIndex = -1;
	for (int i = 0; i < m_numValidEntries; i++)
	{
		float error = m_palette[i].GetError(R, G, B);
		if (bestIndex < 0 || error < bestError)
		{
			bestError = error;
			bestIndex = i;
			if (bestError == 0)
			{
				break;
			}
		}
	}
	return bestIndex;
}

void Palette::BuildGrayscale(int maxIndex /*= 7*/)
{
	m_numValidEntries = maxIndex + 1;
	for (int i = 0; i < m_numValidEntries; i++)
	{
		uint8_t val = static_cast<uint8_t>(i * 255 / maxIndex);
		m_palette[i].Set(val, val, val, i == 0 ? 0 : 255);
	}

}
