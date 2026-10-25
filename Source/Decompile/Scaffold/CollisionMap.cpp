#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <new>
#include <stdexcept>

namespace Desperabis
{
    Level CreateDummyLevel()
    {
        constexpr std::size_t width = 200;
        constexpr std::size_t height = 200;
        static_assert(width * height + 4 == sizeof(CollisionMap));
        Level level;
        void* allocation = ::operator new(width * height + 4);
        level.collisionMap.reset(::new (allocation) CollisionMap{});
        level.collisionMap->widthMinusOne = static_cast<std::uint16_t>(width - 1);
        level.collisionMap->heightMinusOne = static_cast<std::uint16_t>(height - 1);
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                level.roomHeights[row][column] = 50;
                level.ceilingObjects[row][column] = 1;
            }
        }
        return level;
    }

    std::uint8_t GameState::GetCollisionMapPixelAtWorldXZ(std::int16_t x, std::int16_t z) const
    {
        if (!collisionMap)
        {
            throw std::runtime_error("GetCollisionMapPixelAtWorldXZ requires collision-map memory (DS:69C4).");
        }
        const auto column = WrapSignedWord(static_cast<std::int32_t>(x) - 112) / 25;
        const auto row = WrapSignedWord(162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25);
        if (column < 0 || column > 319 || row < 0 || row > 239)
        {
            return 0;
        }
        const auto& map = *collisionMap;
        const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
        const auto address = static_cast<std::uint16_t>(offsetof(CollisionMap, data) + row * stride + column);
        if (address >= sizeof(CollisionMap))
        {
            throw std::out_of_range("Collision-map read is outside the allocated dummy map.");
        }
        // Wrapped offsets can address the header as well as pixel data.
        const auto* bytes = reinterpret_cast<const unsigned char*>(&map);
        return bytes[address];
    }

    void GameState::SetCollisionMapPixelAtWorldXZ(
        std::int16_t x, std::int16_t z, std::uint16_t value)
    {
        if (!collisionMap)
        {
            throw std::runtime_error("SetCollisionMapPixelAtWorldXZ requires collision-map memory (DS:69C4).");
        }
        auto& map = *collisionMap;
        const auto stride = static_cast<std::uint16_t>(map.widthMinusOne + 1u);
        const auto column = WrapSignedWord(static_cast<std::int32_t>(x) - 112) / 25;
        const auto row = WrapSignedWord(162 - WrapSignedWord(static_cast<std::int32_t>(z) - 38) / 25);
        const auto index = static_cast<std::uint16_t>(row * stride + column);
        constexpr auto dataOffset = offsetof(CollisionMap, data);
        const auto address = static_cast<std::uint16_t>(dataOffset + index);
        if (address >= sizeof(CollisionMap))
        {
            throw std::out_of_range("Collision-map write is outside the allocated dummy map.");
        }
        if (address >= dataOffset)
        {
            map.data[address - dataOffset] = static_cast<std::uint8_t>(value);
        }
        else
        {
            // Normalize the far pointer to offset zero; wrapped writes can still hit the header.
            auto* bytes = reinterpret_cast<unsigned char*>(&map);
            bytes[address] = static_cast<std::uint8_t>(value);
        }
    }
}
