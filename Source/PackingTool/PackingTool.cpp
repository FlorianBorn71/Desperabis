// PackingTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <format>
#include <vector>
#include <cassert>
#include <sstream>
#include <direct.h>

using namespace std;

static const __int16 g_MagicXor = 1111;
static const std::string_view g_Filenames[] = {
	"GRAFIK\\TUR6.256",
	"GRAFIK\\TUR61.256",
	"GRAFIK\\TUR8.256",
	"GRAFIK\\TUR21.256",
	"GRAFIK\\TUR4.256",
	"GRAFIK\\TUR5.256",
	"GRAFIK\\TUR10.256",
	"GRAFIK\\TUR3.256",
	"GRAFIK\\TUR9.256",
	"GRAFIK\\TUR11.256",
	"GRAFIK\\TUR14.256",
	"GRAFIK\\TUR12.256",
	"GRAFIK\\TUR13.256",
	"GRAFIK\\TUR16.256",
	"GRAFIK\\TUR19.256",
	"GRAFIK\\TUR7.256",
	"GRAFIK\\TUR15.256",
	"GRAFIK\\TUR28.256",
	"GRAFIK\\TUR2.256",
	"GRAFIK\\TUR60.256",
	"GRAFIK\\TUR17.256",
	"GRAFIK\\TUR22.256",
	"GRAFIK\\TUR23.256",
	"GRAFIK\\TUR1.256",
	"GRAFIK\\TUR18.256",
	"GRAFIK\\TUR26.256",
	"GRAFIK\\TUR27.256",
	"GRAFIK\\TUR24.256",
	"GRAFIK\\TUR20.256",
	"GRAFIK\\TUR29.256",
	"GRAFIK\\TUR25.256",
	"GRAFIK\\TUR30.256",
	"GRAFIK\\TUR31.256",
	"GRAFIK\\TUR32.256",
	"GRAFIK\\TUR33.256",
	"GRAFIK\\TUR44.256",
	"GRAFIK\\TUR46.256",
	"GRAFIK\\TUR34.256",
	"GRAFIK\\TUR35.256",
	"GRAFIK\\TUR36.256",
	"GRAFIK\\TUR37.256",
	"GRAFIK\\TUR39.256",
	"GRAFIK\\TUR43.256",
	"GRAFIK\\TUR38.256",
	"GRAFIK\\TUR40.256",
	"GRAFIK\\TUR41.256",
	"GRAFIK\\TUR42.256",
	"GRAFIK\\TUR62.256",
	"GRAFIK\\TUR47.256",
	"GRAFIK\\TUR48.256",
	"GRAFIK\\TUR45.256",
	"GRAFIK\\TUR49.256",
	"GRAFIK\\TUR50.256",
	"GRAFIK\\TUR51.256",
	"GRAFIK\\TUR52.256",
	"GRAFIK\\TUR53.256",
	"GRAFIK\\TUR54.256",
	"GRAFIK\\TUR55.256",
	"GRAFIK\\TUR56.256",
	"GRAFIK\\TUR57.256",
	"ANIMS\\SCHRIFT2.AN2",
	"ANIMS\\AMA_ANI.AN2",
	"ANIMS\\SCHRIFT0.AN2",
	"ANIMS\\SCHRIFT1.AN2",
	"ANIMS\\ANIM2.AN2",
	"ANIMS\\ANIM5.AN2",
	"ANIMS\\ANIM9.AN2",
	"ANIMS\\ANIM4.AN2",
	"ANIMS\\SCHUSS1.AN2",
	"ANIMS\\ANIM3.AN2",
	"ANIMS\\FACKEL1.AN2",
	"ANIMS\\FACKEL4.AN2",
	"ANIMS\\VORSPANN.AN2",
	"ANIMS\\ANIM1.AN2",
	"ANIMS\\FACKEL2.AN2",
	"ANIMS\\FACKEL5.AN2",
	"ANIMS\\ANIM8.AN2",
	"ANIMS\\ANIM7.AN2",
	"ANIMS\\FACKEL3.AN2",
	"ANIMS\\WAFFEN.AN2",
	"ANIMS\\CURSOR.AN2",
	"ANIMS\\FACKEL6.AN2",
	"ANIMS\\TXTBILD.AN2",
	"ANIMS\\ANIM6.AN2",
	"ANIMS\\ANIM11.AN2",
	"ANIMS\\ANIM12.AN2",
	"ANIMS\\ANIM10.AN2",
	"ANIMS\\FACKEL7.AN2",
	"ANIMS\\EDITOR.AN2",
	"ANIMS\\ANIM13.AN2",
	"ANIMS\\ANIM14.AN2",
	"VEKT\\VEKT18.3DA",
	"VEKT\\VEKT14.3DA",
	"VEKT\\VEKT4.3DA",
	"VEKT\\VEKT5.3DA",
	"VEKT\\VEKT20.3DA",
	"VEKT\\VEKT7.3DA",
	"VEKT\\VEKT13.3DA",
	"VEKT\\VEKT10.3DA",
	"VEKT\\VEKT9.3DA",
	"VEKT\\VEKT15.3DA",
	"VEKT\\VEKT16.3DA",
	"VEKT\\VEKT39.3DA",
	"VEKT\\VEKT30.3DA",
	"VEKT\\VEKT32.3DA",
	"VEKT\\VEKT25.3DA",
	"VEKT\\VEKT27.3DA",
	"VEKT\\VEKT29.3DA",
	"VEKT\\VEKT35.3DA",
	"VEKT\\VEKT37.3DA",
	"VEKT\\VEKT1.3DA",
	"VEKT\\VEKT45.3DA",
	"VEKT\\VEKT46.3DA",
	"VEKT\\VEKT2.3DA",
	"VEKT\\VEKT48.3DA",
	"VEKT\\VEKT50.3DA",
	"VEKT\\VEKT3.3DA",
	"VEKT\\VEKT53.3DA",
	"VEKT\\VEKT55.3DA",
	"VEKT\\VEKT57.3DA",
	"VEKT\\VEKT59.3DA",
	"VEKT\\VEKT61.3DA",
	"VEKT\\VEKT63.3DA",
	"VEKT\\VEKT66.3DA",
	"VEKT\\VEKT68.3DA",
	"VEKT\\VEKT6.3DA",
	"VEKT\\VEKT8.3DA",
	"VEKT\\VEKT11.3DA",
	"VEKT\\VEKT12.3DA",
	"VEKT\\VEKT24.3DA",
	"VEKT\\VEKT19.3DA",
	"VEKT\\VEKT17.3DA",
	"VEKT\\VEKT21.3DA",
	"VEKT\\VEKT22.3DA",
	"VEKT\\VEKT23.3DA",
	"VEKT\\VEKT26.3DA",
	"VEKT\\VEKT28.3DA",
	"VEKT\\VEKT31.3DA",
	"VEKT\\VEKT33.3DA",
	"VEKT\\VEKT38.3DA",
	"VEKT\\VEKT34.3DA",
	"VEKT\\VEKT40.3DA",
	"VEKT\\VEKT36.3DA",
	"VEKT\\VEKT41.3DA",
	"VEKT\\VEKT42.3DA",
	"VEKT\\VEKT43.3DA",
	"VEKT\\VEKT44.3DA",
	"VEKT\\VEKT47.3DA",
	"VEKT\\VEKT49.3DA",
	"VEKT\\VEKT51.3DA",
	"VEKT\\VEKT52.3DA",
	"VEKT\\VEKT54.3DA",
	"VEKT\\VEKT56.3DA",
	"VEKT\\VEKT58.3DA",
	"VEKT\\VEKT60.3DA",
	"VEKT\\VEKT62.3DA",
	"VEKT\\VEKT64.3DA",
	"VEKT\\VEKT65.3DA",
	"VEKT\\VEKT67.3DA",
	"VEKT\\VEKT69.3DA",
	"VEKT\\VEKT70.3DA",
	"VEKT\\VEKT71.3DA",
	"VEKT\\VEKT72.3DA",
	"LEVEL\\WŽNDE.AN2",
	"LEVEL\\STARTMSG.TXT",
	"LEVEL\\ENDMSG.TXT",
	"LEVEL\\BEZEICH.TXT",
	"LEVEL\\TEXTE.TXT",
	"LEVEL\\LEVELS.INF",
	"LEVEL\\FELD0.256",
	"LEVEL\\OPSET13.256",
	"LEVEL\\FELD96.256",
	"LEVEL\\FELD10.256",
	"LEVEL\\FELD17.256",
	"LEVEL\\FELD9.256",
	"LEVEL\\FELD11.256",
	"LEVEL\\FELD12.256",
	"LEVEL\\FELD13.256",
	"LEVEL\\FELD14.256",
	"LEVEL\\FELD15.256",
	"LEVEL\\OPSET14.256",
	"LEVEL\\OPREDU.256",
	"LEVEL\\OPTRANS.256",
	"LEVEL\\OPSET15.256",
	"LEVEL\\FELD24.256",
	"LEVEL\\FELD97.256",
	"LEVEL\\OPSET10.256",
	"LEVEL\\OPSET11.256",
	"LEVEL\\OPSET0.256",
	"LEVEL\\OPSET1.256",
	"LEVEL\\OPSET2.256",
	"LEVEL\\OPSET3.256",
	"LEVEL\\OPSET4.256",
	"LEVEL\\OPSET5.256",
	"LEVEL\\OPSET6.256",
	"LEVEL\\OPSET7.256",
	"LEVEL\\OPSET8.256",
	"LEVEL\\OPSET9.256",
	"LEVEL\\OPSET12.256",
	"LEVEL\\FELD16.256",
	"LEVEL\\OPSET16.256",
	"LEVEL\\OPSET17.256",
	"LEVEL\\FELD21.256",
	"LEVEL\\FELD99.256",
	"LEVEL\\FELD18.256",
	"LEVEL\\FELD98.256",
	"LEVEL\\OPGSET.256",
	"LEVEL\\FELD19.256",
	"LEVEL\\FELD20.256",
	"LEVEL\\OPSET19.256",
	"LEVEL\\FELD22.256",
	"LEVEL\\FELD26.256",
	"LEVEL\\FELD23.256",
	"LEVEL\\FELD25.256",
	"LEVEL\\FELD51.256",
	"LEVEL\\FELD95.256",
	"LEVEL\\OPSET18.256",
	"LEVEL\\FELD50.256",
	"LEVEL\\FELD30.256",
	"LEVEL\\FELD31.256",
	"LEVEL\\FELD32.256",
	"LEVEL\\FELD33.256",
	"LEVEL\\FELD34.256",
	"LEVEL\\FELD35.256",
	"LEVEL\\FELD90.256",
	"LEVEL\\OPSET20.256",
	"LEVEL\\FELD36.256",
	"LEVEL\\OPSET21.256",
	"LEVEL\\FELD91.256",
	"LEVEL\\FELD29.256",
	"LEVEL\\FELD60.256",
	"LEVEL\\FELD92.256",
	"LEVEL\\OBJPOS0.DAT",
	"LEVEL\\OBJPOS9.DAT",
	"LEVEL\\OBJPOS10.DAT",
	"LEVEL\\OBJPOS11.DAT",
	"LEVEL\\OBJPOS12.DAT",
	"LEVEL\\OBJPOS13.DAT",
	"LEVEL\\OBJPOS14.DAT",
	"LEVEL\\OBJPOS15.DAT",
	"LEVEL\\OBJPOS17.DAT",
	"LEVEL\\OBJPOS16.DAT",
	"LEVEL\\OBJPOS96.DAT",
	"LEVEL\\OBJPOS21.DAT",
	"LEVEL\\OBJPOS99.DAT",
	"LEVEL\\OBJPOS18.DAT",
	"LEVEL\\OBJPOS98.DAT",
	"LEVEL\\OBJPOS19.DAT",
	"LEVEL\\OBJPOS20.DAT",
	"LEVEL\\OBJPOS97.DAT",
	"LEVEL\\OBJPOS22.DAT",
	"LEVEL\\OBJPOS23.DAT",
	"LEVEL\\OBJPOS24.DAT",
	"LEVEL\\OBJPOS25.DAT",
	"LEVEL\\OBJPOS26.DAT",
	"LEVEL\\OBJPOS95.DAT",
	"LEVEL\\OBJPOS51.DAT",
	"LEVEL\\OBJPOS50.DAT",
	"LEVEL\\OBJPOS30.DAT",
	"LEVEL\\OBJPOS31.DAT",
	"LEVEL\\OBJPOS32.DAT",
	"LEVEL\\OBJPOS33.DAT",
	"LEVEL\\OBJPOS34.DAT",
	"LEVEL\\OBJPOS35.DAT",
	"LEVEL\\OBJPOS90.DAT",
	"LEVEL\\OBJPOS36.DAT",
	"LEVEL\\OBJPOS91.DAT",
	"LEVEL\\OBJPOS29.DAT",
	"LEVEL\\OBJPOS60.DAT",
	"LEVEL\\OBJPOS92.DAT",
	"LEVEL\\DEMO1.KOO",
	"LEVEL\\DEMO0.KOO",
	"LEVEL\\DEMO5.KOO",
	"LEVEL\\DEMO2.KOO",
	"LEVEL\\DEMO3.KOO",
	"LEVEL\\DEMO4.KOO",
	"LEVEL\\DEMO6.KOO",
	"LEVEL\\DEMO10.KOO",
	"LEVEL\\DEMO7.KOO",
	"GRAFIK\\BACKGR1.PAL",
	"GRAFIK\\BACKGR2.PAL",
	"GRAFIK\\BACKGR3.PAL",
	"GRAFIK\\BACKGR4.PAL",
	"GRAFIK\\BACKGR5.PAL",
	"GRAFIK\\BACKGR6.PAL",
	"GRAFIK\\BACKGR1.256",
	"GRAFIK\\BACKGR2.256",
	"GRAFIK\\BACKGR3.256",
	"GRAFIK\\BACKGR4.256",
	"GRAFIK\\BACKGR5.256",
	"GRAFIK\\BACKGR6.256",
	"GRAFIK\\TASTE.256",
	"GRAFIK\\HERZ.256",
};

