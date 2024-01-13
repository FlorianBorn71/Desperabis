#include "Texture.h"
#include "FileUtils.h"
#include <cassert>
#include <iostream>
#include <sstream>


using namespace std;

void Texture::Read(FILE* fIn)
{
	__int16 w, h;
	FileUtils::FileRead(fIn, &w, sizeof(w));
	FileUtils::FileRead(fIn, &h, sizeof(h));
	assert(w < 512 && h < 512);

	width = w + 1;
	height = h + 1;
	int count = width * height;
	m_data.resize(count);
	FileUtils::FileRead(fIn, m_data.data(), count);
}

bool Texture::Load(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);

	Read(fIn);
	fclose(fIn);
	return true;
}

bool Texture::Save(const char* fileName) const
{
	CREATE_OR_RETURN(fileName, nullptr);

	__int16 w = width - 1, h = height - 1;
	fwrite(&w, sizeof(w), 1, fOut);
	fwrite(&h, sizeof(h), 1, fOut);
	int count = width * height;
	fwrite(m_data.data(), count, 1, fOut);

	fclose(fOut);
	return true;
}


bool Texture::ToText(const char* outFile) const
{
	CREATE_OR_RETURN(outFile, nullptr);

	std::ostringstream oss;
	const unsigned char* ptr = m_data.data();
	for (int h = 0; h < height; h++, ptr += width)
	{
		size_t len = static_cast<size_t>(ptr[0]);
		if (len >= width)
		{
			cerr << "Invalid text length in file '" << outFile << "'" << endl;
			return false;
		}
		const char* str = (const char*)&ptr[1];
		while (len > 0 && str[len - 1] == 0) // happens for line 2 of tur38
			len--;
		string_view s(str, len);
		oss << s << std::endl;
	}

	string buf = oss.str();
	fwrite(buf.data(), buf.length(), 1, fOut);

	fclose(fOut);
	return true;
}

bool Texture::ToMissionText(const char* outFile) const
{
	CREATE_OR_RETURN(outFile, nullptr);

	std::ostringstream oss;
	const unsigned char* ptr = m_data.data();
	for (int h = 0; h < height; h++, ptr += width)
	{
		size_t len = static_cast<size_t>(ptr[0]);
		if (len >= width)
		{
			cerr << "Invalid text length in file '" << outFile << "'" << endl;
			return false;
		}
		if ((h & 1) == 0)
		{
			// mission name
			const char* str = (const char*)&ptr[1];
			while (len > 0 && str[len - 1] == 0) // happens for line 2 of tur38
				len--;
			string_view s(str, len);
			oss << s << std::endl;
		}
		else
		{
			// list of missions
			for (int i = 0; i < len; i++)
			{
				int level = static_cast<int>(ptr[i + 1]);
				if (i > 0)
					oss << ",";
				oss << level;
			}
			if (ptr[len + 1] == 96)
			{
				oss << ",END";
			}
			oss << std::endl;
		}
	}

	string buf = oss.str();
	fwrite(buf.data(), buf.length(), 1, fOut);

	fclose(fOut);
	return true;
}



struct TextLine
{
	unsigned char m_length = 0;
	char m_buffer[1023] = {0};
};

bool Texture::FromText(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);

	height = width = 0;

	char buffer[1024];
	vector<TextLine> lines;
	TextLine line;
	while (fgets(buffer, sizeof(buffer), fIn) != nullptr) {
		int len = static_cast<int>(strlen(buffer));
		if (len == 0 || len > 256)
		{
			cerr << "Invalid line (" << lines.size() << ") in file '" << fileName << "'" << endl;
			return false;
		}
		len--; // remove '\n' at end
		memset(line.m_buffer, 0, sizeof(line.m_buffer));
		memcpy(line.m_buffer, buffer, len);
		line.m_buffer[len] = 0;
		line.m_length = len;
		lines.push_back(line);
		width = std::max(width, line.m_length + 1);
	}
	fclose(fIn);

	height = static_cast<int>(lines.size());
	int count = width * height;
	m_data.resize(count);

	// Copy lines:
	unsigned char* ptr = m_data.data();
	for (int h = 0; h < height; h++, ptr += width)
	{
		memcpy(ptr, &lines[h], width); // the line contains the length
	}

	return true;
}

bool Texture::FromMissionText(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);

	height = width = 0;

	// first load all lines
	char buffer[1024];
	vector<TextLine> lines;
	TextLine line;
	while (fgets(buffer, sizeof(buffer), fIn) != nullptr) {
		int len = static_cast<int>(strlen(buffer));
		if (len == 0 || len > 256)
		{
			cerr << "Invalid line (" << lines.size() << ") in file '" << fileName << "'" << endl;
			return false;
		}
		len--; // remove '\n' at end
		memset(line.m_buffer, 0, sizeof(line.m_buffer));
		if ((lines.size() & 1) == 0)
		{
			// mission name
			memcpy(line.m_buffer, buffer, len);
			line.m_buffer[len] = 0;
			line.m_length = len;
		}
		else
		{
			// list of mission indices
			string input(buffer, len);
			std::istringstream stream(input);
			int levelIndex = 0;
			line.m_length = 0;
			while (stream >> levelIndex)
			{
				line.m_buffer[line.m_length++] = levelIndex;
				// Check for a comma and discard it
				if (stream.peek() == ',')
					stream.ignore();
				if (stream.peek() == 'E') // 'END'
				{
					line.m_buffer[line.m_length] = 96; // does not inc m_length
					width = std::max(width, line.m_length + 2);
					break;
				}

			}
		}

		lines.push_back(line);
		width = std::max(width, line.m_length + 1);
	}
	fclose(fIn);

	height = static_cast<int>(lines.size());
	if (height & 1)
	{
		cerr << "Mission file must have an even number of lines" << endl;
	}
	int count = width * height;
	m_data.resize(count);

	// Copy lines:
	unsigned char* ptr = m_data.data();
	memset(ptr, 0, count);
	for (int h = 0; h < height; h++, ptr += width)
	{
		memcpy(ptr, &lines[h], width); // use width, not m_length+1, because the 'END' level does not count but is still in the list
	}

	return true;
}