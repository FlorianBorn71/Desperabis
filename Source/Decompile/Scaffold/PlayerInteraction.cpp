#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        bool AxisOverlaps(std::int16_t first, std::int16_t second,
            std::int16_t firstRadius, std::int16_t secondRadius)
        {
            const auto difference = WrapSignedWord(first - second);
            const auto distance = difference < 0 ? WrapSignedWord(-difference) : difference;
            return distance < WrapSignedWord(firstRadius + secondRadius);
        }

        bool ObjectsOverlap(const RawGameObject& source, const RawGameObject& candidate)
        {
            return (source.field_07 & candidate.field_07) != 0 &&
                AxisOverlaps(source.posX, candidate.posX, source.collisionRadiusXZ, candidate.collisionRadiusXZ) &&
                AxisOverlaps(source.posY, candidate.posY, source.collisionRadiusY, candidate.collisionRadiusY) &&
                AxisOverlaps(source.posZ, candidate.posZ, source.collisionRadiusXZ, candidate.collisionRadiusXZ);
        }
    }

    std::uint16_t GameState::FindCollidingObject(std::int16_t sourceSlot) const
    {
        if (!objectCollisionsEnabled)
        {
            return 0;
        }
        if (sourceSlot < 0 || static_cast<std::size_t>(sourceSlot) >= worldObjects.size())
        {
            throw std::out_of_range("Collision source slot must be in 0..165.");
        }
        const auto& source = worldObjects[sourceSlot];
        for (std::uint16_t slot = 2; slot <= 164; ++slot)
        {
            const auto& candidate = worldObjects[slot];
            if (slot != sourceSlot && candidate.health > 0 && ObjectsOverlap(source, candidate))
            {
                return slot;
            }
        }
        return 0;
    }

    std::uint16_t GameState::FindPlayerCollision() const
    {
        const auto first = FindCollidingObject(1);
        if (first == 0)
        {
            return 0;
        }
        auto slot = first;
        do
        {
            ++slot;
            const auto& candidate = worldObjects[slot];
            if (candidate.health > 999 && candidate.field_32 != 0 &&
                ObjectsOverlap(worldObjects[1], candidate))
            {
                return slot;
            }
        } while (slot <= 163);
        return first;
    }

    InteractionResult UpdatePlayerInteraction(GameState& context, InputHandler& input)
    {
        context.playerCollisionSlot = context.FindPlayerCollision();
        if (context.playerCollisionSlot == 0 || !input.IsInteractionKeyPressed())
        {
            return { InteractionStatus::NoEffect };
        }
        return HandlePlayerObjectInteraction(context.worldObjects[context.playerCollisionSlot], context);
    }
}
