#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <iterator>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        void PlayDoorSound(RawGameObject& object, GameContext& context, SoundEffect effect,
            std::uint8_t volume, std::int16_t limit)
        {
            if (static_cast<std::uint16_t>(effect) != 0)
            {
                context.PlaySoundEffect(effect, volume, object, limit);
            }
        }

        bool ActivateDoor(RawGameObject& object, GameContext& context)
        {
            if (object.collisionType != CollisionType::Door)
            {
                throw std::runtime_error("Only ordinary door activation has been recovered here.");
            }
            if (object.doorLockOrGroupBitmask >= 1)
            {
                return false;
            }
            context.DamageObject(object, 0);
            return true;
        }
    }

    InteractionResult HandleSwitchInteraction(RawGameObject& object, GameContext& context)
    {
        if (object.collisionType != CollisionType::Switch)
        {
            throw std::invalid_argument("Ordinary switch interaction requires a switch collision type.");
        }
        if (object.health <= 0) { return { InteractionStatus::InactiveObject }; }
        if (object.blendWeightOrSpriteOpacity != 0) { return { InteractionStatus::NoEffect }; }

        const bool activatesTargets = object.itemValue > 0 && object.health == 1000;
        // 10AC:2094 starts the animation even when the target index/health gate fails.
        object.field_26 = object.field_24;
        if (activatesTargets)
        {
            context.ActivateObjectGroup(object.itemValue);
            PlayDoorSound(object, context, static_cast<SoundEffect>(object.soundEffectId),
                context.GetSoundVolumeAt(object.posX, object.posZ), 1);
        }
        return { InteractionStatus::Applied };
    }

    InteractionResult HandleDoorInteraction(RawGameObject& object, GameContext& context)
    {
        if (object.collisionType != CollisionType::Door)
        {
            throw std::invalid_argument("Door interaction requires a door collision type.");
        }
        if (object.health <= 0) { return { InteractionStatus::InactiveObject }; }
        if (object.health != 1000 || object.field_26 != 0) { return { InteractionStatus::NoEffect }; }
        const auto lock = object.doorLockOrGroupBitmask;
        if (lock < 0)
        {
            if (!context.IsUserMessageActive()) { context.PromptUserMessage("abgeschlossen", 60); }
            return { InteractionStatus::NoEffect };
        }
        if (lock > 0)
        {
            std::int16_t selectedKey = 0;
            std::size_t selectedSlot = 0;
            for (std::size_t slot = 0; slot < context.inventory.size(); ++slot)
            {
                const auto& key = context.inventory[slot];
                if (key.health > 0 && key.collisionType == CollisionType::Key && selectedKey != lock)
                {
                    selectedKey = key.itemValue;
                    selectedSlot = slot;
                }
            }
            if (selectedKey == 0)
            {
                if (!context.IsUserMessageActive()) { context.PromptUserMessage("abgeschlossen", 60); }
                return { InteractionStatus::NoEffect };
            }
            if (selectedKey != lock)
            {
                context.PromptUserMessage("Falscher Schl\x81ssel", 90);
                return { InteractionStatus::NoEffect };
            }
            // Emptying the slot also removes a held key; refreshing its HUD icon stays with the host.
            context.inventory[selectedSlot].health = 0;
        }
        object.doorLockOrGroupBitmask = 0;
        return { ActivateDoor(object, context) ? InteractionStatus::Applied : InteractionStatus::NoEffect };
    }

    DispatchResult Function_Door(RawGameObject& object, GameContext& context)
    {
        if (object.groupIndexOrLock < 0 ||
            static_cast<std::size_t>(object.groupIndexOrLock) >= std::size(context.switchGroupMasks))
        {
            throw std::out_of_range("Door switch-group index must be in 0..4.");
        }
        const auto volume = context.GetSoundVolumeAt(object.posX, object.posZ);
        object.collisionRadiusXZ = object.health == 1000 ? 54 : 10;
        if (context.switchGroupMasks[object.groupIndexOrLock] == 0) { ActivateDoor(object, context); }

        if (WrapSignedWord(object.field_24) > 0)
        {
            if (object.itemValue > 0) { object.itemValue = WrapSignedWord(object.itemValue - context.tickCount); }
            if (object.behaviorParameter != 0 && object.blendWeightOrSpriteOpacity > 0)
            {
                // Contact recorded before this update reverses closing and refreshes the delay.
                object.itemValue = WrapSignedWord(object.field_24);
                object.field_26 = 3;
            }
            if (object.itemValue < 1 && object.blendWeightOrSpriteOpacity == 100)
            {
                PlayDoorSound(object, context, static_cast<SoundEffect>(object.soundEffectId), volume, 1);
                object.field_26 = static_cast<std::uint16_t>(-3);
            }
        }
        object.blendWeightOrSpriteOpacity = WrapSignedWord(
            object.blendWeightOrSpriteOpacity + object.field_26 * static_cast<std::int32_t>(context.tickCount));
        if (object.blendWeightOrSpriteOpacity > 100)
        {
            object.blendWeightOrSpriteOpacity = 100;
            if (object.field_24 == 0)
            {
                // One-shot doors disable on overshoot, not when they land exactly at fully open.
                object.behavior = BehaviorType::None;
                object.collisionRadiusXZ = 0;
                object.field_07 = 0;
            }
        }
        if (object.blendWeightOrSpriteOpacity < 0)
        {
            object.blendWeightOrSpriteOpacity = 0;
            PlayDoorSound(object, context, SoundEffect::DoorClosed, volume, 2);
        }
        if (object.blendWeightOrSpriteOpacity == 0 || object.blendWeightOrSpriteOpacity == 100)
        {
            object.field_26 = 0;
        }
        const auto collisionValue = object.blendWeightOrSpriteOpacity == 100 ?
            static_cast<std::uint16_t>(object.velocityY) : CollisionMapValue_BlockingWall;
        context.SetDoorCollisionMapAtWorldXZ(object.posX, object.posZ, collisionValue);
        const auto collision = context.FindCollidingObject(object);
        object.behaviorParameter = static_cast<std::uint8_t>(collision);
        return { DispatchStatus::ExecutedCase, BehaviorType::Door, 0x14ff };
    }
}
