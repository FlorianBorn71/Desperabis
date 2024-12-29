#pragma once
#include <DesperabisCommon.h>

struct RawGameObjectData
{
	union
	{
		byte m_rawBytes[54];
		int16_t m_rawShorts[27];
		uint16_t m_rawUShorts[27];
	};

	RawGameObjectData& operator = (const RawGameObjectData& other) {
		memcpy(m_rawBytes, other.m_rawBytes, sizeof(m_rawBytes));
		return *this;
	}
	bool operator == (const RawGameObjectData& other) const {
        return memcmp(m_rawBytes, other.m_rawBytes, sizeof(m_rawBytes)) == 0;
    }
	bool operator != (const RawGameObjectData& other) const {
		return !(*this == other);
	}

};

struct RawGameObject : RawGameObjectData
{
    class Level* m_level;
    int m_objectNumber;

	RawGameObject& operator = (const RawGameObjectData& other) {
		memcpy(m_rawBytes, other.m_rawBytes, sizeof(m_rawBytes));
		return *this;
	}
	bool operator == (const RawGameObject& other) const {
		return memcmp(m_rawBytes, other.m_rawBytes, sizeof(m_rawBytes)) == 0;
	}
	bool operator != (const RawGameObject& other) const {
		return !(*this == other);
	}

	/*
    union
    {
        byte m_rawBytes[54];
        int16_t m_rawShorts[27];
        uint16_t m_rawUShorts[27];
    };
	*/
};
