#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <stdexcept>

namespace Desperabis
{
    const std::vector<BehaviorDescriptor>& BehaviorCatalog()
    {
        static const std::vector<BehaviorDescriptor> catalog{
#define BEHAVIOR_CASE(name, entry) { BehaviorType::name, #name, entry, true },
#define BEHAVIOR_COMMON(name) { BehaviorType::name, #name, 0x5110, false },
#include "BehaviorCases.inc"
#undef BEHAVIOR_COMMON
#undef BEHAVIOR_CASE
        };
        return catalog;
    }

    const BehaviorDescriptor* FindBehavior(BehaviorType type)
    {
        for (const auto& descriptor : BehaviorCatalog())
        {
            if (descriptor.type == type)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    const BehaviorDescriptor* FindBehavior(std::string_view name)
    {
        for (const auto& descriptor : BehaviorCatalog())
        {
            if (name == descriptor.name)
            {
                return &descriptor;
            }
        }
        return nullptr;
    }

    namespace
    {
        DispatchResult DispatchActiveObjectCase(RawGameObject& object, GameContext& context)
        {
            if (!context.behaviorDispatchEnabled)
            {
                return { DispatchStatus::DispatchDisabled, object.behavior, 0x5110 };
            }
            switch (object.behavior)
            {
#define BEHAVIOR_CASE(name, entry) case BehaviorType::name: return Function_##name(object, context);
#define BEHAVIOR_COMMON(name) case BehaviorType::name: return { DispatchStatus::NoSpecificCase, BehaviorType::name, 0x5110 };
#include "BehaviorCases.inc"
#undef BEHAVIOR_COMMON
#undef BEHAVIOR_CASE
            default:
                return { DispatchStatus::NoSpecificCase, object.behavior, 0x5110 };
            }
        }
    }

    DispatchResult DispatchObjectCase(RawGameObject& object, GameContext& context)
    {
        if (context.tickCount < 0)
        {
            throw std::invalid_argument("The scaffold requires a nonnegative tick count.");
        }
        if (object.health <= 0)
        {
            return { DispatchStatus::InactiveObject, object.behavior, 0x5511 };
        }
        return DispatchActiveObjectCase(object, context);
    }

    DispatchResult UpdateObject(RawGameObject& object, GameContext& context)
    {
        if (context.tickCount < 0 || context.frameTickCount < 0)
        {
            throw std::invalid_argument("Object updates require nonnegative behavior and frame tick counts.");
        }
        if (object.health <= 0)
        {
            return { DispatchStatus::InactiveObject, object.behavior, 0x5511 };
        }
        const auto display = static_cast<std::uint8_t>(object.displayType);
        if (display > 49 && display < 121)
        {
            context.PlaySoundEffect(SoundEffect::Burning,
                context.GetSoundVolumeAt(object.posX, object.posZ, 3), 451, 2);
            const auto remaining = static_cast<int>(display) -
                context.Random(static_cast<std::uint16_t>(context.frameTickCount + 1));
            object.displayType = remaining < 50 ? DisplayType::VectorBrightness2 :
                static_cast<DisplayType>(remaining);
        }
        if (context.objectCollisionsEnabled && object.health < 1000)
        {
            context.SetObjectHealth(object,
                WrapSignedWord(object.health + GetStepToward(object.health, 0, context.tickCount)));
        }
        // The native loop does not recheck health after the countdown.
        return DispatchActiveObjectCase(object, context);
    }

    std::array<DispatchResult, 163> UpdateObjects(GameContext& context)
    {
        if (context.tickCount < 0 || context.frameTickCount < 0)
        {
            throw std::invalid_argument("Object updates require nonnegative behavior and frame tick counts.");
        }
        std::array<DispatchResult, 163> results;
        for (std::size_t slot = 2; slot <= 164; ++slot)
        {
            results[slot - 2] = UpdateObject(context.worldObjects[slot], context);
        }
        return results;
    }
}
