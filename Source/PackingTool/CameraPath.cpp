#include "CameraPath.h"
#include "FileUtils.h"

#include <cassert>
#include <iostream>
#include <iosfwd>


using namespace std;

bool CameraPath::Load(const char* fileName)
{
	OPEN_OR_RETURN(fileName, nullptr);
	int size = FileUtils::GetFileSize(fIn) - sizeof(FileHeader);

	if (size % sizeof(KeyFrame) != 0)
	{
		cerr << "File '" << fileName << "' is not a valid path file." << endl;
		return false;
	}

	FileHeader header;
	FileUtils::FileRead(fIn, &header, sizeof(FileHeader));

	m_path.resize(size / sizeof(KeyFrame));
	FileUtils::FileRead(fIn, m_path.data(), size);
	fclose(fIn);

	/*
	// Just some tests:
	for (const KeyFrame& kf : m_path)
	{
		if (kf.actionBits != 0 && kf.actionBits != 16 && kf.actionBits != 32 && kf.actionBits != 64)
		{
			return false;
		}
	}
	*/
	return true;
}

bool CameraPath::SaveCSV(const char* fileName) const
{
	CREATE_OR_RETURN(fileName, nullptr);

	std::ostringstream oss;
	const char* sep = "; ";
	for (const KeyFrame& kf : m_path)
	{
		oss << (int)kf.position[0] << sep << (int)kf.position[1] << sep << (int)kf.angle << sep << (int)kf.actionBits << endl;
	}
	string s = oss.str();
	fwrite(s.data(), s.size(), 1, fOut);
	fclose(fOut);
	return true;
}
