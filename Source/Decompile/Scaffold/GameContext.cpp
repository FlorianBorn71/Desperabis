#include "GameContext.h"

namespace Desperabis
{
    std::uint16_t GameContext::FindFreeObjectIndex()
    {
        std::uint16_t slot = 2;
        while (slot < 165 && worldObjects[slot].health != 0) { ++slot; }
        worldObjects[slot] = {};
        return slot;
    }

    std::uint16_t GameContext::FindFreeOrLowestHealthObjectIndex()
    {
        std::uint16_t lowestSlot = 165;
        std::int16_t lowestHealth = 1000;
        std::uint16_t slot = 2;
        for (; slot <= 164; ++slot)
        {
            const auto health = worldObjects[slot].health;
            if (health < lowestHealth)
            {
                lowestHealth = health;
                lowestSlot = slot;
            }
            if (health == 0) { break; }
        }
        if (slot > 164) { slot = lowestSlot; }
        worldObjects[slot] = {};
        return slot;
    }
}
