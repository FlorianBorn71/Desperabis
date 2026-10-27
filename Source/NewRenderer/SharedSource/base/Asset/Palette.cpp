#include "pch.h"
#include <Asset/Palette.h>
#include <iosfwd>
#include <stdexcept>

void Palette::BuildGrayscale(int maxIndex)
{
    if (maxIndex < 1 || maxIndex > 255)
    {
        throw std::invalid_argument("Grayscale palette maximum index must be between 1 and 255.");
    }
    for (int i = 0; i < 256; ++i)
    {
        const byte intensity = static_cast<byte>(std::min(i, maxIndex) * 255 / maxIndex);
        m_palette[i].Set(intensity, intensity, intensity, i == 0 ? 0 : 255);
    }
}

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
