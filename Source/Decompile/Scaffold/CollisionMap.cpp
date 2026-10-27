#include "GameContext.h"

#include <new>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        using Icon = std::array<std::uint8_t, 25>;
        constexpr Icon placeholderPattern{
            0xf2, 0xff, 0, 0xff, 0xf2,
            0xff, 0xf2, 0xff, 0xf2, 0xff,
            0, 0xff, 0xf2, 0xff, 0,
            0xff, 0xf2, 0xff, 0xf2, 0xff,
            0xf2, 0xff, 0, 0xff, 0xf2 };
        // Immutable wall-icon AN2 frames 1..19; native loading discards animation offsets.
        constexpr std::array<Icon, 19> wallIcons{{
            { 56,0,0,0,0, 56,0,0,0,0, 56,0,0,0,0, 56,0,0,0,0, 56,0,0,0,0 },
            { 56,56,56,56,56, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
            { 0,0,0,0,56, 0,0,0,0,56, 0,0,0,0,56, 0,0,0,0,56, 0,0,0,0,56 },
            { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 56,56,56,56,56 },
            { 56,56,56,56,56, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 56,56,56,56,56 },
            { 56,0,0,0,56, 56,0,0,0,56, 56,0,0,0,56, 56,0,0,0,56, 56,0,0,0,56 },
            { 56,56,56,56,56, 56,56,56,56,56, 56,56,56,56,56, 56,56,56,56,56, 56,56,56,56,56 },
            { 56,0,0,0,56, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 56,0,0,0,56 },
            placeholderPattern, placeholderPattern, placeholderPattern,
            { 0,0,56,56,56, 0,0,56,56,56, 0,0,56,56,56, 0,0,0,0,0, 0,0,0,0,0 },
            { 56,56,56,0,0, 56,56,56,0,0, 56,56,56,0,0, 0,0,0,0,0, 0,0,0,0,0 },
            { 0,0,0,0,0, 0,0,0,0,0, 56,56,56,0,0, 56,56,56,0,0, 56,56,56,0,0 },
            { 0,0,0,0,0, 0,0,0,0,0, 0,0,56,56,56, 0,0,56,56,56, 0,0,56,56,56 },
            { 0,0,0,0,0, 0,0,0,0,0, 56,56,56,56,56, 0,0,0,0,0, 0,0,0,0,0 },
            { 0,0,56,0,0, 0,0,56,0,0, 0,0,56,0,0, 0,0,56,0,0, 0,0,56,0,0 },
            { 0,0,0,0,0, 0,0,0,0,0, 0,0,56,0,0, 0,0,0,0,0, 0,0,0,0,0 },
            placeholderPattern
        }};
        constexpr std::array<std::uint8_t, 20> wallPassOrder{
            0, 16, 17, 18, 19, 27, 9, 10, 11, 12, 13, 14, 15, 20, 21, 22, 23, 24, 25, 26 };
    }

    void Level::BuildCollisionMap()
    {
        if (geometryWidth == 0 || geometryWidth > Width || geometryHeight == 0 || geometryHeight > Height)
        {
            throw std::invalid_argument("Collision-map geometry dimensions must fit 50x40.");
        }
        const auto width = geometryWidth * 4 - 2;
        const auto height = geometryHeight * 4 - 1;
        auto map = std::make_unique<CollisionMap>();
        map->widthMinusOne = static_cast<std::uint16_t>(width - 1);
        map->heightMinusOne = static_cast<std::uint16_t>(height - 1);
        const auto putPixel = [&](int x, int y, std::uint8_t value)
        {
            if (x >= 0 && y >= 0 && static_cast<std::size_t>(x) < width && static_cast<std::size_t>(y) < height)
            {
                map->data[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = value;
            }
        };
        // 1E48:594E draws at (4*column-1,4*row-1), then 0000:07F4 captures from (4,3).
        for (const auto pass : wallPassOrder)
        {
            for (std::size_t row = 0; row < geometryHeight; ++row)
            {
                for (std::size_t column = 0; column < geometryWidth; ++column)
                {
                    const int x = static_cast<int>(column * 4) - 1;
                    const int y = static_cast<int>(row * 4);
                    const auto selector = surfaceFlags[row][column] & 31;
                    for (int dy = 0; dy < 5; ++dy)
                    {
                        for (int dx = 0; dx < 5; ++dx)
                        {
                            if (pass == 0) { putPixel(x + dx, y + dy, collisionMaterials[row][column]); }
                            else if (selector == pass && selector >= 9 && selector <= 27)
                            {
                                const auto value = wallIcons[selector - 9][static_cast<std::size_t>(dy * 5 + dx)];
                                if (value != 0) { putPixel(x + dx, y + dy, value); }
                            }
                        }
                    }
                }
            }
        }
        collisionMap = std::move(map);
    }

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
}
