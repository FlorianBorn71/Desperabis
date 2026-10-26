#include "GameContext.h"
#include "GameMath.h"

#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        int TileCoordinate(std::int16_t coordinate)
        {
            const int tile = coordinate / 100;
            if (tile < -128 || tile > 127)
            {
                throw std::overflow_error("Lighting lookup would overflow the DOS signed-byte division.");
            }
            return tile;
        }
    }

    void LightingCache::Reset(std::int16_t x, std::int16_t z, std::uint8_t radius)
    {
        if (radius > MaxRadius)
        {
            throw std::out_of_range("Lighting radius exceeds the 21x21 cache.");
        }
        TileCoordinate(x);
        TileCoordinate(z);
        centerX = x;
        centerZ = z;
        activeRadius = radius;
        contributions = {};
    }

    bool LightingCache::AddAtWorldXZ(std::int16_t x, std::int16_t z, std::int16_t amount)
    {
        if (activeRadius > MaxRadius)
        {
            throw std::out_of_range("Lighting radius exceeds the 21x21 cache.");
        }
        const int dx = TileCoordinate(WrapSignedWord(static_cast<int>(x) + 50)) - TileCoordinate(centerX);
        const int dz = TileCoordinate(WrapSignedWord(static_cast<int>(z) + 50)) - TileCoordinate(centerZ);
        if (dx < -activeRadius || dx > activeRadius || dz < -activeRadius || dz > activeRadius)
        {
            return false;
        }
        auto& contribution = contributions[static_cast<std::size_t>(dx + MaxRadius)]
            [static_cast<std::size_t>(dz + MaxRadius)];
        contribution = WrapSignedWord(static_cast<int>(contribution) + amount);
        return true;
    }
}
