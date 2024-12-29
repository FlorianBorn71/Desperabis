#include "pch.h"
#include <Asset/Palette.h>
#include <iosfwd>

void Palette::Read(AssetStream& s)
{
    for (int i = 0; i < 256; i++)
    {
        Color4& pal = m_palette[i];
        pal.r = (byte)((int)s.Read<byte>() * 255 / 63);
        pal.g = (byte)((int)s.Read<byte>() * 255 / 63);
        pal.b = (byte)((int)s.Read<byte>() * 255 / 63);
        pal.a = (i == 0) ? 0 : 255;
    }
}
