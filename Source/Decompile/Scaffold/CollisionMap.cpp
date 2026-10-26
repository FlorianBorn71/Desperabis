#include "GameContext.h"

#include <new>

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
}