static const __int16 g_NumFiles = static_cast<__int16>(std::size(g_Filenames));
static_assert(g_NumFiles == 293);
static const int g_DirEntrySize = 28;
static const int g_initialFileOfs = 4 + g_DirEntrySize * g_NumFiles;
static_assert(g_initialFileOfs == 8208);

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

struct DirEntry
{
	// hardcoded block:
	unsigned char m_length;
	char m_name[19];
	int m_fileOfs = 0;
	int m_fileLength;

	// additional data

	string_view Filename() const { return string_view(m_name, m_length); }

	void SetName(string_view n)
	{
		m_length = static_cast<unsigned char>(n.length());
		memset(m_name, 0, sizeof(m_name));
		memcpy(m_name, n.data(), m_length);
	}

	void ReadEntry(FILE* fIn, __int16 magicXor)
	{
		FileRead(fIn, this, g_DirEntrySize);

		unsigned __int16* n2 = (unsigned __int16*)this;

		for (int x = 0; x < g_DirEntrySize / 2; x++)
			n2[x] = n2[x] ^ magicXor;

		assert(m_length < 19);
		m_name[m_length] = 0;
	}

	void WriteEntry(FILE* fOut, __int16 magicXor) const
	{
		__int16 temp[g_DirEntrySize / 2];
		memcpy(temp, this, g_DirEntrySize);
		for (int x = 0; x < g_DirEntrySize / 2; x++)
			temp[x] = temp[x] ^ magicXor;

		fwrite(temp, g_DirEntrySize, 1, fOut);
	}


