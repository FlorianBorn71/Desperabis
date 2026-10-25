#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        struct HeightPosition
        {
            std::size_t column;
            std::size_t row;
            std::uint16_t fractionX;
            std::uint16_t fractionZ;
        };

        HeightPosition GetHeightPosition(std::int16_t x, std::int16_t z)
        {
            const int column = x / 100;
            const int zTile = z / 100;
            if (column < -128 || column > 127 || zTile < -128 || zTile > 127)
            {
                throw std::overflow_error("Height lookup would overflow the DOS signed-byte division.");
            }
            const int row = 41 - zTile;
            if (column < 1 || column > static_cast<int>(Level::Width) ||
                row < 1 || row > static_cast<int>(Level::Height))
            {
                throw std::out_of_range("Height lookup is outside the 50x40 level.");
            }
            // The original arrays are one-based; their instruction displacements are biased by 51.
            return { static_cast<std::size_t>(column - 1), static_cast<std::size_t>(row - 1),
                static_cast<std::uint16_t>((x % 100) * 661),
                static_cast<std::uint16_t>((z % 100) * 661) };
        }

        void CheckHeightCorners(const HeightPosition& position)
        {
            if (position.column + 1 >= Level::Width || position.row + 1 >= Level::Height)
            {
                throw std::out_of_range("Height interpolation requires four samples inside the level.");
            }
        }

        std::uint16_t Interpolate(std::uint16_t first, std::uint16_t second, std::uint16_t fraction)
        {
            const auto sum = static_cast<std::uint32_t>(first) * (65535u - fraction) +
                static_cast<std::uint32_t>(second) * fraction;
            return static_cast<std::uint16_t>(sum >> 16);
        }

        std::uint16_t InterpolateHeight(const std::array<std::int16_t, 4>& corners,
            const HeightPosition& position)
        {
            const auto upper = Interpolate(static_cast<std::uint16_t>(corners[0]),
                static_cast<std::uint16_t>(corners[1]), position.fractionX);
            const auto lower = Interpolate(static_cast<std::uint16_t>(corners[2]),
                static_cast<std::uint16_t>(corners[3]), position.fractionX);
            return Interpolate(lower, upper, position.fractionZ);
        }

        std::array<std::int16_t, 4> GetCorners(
            const std::uint8_t (&plane)[Level::Height][Level::Width], const HeightPosition& position)
        {
            CheckHeightCorners(position);
            const auto row = position.row;
            const auto column = position.column;
            return { plane[row][column], plane[row][column + 1],
                plane[row + 1][column], plane[row + 1][column + 1] };
        }

        std::int16_t RepeatFloor(std::int16_t corner, std::int16_t queryHeight, std::int16_t step)
        {
            corner = WrapSignedWord(corner % step - step);
            const auto threshold = WrapSignedWord(queryHeight - 28);
            for (unsigned int iterations = 0; threshold > corner; ++iterations)
            {
                if (iterations == 65536u)
                {
                    throw std::runtime_error("Repeated-floor lookup would loop forever in the DOS game.");
                }
                corner = WrapSignedWord(corner + step);
            }
            return std::clamp<std::int16_t>(corner, 0, 255);
        }
    }

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

    std::int16_t GameState::GetFloorHeight(std::int16_t x, std::int16_t y, std::int16_t z)
    {
        if (!level) { throw std::runtime_error("GetFloorHeight requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        const auto surface = level->surfaceFlags[position.row][position.column] & 31;
        std::int16_t depth = 1000;
        std::int16_t value = -1;
        if (surface >= 1 && surface <= static_cast<int>(level->heightSurfaces.size()))
        {
            const auto& entry = level->heightSurfaces[surface - 1];
            if (!entry)
            {
                throw std::runtime_error("Floor surface slot requires height/value metadata from level information.");
            }
            depth = entry->depth;
            value = entry->value;
        }
        floorSurfaceValue = value;
        const auto flatFloor = WrapSignedWord(-static_cast<std::int32_t>(depth));
        if (flatFloor >= -900) { return flatFloor; }

        auto corners = GetCorners(level->floorHeights, position);
        const auto halfY = y >= 0 ? y / 2 : (static_cast<int>(y) - 1) / 2;
        const auto queryHeight = WrapSignedWord(-halfY);
        constexpr int stackedFloorSurface = 28;
        if (surface == stackedFloorSurface)
        {
            const auto room = GetCorners(level->roomHeights, position);
            for (std::size_t i = 0; i < corners.size(); ++i)
            {
                corners[i] = WrapSignedWord(corners[i] - room[i]);
            }
            if (WrapSignedWord(*std::max_element(corners.begin(), corners.end()) - 5) < queryHeight)
            {
                for (std::size_t i = 0; i < corners.size(); ++i)
                {
                    corners[i] = WrapSignedWord(corners[i] + room[i]);
                }
            }
        }
        if (floorRepeatHeight > 0 && surface != 0)
        {
            for (auto& corner : corners) { corner = RepeatFloor(corner, queryHeight, floorRepeatHeight); }
        }
        return WrapSignedWord(-2 * static_cast<std::int32_t>(InterpolateHeight(corners, position)));
    }

    std::int16_t GameState::GetRoomHeight(std::int16_t x, std::int16_t z) const
    {
        if (!level) { throw std::runtime_error("GetRoomHeight requires level geometry."); }
        const auto position = GetHeightPosition(x, z);
        if (level->ceilingObjects[position.row][position.column] == 0) { return 20000; }
        const auto corners = GetCorners(level->roomHeights, position);
        return WrapSignedWord(2 * static_cast<std::int32_t>(InterpolateHeight(corners, position)));
    }
}
