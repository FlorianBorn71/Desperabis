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
            if (object.pickupType == PickupType::None)
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
                    state.weaponAmounts[weaponIndex] = WrapSignedWord(stored.generalPurpose1);
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
        case CollisionType::TriggerTextPrompt:
            return state.HandleTextPromptContact(object);
        case CollisionType::Teleporter:
            return state.HandleTeleporterContact(object, true);
        case CollisionType::LevelEnd:
            return state.HandleLevelEndContact(object, true);
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

        case CollisionType::PictureMap:
            state.OpenMap();
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
            if (object.pickupType != PickupType::None)
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
                    stored.generalPurpose1 = static_cast<std::uint16_t>(stored.generalPurpose1 + item.generalPurpose1);
                    state.weaponAmounts[static_cast<std::size_t>(weapon)] = WrapSignedWord(stored.generalPurpose1);
                    state.SetObjectHealth(object, 0);
                }
                else
                {
                    object.collisionType = CollisionType::None;
                    object.collisionMask = 0;
                    object.pickupType = PickupType::None;
                    state.playerCollisionSlot = 0;
                }
                return { InteractionStatus::Applied, static_cast<std::uint16_t>(i + 1) };
            }
            break;

        case CollisionType::Switch:
            return HandleSwitchInteraction(object, state);

        case CollisionType::Lorry:
            if (object.doorLockOrGroupBitmask == 0)
            {
                const auto slot = state.FindWorldObjectSlot(object);
                if (slot < 1) { throw std::invalid_argument("Boarding a lorry requires a world-object slot."); }
                object.doorLockOrGroupBitmask = 1;
                state.controlledVehicleSlot = static_cast<std::uint16_t>(slot);
                return { InteractionStatus::Applied };
            }
            if (object.doorLockOrGroupBitmask == 1)
            {
                object.blendWeightOrSpriteOpacity = 100;
                state.PlaySoundEffect(static_cast<SoundEffect>(object.soundEffectId),
                    GameContext::GetSoundVolume(static_cast<std::int16_t>(state.Random(500))), object, 1);
                return { InteractionStatus::Applied };
            }
            return { InteractionStatus::NoEffect };

        case CollisionType::GroupedSwitch:
            if (object.generalPurpose2 == 0)
            {
                if (object.blendWeightOrSpriteOpacity == 100)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(-10);
                }
                if (object.blendWeightOrSpriteOpacity == 0)
                {
                    object.generalPurpose2 = 10;
                }
                object.generalPurpose1 = static_cast<std::uint16_t>(object.itemValue);
                if (object.soundEffectId == 0)
                {
                    object.soundEffectId = 2;
                }
                activated = true;
            }
            break;

        case CollisionType::PuzzleGameTile:
            object.generalPurpose3 = 1;
            activated = true;
            break;

        case CollisionType::PuzzleSpool:
            object.velocityX = 1;
            activated = true;
            break;

        case CollisionType::GrandfatherClockItem:
            object.velocityX = WrapSignedWord(object.velocityX + WrapSignedWord(object.generalPurpose1));
            return { InteractionStatus::Applied };

        case CollisionType::Door:
            if (object.health == 1000) { return HandleDoorInteraction(object, state); }
            break;

        case CollisionType::BuyItem:
        {
            if (object.itemValue <= 0) { return { InteractionStatus::NoEffect }; }
            if (object.itemValue < 2 || static_cast<std::size_t>(object.itemValue) >= state.worldObjects.size() ||
                state.worldObjects[object.itemValue].collisionType == CollisionType::BuyItem)
            {
                throw std::out_of_range("Shop stock must reference a non-proxy world slot 2..165.");
            }
            const auto price = object.doorLockOrGroupBitmask;
            if (price > 0 && state.gold < price)
            {
                state.PromptUserMessage("Nicht genug Gold", 90);
                return { InteractionStatus::NoEffect };
            }
            if (price > 0) { state.gold = WrapSignedWord(state.gold - price); }
            auto& stock = state.worldObjects[object.itemValue];
            stock.collisionMask = 255;
            const auto result = HandlePlayerObjectInteraction(stock, state);
            // Native 09BE consumes the proxy even if the linked pickup reports a full inventory.
            state.SetObjectHealth(object, 0);
            return result;
        }
        case CollisionType::BookWithCode:
        case CollisionType::WheelToLiftRope:
        case CollisionType::VaseToGrab:
        case CollisionType::MirrorToGrab:
        case CollisionType::SarcophagusWithBones:
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