	bool LoadContent()
	{
		string n(Filename());
		FILE* fIn = fopen(n.c_str(), "rb");
		if (fIn == nullptr)
			return false;

		fseek(fIn, 0, SEEK_END);
		m_fileLength = ftell(fIn);
		fseek(fIn, 0, SEEK_SET);
		m_content.resize(m_fileLength);
		fread(m_content.data(), 1, m_fileLength, fIn);
		fclose(fIn);
		return true;
	}

	void WriteContent(FILE* fOut) const
	{
		fwrite(m_content.data(), m_fileLength, 1, fOut);
	}


	bool HasExtension(const char* ext) const
	{
		const char* extension = strchr(m_name, '.');
		assert(extension != nullptr);
		extension++;

		return _stricmp(extension, ext) == 0;
	}

	bool HasName(const char* name) const
	{
		return _stricmp(m_name, name) == 0;
	}

	vector<char> m_content;
};


struct Texture
{
	int width = 0, height = 0;
	vector<unsigned char> m_data;
	bool hasAlpha = false;

	void Read(FILE* fIn)
	{
		__int16 w, h;
		FileRead(fIn, &w, sizeof(w));
		FileRead(fIn, &h, sizeof(h));
		assert(w < 512 && h < 512);

		width = w + 1;
		height = h + 1;
		int count = width * height;
		m_data.resize(count);
		FileRead(fIn, m_data.data(), count);
	}

