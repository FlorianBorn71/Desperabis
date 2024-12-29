#pragma once
#include <DesperabisCommon.h>
#include <Asset/AssetBase.h>
#include <Asset/ModelGeometry.h>

class D_IMPEXP Palette : public AssetBase
{
public:
    Palette() = default;
    virtual ~Palette() = default;

    virtual void Read(AssetStream& stream);

	inline Color4 GetEntry(int idx) const {
        return m_palette[idx];
	}

	inline Color4 GetEntryCustomAlpha(int idx, uint8_t alpha=0) const {
		Color4 col = m_palette[idx];
		col.a = alpha;
		return col;
	}

protected:
    Color4 m_palette[256] = {};
};

