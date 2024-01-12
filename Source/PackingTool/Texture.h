#pragma once
#include <vector>

namespace 
{
	static void FileRead(FILE* fIn, void* buffer, int size)
	{
		int actual = (int)fread(buffer, 1, size, fIn);
		if (actual != size)
		{
			buffer = nullptr;
			return;
		}
		//assert(actual == size);
	}
}

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

