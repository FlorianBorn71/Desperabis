#include "GameContext.h"

#include <cstring>
#include <stdexcept>

namespace Desperabis
{
    void Level::LoadGeometry(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size < GeometryStreamSize)
        {
            throw std::invalid_argument("Geometry input requires a header and seven complete 50x40 byte planes.");
        }
        const auto widthMinusOne = data[0] | (static_cast<unsigned int>(data[1]) << 8);
        const auto heightMinusOne = data[2] | (static_cast<unsigned int>(data[3]) << 8);
        if (widthMinusOne != Width - 1 || heightMinusOne != Height - 1)
        {
            throw std::invalid_argument("Native gameplay height lookups require 50x40 level geometry.");
        }
        constexpr auto planeSize = Width * Height;
        std::memcpy(ceilingObjects, data + 4 + 2 * planeSize, planeSize);
        std::memcpy(floorHeights, data + 4 + 3 * planeSize, planeSize);
        std::memcpy(roomHeights, data + 4 + 4 * planeSize, planeSize);
        std::memcpy(surfaceFlags, data + 4 + 5 * planeSize, planeSize);
    }
}
