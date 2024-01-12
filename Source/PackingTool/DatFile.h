#pragma once

#include <vector>
#include <string>


class DatFile
{
public:
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

};

