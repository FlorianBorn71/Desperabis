#include "TextureAnim.h"
#include "FileUtils.h"
#include <sstream>
#include <iostream>

using namespace std;

bool TextureAnim::Load(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);
	uint8_t value[2];
	fread(value, 2, 1, fIn);
	m_countX = value[0];
	m_countY = value[1];
	for (int x = 0; x < m_countX; x++)
	{
		for (int y = 0; y < m_countY; y++)
		{
			Texture& frame = m_frames[x][y];
			frame.Read(fIn, true);
		}
	}
	return true;
}

bool TextureAnim::ReplaceWithTGAs(const char* tgaBaseName, const Palette& palette, bool retainOffsets)
{
	if (m_countX < 1 || m_countY < 1)
	{
		cerr << "Can only load TGA frames on top of a loaded animation. File: '" << tgaBaseName << "'" << endl;
		return false;
	}
	for (int x = 0; x < m_countX; x++)
	{
		for (int y = 0; y < m_countY; y++)
		{
			std::ostringstream frameFile;
			frameFile << tgaBaseName << "." << x << "." << y << ".tga";
			Texture& frame = m_frames[x][y];
			string s = frameFile.str();
			FILE* exists = FileUtils::OpenFile(s.c_str(), nullptr, false); // no warning
			if (exists == nullptr)
			{
				// gracefully ignore frames that are missing...
				continue;
			}
			fclose(exists);
			if (!frame.LoadTGA(s.c_str(), palette, retainOffsets))
			{
				//...but fail hard if loading failed
				cerr << "Failed to load tga frame '" << s.c_str() << "'" << endl;
				return false;
			}
		}
	}

	return true;
}

bool TextureAnim::Save(const char* outFile) const
{
	CREATE_OR_RETURN(outFile, nullptr);
	uint8_t value[2] = { static_cast<uint8_t>(m_countX), static_cast<uint8_t>(m_countY) };
	fwrite(value, 2, 1, fOut);
	for (int x = 0; x < m_countX; x++)
	{
		for (int y = 0; y < m_countY; y++)
		{
			const Texture& frame = m_frames[x][y];
			frame.Write(fOut, true);
		}
	}

	return true;
}

bool TextureAnim::SaveTGAs(const char* tgaBaseName, const Palette& palette) const
{
	for (int x = 0; x < m_countX; x++)
	{
		for (int y = 0; y < m_countY; y++)
		{
			std::ostringstream frameFile;
			frameFile << tgaBaseName << "." << x << "." << y << ".tga";
			const Texture& frame = m_frames[x][y];
			string s = frameFile.str();
			if (!frame.SaveTGA(s.c_str(), palette))
			{
				cerr << "Failed to save tga frame '" << s.c_str() << "'" << endl;
				return false;
			}
		}
	}

	return true;
}