	void Load(const char* fileName)
	{
		FILE* fIn = fopen(fileName, "rb");
		assert(fIn != nullptr);
		Read(fIn);
		fclose(fIn);
	}

	bool Save(const char* fileName) const
	{
		FILE* fOut = fopen(fileName, "wb");
		if (fOut == nullptr)
		{
			assert(fOut != nullptr);
			return false;
		}
		__int16 w = width - 1, h = height - 1;
		fwrite(&w, sizeof(w), 1, fOut);
		fwrite(&h, sizeof(h), 1, fOut);
		int count = width * height;
		fwrite(m_data.data(), count, 1, fOut);

		fclose(fOut);
		return true;
	}


	bool ToText(const char* outFile) const
	{
		FILE* fOut = fopen(outFile, "wb");
		if (fOut == nullptr)
		{
			assert(fOut != nullptr);
			return false;
		}

		std::ostringstream oss;
		const unsigned char* ptr = m_data.data();
		for (int h = 0; h < height; h++, ptr += width)
		{
			size_t len = static_cast<size_t>(ptr[0]);
			if (len >= width)
				return false;
			const char* str = (const char*)&ptr[1];
			while (len > 0 && str[len-1] == 0) // happens for line 2 of tur38
				len--;
			string_view s(str, len);
			oss << s << std::endl;
		}

		string buf = oss.str();
		fwrite(buf.data(), buf.length(), 1, fOut);

		fclose(fOut);
		return true;
	}


