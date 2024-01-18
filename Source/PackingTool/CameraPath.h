#pragma once
#include <vector>


class CameraPath
{
public:
#pragma pack(push, 1) // Pack the struct tightly
	struct FileHeader
	{
		uint8_t levelIndex;
		//uint8_t value1, value2;
		uint16_t numberOfBytes;
	};

	struct KeyFrame
	{
		//int8_t padding;
		int16_t position[2];
		uint8_t angle, actionBits;
	};
#pragma pack(pop)
	//static_assert(sizeof(KeyFrame) == 18);

	bool Load(const char* fileName);
	bool Save(const char* fileName) const;
	bool SaveCSV(const char* fileName) const;

	std::vector<KeyFrame> m_path;
};

