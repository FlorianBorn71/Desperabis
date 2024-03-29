#pragma once

#include <vector>
#include <string>


class DatFile
{
public:
	DatFile(std::string_view name, const std::string_view* fileList, size_t numFiles);

	bool PackageAndSave(std::string* out_path = nullptr) const;
	std::string_view GetFileAtOffset(int ofs, int& ofsWithin) const;

	static const DatFile Game;
	static const DatFile Sounds;
	static const DatFile Music;

	static std::string BuildFileList(const char* DATFile, const char* postFix);

private:
	std::string m_outName;
	const std::string_view* m_fileList;
	int m_numFiles;

	struct DirEntry
	{
		// hardcoded block:
		unsigned char m_length;
		char m_name[19];
		int m_fileOfs = 0;
		int m_fileLength;

		// additional data

		std::string_view Filename() const { return std::string_view(m_name, m_length); }

		void SetName(std::string_view n);

		void ReadEntry(FILE* fIn, __int16 magicXor);
		void WriteEntry(FILE* fOut, __int16 magicXor) const;


		bool LoadContent();
		void WriteContent(FILE* fOut) const;
		bool HasExtension(const char* ext) const;
		bool HasName(const char* name) const;

		std::vector<char> m_content;
	};

	static bool ExploreFilesInDAT(const char* fileIn, std::vector<DatFile::DirEntry>& entries);

};

