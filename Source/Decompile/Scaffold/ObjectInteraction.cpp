#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        std::size_t WeaponAmountIndex(std::int16_t value, const GameContext& state)
        {
            if (value < 0 || static_cast<std::size_t>(value) >= state.weaponAmounts.size())
            {
                throw std::out_of_range("Weapon amount index must be in 0..31 (DS:DD36).");
            }
            return static_cast<std::size_t>(value);
        }

        InteractionResult StoreInventoryObject(
            const RawGameObject& item, RawGameObject& object, GameContext& state)
        {
            if (object.field_32 == 0)
            {
                return { InteractionStatus::NoEffect };
            }
            std::size_t index = 0;
            while (index < state.inventory.size() && state.inventory[index].health != 0)
            {
                ++index;
            }
            if (index == state.inventory.size())
            {
                return { InteractionStatus::InventoryFull };
            }
            if (item.collisionType == CollisionType::Weapon && item.itemValue >= 0)
            {
                WeaponAmountIndex(item.itemValue, state);
            }

            auto& stored = state.inventory[index];
            stored = item;
            if (stored.renderPlacementMode == 20)
            {
                stored.renderPlacementMode = 1;
                stored.doorLockOrGroupBitmask = stored.spriteScale;
            }

            const auto slot = static_cast<std::uint16_t>(index + 1);
            if (stored.collisionType == CollisionType::Weapon && stored.itemValue >= 0)
            {
                const auto weaponIndex = static_cast<std::size_t>(stored.itemValue);
                state.weaponInventorySlots[weaponIndex] = slot;
                const auto weapon = static_cast<WeaponType>(stored.itemValue);
                if (weapon == WeaponType::Fireball)
                {
                    state.weaponAmounts[weaponIndex] = WrapSignedWord(stored.field_24);
                }
                else if (weapon == WeaponType::Dagger || weapon == WeaponType::Sword ||
                    (weapon >= WeaponType::Stone && weapon <= WeaponType::Showel))
                {
                    state.weaponAmounts[weaponIndex] = 360;
                }
            }
            state.SetObjectHealth(object, 0);
            return { InteractionStatus::Applied, slot };
        }
    }

    InteractionResult HandlePlayerObjectInteraction(RawGameObject& object, GameContext& state)
    {
        if (object.health <= 0)
        {
            return { InteractionStatus::InactiveObject };
        }

        // DS:2DEC is the original preview/scratch record, outside the fourteen usable slots.
        const RawGameObject item = object;
        bool activated = false;
        switch (object.collisionType)
        {
        case CollisionType::Poison:
            if (state.arrowPoison < object.itemValue)
            {
                state.arrowPoison = object.itemValue;
            }
            state.SetObjectHealth(object, 0);
            return { InteractionStatus::Applied };

        case CollisionType::PickableMap:
            state.hasMap = true;
            state.SetObjectHealth(object, 0);
            return { InteractionStatus::Applied };

        case CollisionType::Medicine:
            for (std::size_t i = 0; i < state.inventory.size(); ++i)
            {
                auto& stored = state.inventory[i];
                if (stored.health == 1000 && stored.collisionType == CollisionType::Medicine)
                {
                    stored.itemValue = WrapSignedWord(stored.itemValue + object.itemValue);
                    state.SetObjectHealth(object, 0);
                    return { InteractionStatus::Applied, static_cast<std::uint16_t>(i + 1) };
                }
            }
            break;

        case CollisionType::Gold:
            state.gold = WrapSignedWord(state.gold + object.itemValue);
            state.SetObjectHealth(object, 0);
            return { InteractionStatus::Applied };

        case CollisionType::Ammo:
            if (object.field_32 != 0)
            {
                const auto index = WeaponAmountIndex(object.itemValue, state);
                if (object.groupIndexOrLock == 0)
                {
                    object.groupIndexOrLock = 1;
                }
                state.weaponAmounts[index] =
                    WrapSignedWord(state.weaponAmounts[index] + object.groupIndexOrLock);
                state.SetObjectHealth(object, 0);
                return { InteractionStatus::Applied };
            }
            break;

        case CollisionType::Weapon:
            for (std::size_t i = 0; i < state.inventory.size(); ++i)
            {
                auto& stored = state.inventory[i];
                if (stored.health != 1000 || stored.collisionType != CollisionType::Weapon ||
                    stored.itemValue != object.itemValue)
                {
                    continue;
                }
                const auto weapon = static_cast<WeaponType>(object.itemValue);
                if (object.itemValue == static_cast<std::int16_t>(WeaponType::Stone) ||
                    object.itemValue == static_cast<std::int16_t>(WeaponType::TrapOrPole))
                {
                    continue;
                }
                if (object.itemValue == static_cast<std::int16_t>(WeaponType::Fireball))
                {
                    stored.field_24 = static_cast<std::uint16_t>(stored.field_24 + item.field_24);
                    state.weaponAmounts[static_cast<std::size_t>(weapon)] = WrapSignedWord(stored.field_24);
                    state.SetObjectHealth(object, 0);
                }
                else
                {
                    object.collisionType = CollisionType::None;
                    object.field_07 = 0;
                    object.field_32 = 0;
                    state.playerCollisionSlot = 0;
                }
                return { InteractionStatus::Applied, static_cast<std::uint16_t>(i + 1) };
            }
            break;

        case CollisionType::Switch:
            return HandleSwitchInteraction(object, state);

        case CollisionType::GroupedSwitch:
            if (object.field_26 == 0)
            {
                if (object.blendWeightOrSpriteOpacity == 100)
                {
                    object.field_26 = static_cast<std::uint16_t>(-10);
                }
                if (object.blendWeightOrSpriteOpacity == 0)
                {
                    object.field_26 = 10;
                }
                object.field_24 = static_cast<std::uint16_t>(object.itemValue);
                if (object.soundEffectId == 0)
                {
                    object.soundEffectId = 2;
                }
                activated = true;
            }
            break;

        case CollisionType::PuzzleGameTile:
            object.field_28 = 1;
            activated = true;
            break;

        case CollisionType::PuzzleSpool:
            object.velocityX = 1;
            activated = true;
            break;

        case CollisionType::GrandfatherClockItem:
            object.velocityX = WrapSignedWord(object.velocityX + WrapSignedWord(object.field_24));
            return { InteractionStatus::Applied };

        case CollisionType::Door:
            if (object.health == 1000) { return HandleDoorInteraction(object, state); }
            break;

        case CollisionType::BookWithCode:
        case CollisionType::BuyItem:
        case CollisionType::WheelToLiftRope:
        case CollisionType::VaseToGrab:
        case CollisionType::MirrorToGrab:
        case CollisionType::LevelEnd:
        case CollisionType::Teleporter:
        case CollisionType::SarcophagusWithBones:
        case CollisionType::PictureMap:
        case CollisionType::Lorry:
            return { InteractionStatus::UnimplementedHandler };

        default:
            break;
        }

        auto result = StoreInventoryObject(item, object, state);
        if (activated && result.status == InteractionStatus::NoEffect)
        {
            result.status = InteractionStatus::Applied;
        }
        return result;
    }
}
