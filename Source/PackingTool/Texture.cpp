#include "Texture.h"
#include "FileUtils.h"
#include <cassert>
#include <iostream>
#include <sstream>


using namespace std;

void Texture::Read(FILE* fIn, bool includeOfs)
{
	__int16 w, h;
	if (includeOfs)
	{
		FileUtils::FileRead(fIn, &w, sizeof(w));
		FileUtils::FileRead(fIn, &h, sizeof(h));
		offsetX = w;
		offsetY = h;
	}
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

	offsetX = offsetY = 0;
	Read(fIn);
	fclose(fIn);
	return true;
}



void Texture::Write(FILE* fOut, bool includeOfs /*= false*/) const
{
	if (includeOfs)
	{
		__int16 offset[2] = { static_cast<__int16>(offsetX), static_cast<__int16>(offsetY) };
		fwrite(offset, sizeof(offset), 1, fOut);
	}

	__int16 w = width - 1, h = height - 1;
	fwrite(&w, sizeof(w), 1, fOut);
	fwrite(&h, sizeof(h), 1, fOut);
	int count = width * height;
	fwrite(m_data.data(), count, 1, fOut);

}

bool Texture::Save(const char* fileName) const
{
	CREATE_OR_RETURN(fileName, nullptr);
	Write(fOut);
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
	offsetX = offsetY = 0;

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
	offsetX = offsetY = 0;

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


#pragma pack(push, 1) // Pack the struct tightly to ensure correct header size
struct TGAHeader {
	uint8_t idLength = 0;
	uint8_t colorMapType = 0;
	uint8_t imageType = 2;
	uint16_t colorMapOrigin = 0;
	uint16_t colorMapLength = 0;
	uint8_t colorMapDepth = 0;
	uint16_t xOrigin = 0;
	uint16_t yOrigin = 0;
	uint16_t imageWidth = 0;
	uint16_t imageHeight = 0;
	uint8_t pixelDepth = 24;
	uint8_t imageDescriptor = 0x20;
};
#pragma pack(pop)

enum class TGAImageType: uint8_t
{
	NoImageData = 0,
	ColorMappedImage = 1,
	TrueColorImage = 2,
	GrayscaleImage = 3,
	RLEColorMappedImage = 9,
	RLETrueColorImage = 10,
	RLEGrayscaleImage = 11
};

bool Texture::SaveTGA(const char* outFile, const Palette& palette) const
{
	CREATE_OR_RETURN(outFile, nullptr);

	TGAHeader header;
	header.xOrigin = offsetX;
	header.yOrigin = height - 1 - offsetY;
	header.imageWidth = width;
	header.imageHeight = height;

	fwrite(&header, sizeof(header), 1, fOut);

	const uint8_t* data = m_data.data();
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			uint8_t index = data[y * width + x];
			const Palette::PaletteEntry& pal = palette.m_palette[index];

			// swap RB
			fwrite(&pal.b, 1, 1, fOut);
			fwrite(&pal.g, 1, 1, fOut);
			fwrite(&pal.r, 1, 1, fOut);
		}
	}

	fclose(fOut);
	return true;
}

bool Texture::LoadTGA(const char* fileName, const Palette& palette, bool retainOffsets)
{
	OPEN_OR_RETURN(fileName, nullptr);

	TGAHeader header;
	FileUtils::FileRead(fIn, &header, sizeof(header));
	TGAImageType imageType = static_cast<TGAImageType>(header.imageType);
	switch (imageType)
	{
	case TGAImageType::TrueColorImage:
	case TGAImageType::GrayscaleImage:
		break; // these formats are OK and handled below
	case TGAImageType::RLETrueColorImage:
	case TGAImageType::RLEGrayscaleImage:
		cerr << "Compressed TGA formats are no supported. Please re-save without RLE. File '" << fileName << "'" << endl;
		return false;
	case TGAImageType::ColorMappedImage:
	case TGAImageType::RLEColorMappedImage:
		cerr << "Paletted TGA formats are no supported. Please re-save as True Color image. File '" << fileName << "'" << endl;
		return false;
	case TGAImageType::NoImageData:
	default:
		cerr << "Invalid Image type " << static_cast<int>(imageType) << " in .TGA file '" << fileName << "'" << endl;
		return false;

	}
	width = header.imageWidth;
	height = header.imageHeight;
	if (!retainOffsets)
	{
		offsetX = header.xOrigin;
		offsetY = height - 1 - header.yOrigin;
	}
	bool flippedVert = (header.imageDescriptor & 0x20) == 0;
	int count = width * height;
	m_data.resize(count);
	uint8_t* data = m_data.data();
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			uint8_t& index = flippedVert ? data[(height - 1 - y) * width + x] : data[y * width + x];
			const Palette::PaletteEntry& pal = palette.m_palette[index];

			uint8_t R, G, B, A;
			if (imageType == TGAImageType::GrayscaleImage)
			{
				fread(&R, 1, 1, fIn);
				G = B = R;
			}
			else
			{
				// swap RB
				fread(&B, 1, 1, fIn);
				fread(&G, 1, 1, fIn);
				fread(&R, 1, 1, fIn);
				if (header.pixelDepth == 32)
				{
					fread(&A, 1, 1, fIn);
				}
			}
			index = palette.FindBestEntry(R, G, B);
		}
	}

	fclose(fIn);

	return true;
}