	struct TextLine
	{
		unsigned char m_length = 0;
		char m_buffer[1023];
	};

	bool FromText(const char* fileName)
	{
		FILE* fIn = fopen(fileName, "rb");
		if (fIn == nullptr)
		{
			assert(fIn != nullptr);
			return false;
		}

		height = width = 0;

		char buffer[1024];
		vector<TextLine> lines;
		TextLine line;
		while (fgets(buffer, sizeof(buffer), fIn) != nullptr) {
			int len = static_cast<int>(strlen(buffer));
			assert(len > 0);
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

};


void ExploreFilesInDAT(vector<DirEntry>& entries, const char* fileIn)
{
	FILE* fIn = fopen(fileIn, "rb");
	if (fIn == nullptr)
		return;

	__int16 magicXor;
	FileRead(fIn, &magicXor, sizeof(magicXor));
	__int16 numFiles;
	FileRead(fIn, &numFiles, sizeof(numFiles));

	DirEntry entry;

	int firstEntry = (int)entries.size();
	for (int i = 0; i < numFiles; i++)
	{
		entry.ReadEntry(fIn, magicXor);
		entries.push_back(entry);
	}

	fclose(fIn);
}

void BuildFileList(vector<DirEntry>& entries, std::ostringstream& oss)
{
	oss << "static const std::string_view g_Filenames[] = {\n";
	for (const DirEntry& entry : entries)
	{
		std::string_view sw(entry.m_name, entry.m_length);
		oss << "    \"" << entry.Filename() << "\"," << std::endl;
	}
	oss << "};";
}

bool BuildDAT(const char* outFile)
{
	vector<DirEntry> entries;

	// load content of all files
	DirEntry entry = { 0 };
	entry.m_fileOfs = g_initialFileOfs;
	for (string_view name : g_Filenames)
	{
		entry.SetName(name);
		if (!entry.LoadContent())
		{
			cerr << "Failed to load file '" << name << "'" << endl;
			return false;
		}
		entries.push_back(entry);
		entry.m_fileOfs += entry.m_fileLength;
	}


	FILE* fOut = fopen(outFile, "wb");
	if (fOut == nullptr)
	{
		cerr << "Failed to open output file '" << outFile << "'" << endl;
		return false;
	}

	__int16 magicXor = g_MagicXor;
	fwrite(&magicXor, sizeof(magicXor), 1, fOut);
	__int16 numFiles = static_cast<__int16>(entries.size());
	fwrite(&numFiles, sizeof(numFiles), 1, fOut);

	for (const DirEntry& e : entries)
	{
		e.WriteEntry(fOut, magicXor);
	}

	for (const DirEntry& e : entries)
	{
		e.WriteContent(fOut);
	}

	fclose(fOut);
	return true;
}


void TexturesToText()
{
	Texture t;
//	t.Load("GRAFIK\\TUR29.256");
//	t.ToText("LEVEL\\Ingame29.txt");
	t.Load("GRAFIK\\TUR38.256");
	t.ToText("LEVEL\\Ingame38.txt");
	t.Load("GRAFIK\\TUR39.256");
	t.ToText("LEVEL\\Ingame39.txt");
}

void TextToTextures()
{
	Texture t;
	t.FromText("LEVEL\\Ingame38.txt");
	t.Save("GRAFIK\\TUR38.256");
	t.FromText("LEVEL\\Ingame39.txt");
	t.Save("GRAFIK\\TUR39.256");
}


int main(int argc, char* argv[])
{
	string sourceDir, destFile;

	if (argc <= 1)
	{
		std::cout << "Command line args: " << std::endl;
		std::cout << "  -s <folder> : source root folder (absolute path) under which the unpacked game assets are located" << std::endl;
		std::cout << "  -d <file> : absolute file path of the destination file (.DAT)." << std::endl;
		return 0;
	}
	else
	{
		std::cout << "Number of arguments: " << argc - 1 << std::endl;
		// Print each command line argument
		for (int i = 1; i < argc; ++i)
		{
			std::cout << "  Argument " << i << ": " << argv[i] << std::endl;
			if (_stricmp(argv[i], "-s") == 0 && i < argc - 1)
			{
				sourceDir = argv[i + 1];
			}
			if (_stricmp(argv[i], "-d") == 0 && i < argc - 1)
			{
				destFile = argv[i + 1];
			}
		}
	}


	if (destFile.empty())
	{
		destFile = "GAME.DAT"; // save to current cwd
	}
	// make it absolute
	char buf[4096];
	std::filesystem::path cwd = _getcwd(buf, sizeof(buf));
	std::filesystem::path outputPath = cwd / destFile;
	outputPath = outputPath.lexically_normal();
	destFile = outputPath.string();

	if (!sourceDir.empty())
	{
		if (_chdir(sourceDir.c_str()) != 0)
		{
			cerr << "Failed to set source directory." << endl;
			return 1;
		}
	}

	const bool loadOriginal = false;
	if (loadOriginal)
	{

		vector<DirEntry> entries;

		ExploreFilesInDAT(entries, "GAME.DAT.orig");
		std::ostringstream oss;
		BuildFileList(entries, oss);
		string s = oss.str();
		TexturesToText();
	}
	else
	{
		TextToTextures();

		if (!BuildDAT(destFile.c_str()))
		{
			cerr << "An error occurred during packaging." << endl;
			return 1;
		}
	}
	cout << "Packing file was successfully written to: " << destFile.c_str() << endl;
	return 0;
}
