#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    void TestObjectLayout()
    {
        RawGameObject object;
        for (const auto value : object.ReadBytes())
        {
            Check(value == 0, "Default object must remain zero-initialized.");
        }
        for (std::size_t i = 0; i < 27; ++i)
        {
            object.WriteWord(i * 2, static_cast<std::uint16_t>(0x8080u + i * 257u));
        }
        const auto bytes = object.ReadBytes();
        const auto signedWords = object.ReadSignedWords();
        const auto unsignedWords = object.ReadUnsignedWords();
        for (std::size_t i = 0; i < unsignedWords.size(); ++i)
        {
            Check(unsignedWords[i] == object.ReadWord(i * 2),
                "Unsigned word view must match original little-endian words.");
            Check(signedWords[i] == object.ReadSignedWord(i * 2),
                "Signed word view must match original signed words.");
        }
        Check(object.modelId == bytes[0] && object.modelAnim == bytes[1] &&
            object.modelFrame2 == bytes[2] && object.animFrame == bytes[3],
            "Animation byte fields differ from the renderer accessor layout.");
        Check(static_cast<std::uint16_t>(object.behavior) == unsignedWords[4] && object.health == signedWords[5] &&
            object.posX == signedWords[6] && object.posY == signedWords[7] && object.posZ == signedWords[8],
            "Behavior, health, or position field layout changed.");
        Check(object.blendWeightOrSpriteOpacity == signedWords[21] &&
            object.groupIndexOrLock == signedWords[22] &&
            object.doorLockOrGroupBitmask == signedWords[23] &&
            static_cast<std::uint8_t>(object.displayType) == bytes[51] && object.spriteScale == unsignedWords[26],
            "Behavior-dependent or display field layout changed.");

        RawGameObject copy = object;
        Check(copy.ReadBytes() == bytes, "Struct copies must preserve all 54 raw bytes.");
        copy.behavior = BehaviorType::Behavior_300;
        copy.collisionType = CollisionType::Gold;
        copy.displayType = DisplayType::SpriteTransparent;
        copy.health = -123;
        copy.posX = -32768;
        copy.posY = 1234;
        copy.posZ = 32767;
        copy.spriteScale = 65535;
        Check(copy.ReadWord(8) == 300 && copy.ReadSignedWord(10) == -123,
            "Named fields must preserve full-width behavior and signed health.");
        Check(copy.ReadBytes()[6] == 3 && copy.ReadBytes()[51] == 1,
            "Typed collision/display members must preserve their original byte values.");
        Check(copy.ReadWord(12) == 0x8000 && copy.ReadSignedWord(14) == 1234 &&
            copy.ReadSignedWord(16) == 32767 && copy.ReadWord(52) == 0xffff,
            "Named field writes did not reach the original offsets.");
        copy.WriteWord(12, 0xff9c);
        Check(copy.posX == -100, "Raw writes must update named members directly.");
        RawGameObject& direct = copy;
        direct.posX = 99;
        Check(copy.posX == 99, "Passing the object by reference must not create a snapshot.");
    }

    void TestCases()
    {
        std::size_t specific = 0;
        for (const auto& entry : BehaviorCatalog())
        {
            RawGameObject object;
            object.behavior = entry.type;
            object.WriteWord(10, 1);
            object.WriteWord(12, 0xff9c);
            if (entry.type == BehaviorType::WaterDrop)
            {
                object.posX = 150;
                object.posZ = 4050;
            }
            const auto before = object.ReadBytes();
            GameState context{ 3, true };
            auto level = CreateDummyLevel();
            context.collisionMap = level.collisionMap.get();
            context.level = &level;
            const auto result = DispatchObjectCase(object, context);
            Check(result.moduleOffset == entry.moduleOffset, "Wrong case entry.");
            Check(result.behavior == entry.type, "Wrong behavior enum from handler.");
            Check(FindBehavior(entry.type) == &entry && FindBehavior(entry.name) == &entry,
                "Behavior enum/name lookup must reference the same descriptor.");
            const bool implemented = entry.type == BehaviorType::Pillar || entry.type == BehaviorType::IdleAnimation ||
                entry.type == BehaviorType::SmokeFading || entry.type == BehaviorType::Switch1 ||
                entry.type == BehaviorType::SwitchGrouped || entry.type == BehaviorType::WaterDrop;
            Check(result.status == (implemented ?
                DispatchStatus::ExecutedCase : entry.hasSpecificCase ?
                DispatchStatus::UnimplementedHandler : DispatchStatus::NoSpecificCase), "Wrong case status.");
            if (!implemented)
            {
                Check(object.ReadBytes() == before, "An unrecovered handler changed object state.");
            }
            Check(context.tickCount == 3 && context.behaviorDispatchEnabled,
                "A handler changed the dispatch inputs.");
            if (entry.hasSpecificCase) { ++specific; }
        }
        Check(specific == 187, "Expected 186 behavior cases plus the FFFF sentinel.");
        Check(BehaviorCatalog().size() == 190, "Unexpected catalog coverage.");
    }

    std::int16_t ReferenceSignedWord(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    std::int16_t ReferenceStep(std::int16_t current, std::int16_t target, std::int16_t step)
    {
        const auto difference = ReferenceSignedWord(static_cast<std::int64_t>(target) - current);
        const auto magnitude = ReferenceSignedWord(difference < 0 ? -static_cast<std::int64_t>(difference) : difference);
        return magnitude <= step ? difference :
            ReferenceSignedWord((difference < 0 ? -1LL : difference > 0 ? 1LL : 0LL) * step);
    }

    void TestPlayerInteractionLoop()
    {
        struct TestInputHandler final : InputHandler
        {
            bool pressed = false;
            int calls = 0;

            bool IsInteractionKeyPressed() override
            {
                ++calls;
                return pressed;
            }
        };

        GameState state;
        auto& player = state.worldObjects[1];
        auto& target = state.worldObjects[2];
        const auto reset = [&]
        {
            state = {};
            player.health = 1000;
            player.field_07 = 3;
            player.collisionRadiusXZ = 10;
            player.collisionRadiusY = 20;
            target = player;
            target.field_07 = 2;
            target.collisionRadiusXZ = 5;
            target.collisionRadiusY = 7;
            target.collisionType = CollisionType::Gold;
            target.itemValue = 10;
            target.field_32 = 1;
        };
        reset();
        Check(state.FindCollidingObject(1) == 2 && state.FindPlayerCollision() == 2,
            "Player collision must find the first eligible world object.");
        target.health = 0;
        Check(state.FindCollidingObject(1) == 0, "Zero-health candidates must be ignored.");
        target.health = -1;
        Check(state.FindCollidingObject(1) == 0, "Negative-health candidates must be ignored.");
        target.health = 1;
        target.field_07 = 4;
        Check(state.FindCollidingObject(1) == 0, "Collision masks must share a bit.");
        target.field_07 = 2;
        target.field_32 = 0;
        player.health = 0;
        Check(state.FindCollidingObject(1) == 2,
            "The original generic query does not gate on source health or candidate pickup flag.");
        Check(state.FindCollidingObject(2) == 0, "Generic collision queries must exclude their source slot.");

        reset();
        for (int axis = 0; axis < 3; ++axis)
        {
            for (int bits = 0; bits <= 65535; ++bits)
            {
                const auto position = ReferenceSignedWord(bits);
                target.posX = axis == 0 ? position : 0;
                target.posY = axis == 1 ? position : 0;
                target.posZ = axis == 2 ? position : 0;
                const auto difference = ReferenceSignedWord(-static_cast<std::int64_t>(position));
                const auto distance = ReferenceSignedWord(difference < 0 ?
                    -static_cast<std::int64_t>(difference) : difference);
                const auto radiusSum = axis == 1 ? 27 : 15;
                Check(state.FindCollidingObject(1) == (distance < radiusSum ? 2 : 0),
                    "Collision geometry differs from signed 16-bit subtraction/ABS or strict axis bounds.");
            }
        }
        reset();
        for (int bits = 0; bits <= 65535; ++bits)
        {
            player.collisionRadiusXZ = ReferenceSignedWord(bits);
            target.collisionRadiusXZ = 1;
            target.posX = 1;
            const auto sum = ReferenceSignedWord(static_cast<std::int64_t>(player.collisionRadiusXZ) + 1);
            Check(state.FindCollidingObject(1) == (1 < sum ? 2 : 0),
                "XZ radius sums must wrap before signed comparison.");
        }
        reset();
        player.posX = 32767;
        target.posX = -32768;
        Check(state.FindCollidingObject(1) == 2, "Coordinate differences must wrap across the word boundary.");
        reset();
        player.collisionRadiusXZ = 0;
        target.collisionRadiusXZ = 0;
        target.posX = -32768;
        target.posZ = -32768;
        Check(state.FindCollidingObject(1) == 2,
            "The original signed ABS overflow must remain observable, even with zero XZ radii.");
        reset();
        for (const auto position : { -15, 15 })
        {
            target.posX = static_cast<std::int16_t>(position);
            Check(state.FindCollidingObject(1) == 0, "Merely touching an X boundary is not a collision.");
        }
        target.posX = 0;
        for (const auto position : { -27, 27 })
        {
            target.posY = static_cast<std::int16_t>(position);
            Check(state.FindCollidingObject(1) == 0, "Merely touching a Y boundary is not a collision.");
        }

        reset();
        state.worldObjects[0] = target;
        state.worldObjects[165] = target;
        target.health = 0;
        Check(state.FindCollidingObject(1) == 0 && state.FindPlayerCollision() == 0,
            "Reserved slots must not become initial collision candidates.");
        state.worldObjects[164] = state.worldObjects[165];
        Check(state.FindCollidingObject(1) == 164 && state.FindPlayerCollision() == 165,
            "The player wrapper must preserve its post-test slot-165 boundary read.");
        state.worldObjects[165].field_32 = 0;
        Check(state.FindPlayerCollision() == 164, "An ineligible boundary candidate must not replace slot 164.");
        Check(state.FindCollidingObject(164) == 0, "Last-slot generic queries must exclude themselves.");
        Check(state.FindCollidingObject(165) == 164, "A valid source in slot 165 must use the original candidate range.");

        reset();
        state.worldObjects[3] = target;
        state.worldObjects[3].posX = 10;
        state.worldObjects[4] = target;
        Check(state.FindPlayerCollision() == 3,
            "Player selection must prefer the first qualifying later object, not the nearest or the last.");
        state.worldObjects[4].health = 0;
        for (const auto health : { -1, 0, 1, 999 })
        {
            state.worldObjects[3].health = static_cast<std::int16_t>(health);
            Check(state.FindPlayerCollision() == 2, "Preferred candidates require signed health above 999.");
        }
        state.worldObjects[3].health = 1000;
        state.worldObjects[3].field_32 = 0;
        Check(state.FindPlayerCollision() == 2, "Preferred candidates require a nonzero pickup flag.");
        state.worldObjects[3].field_32 = 1;
        state.worldObjects[3].field_07 = 4;
        Check(state.FindPlayerCollision() == 2, "Preferred candidates must still pass mask filtering.");
        state.worldObjects[3].field_07 = 2;
        state.worldObjects[3].posX = 15;
        Check(state.FindPlayerCollision() == 2, "Preferred candidates must still strictly overlap.");
        state.worldObjects[3].posX = 14;
        Check(state.FindPlayerCollision() == 3, "A valid later overlap must replace the initial candidate.");

        reset();
        for (const auto source : { -32768, -1, 166, 32767 })
        {
            bool rejected = false;
            try { state.FindCollidingObject(static_cast<std::int16_t>(source)); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected, "Out-of-storage source slots must fail explicitly.");
        }
        state.objectCollisionsEnabled = false;
        Check(state.FindCollidingObject(-1) == 0 && state.FindPlayerCollision() == 0,
            "The original DS:D500 gate must return before unused source access.");

        reset();
        TestInputHandler input;
        auto result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::NoEffect && state.playerCollisionSlot == 2 &&
            input.calls == 1 && target.health == 1000 && state.gold == 0,
            "Overlap without interaction input must select the target without collecting it.");
        input.pressed = true;
        state.behaviorDispatchEnabled = false;
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::Applied && state.playerCollisionSlot == 2 &&
            state.gold == 10 && target.health == 0 && input.calls == 2,
            "Collision then input must invoke the interaction independently of behavior dispatch.");
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::NoEffect && state.playerCollisionSlot == 0 &&
            state.gold == 10 && input.calls == 2,
            "Collected objects must stop being selected; no target must not invoke input/interaction.");

        reset();
        input = {};
        input.pressed = true;
        state.worldObjects[3] = target;
        state.worldObjects[3].itemValue = 20;
        UpdatePlayerInteraction(state, input);
        Check(state.playerCollisionSlot == 3 && state.gold == 20 && target.health == 1000,
            "Input must act on the preferred selected object, not the initial generic hit.");
        UpdatePlayerInteraction(state, input);
        Check(state.playerCollisionSlot == 2 && state.gold == 30 && input.calls == 2,
            "Input edge/held semantics must belong to the input handler, not an invented loop debounce.");

        reset();
        input = {};
        input.pressed = true;
        state.objectCollisionsEnabled = false;
        state.playerCollisionSlot = 123;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::NoEffect &&
            state.playerCollisionSlot == 0 && input.calls == 0 && target.health == 1000,
            "Disabled collision queries must clear stale selection and never interact.");
        state.objectCollisionsEnabled = true;
        target.posZ = 15;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::NoEffect &&
            input.calls == 0 && state.gold == 0, "A pressed key without overlap must not collect anything.");
        target.posZ = 0;
        target.collisionType = CollisionType::Door;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::UnimplementedHandler &&
            target.health == 1000, "The loop must expose unsupported interaction results to its caller.");

        reset();
        input = {};
        input.pressed = true;
        target.collisionType = CollisionType::Key;
        target.itemValue = 7;
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::Applied && result.inventorySlot == 1 &&
            state.inventory[0].collisionType == CollisionType::Key &&
            state.inventory[0].itemValue == 7 && target.health == 0,
            "Collision/input-driven key pickup must populate the real game-context inventory.");

        reset();
        input = {};
        input.pressed = true;
        target.collisionType = CollisionType::Weapon;
        target.itemValue = static_cast<std::int16_t>(WeaponType::Dagger);
        state.inventory[0] = target;
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::Applied && state.playerCollisionSlot == 0 &&
            target.health == 1000 && target.collisionType == CollisionType::None,
            "Duplicate weapon interaction must immediately clear the now-modeled DS:DC60 selection.");
    }

    void TestObjectInteractions()
    {
        const auto pickup = [](CollisionType type, std::int16_t value = 1)
        {
            RawGameObject object;
            object.collisionType = type;
            object.itemValue = value;
            object.health = 1000;
            object.field_32 = 1;
            object.modelId = 17;
            object.posX = -123;
            return object;
        };
        const auto sameObject = [](const RawGameObject& left, const RawGameObject& right)
        {
            return std::memcmp(&left, &right, sizeof(left)) == 0;
        };
        GameState state;
        Check(state.inventory.size() == 14 && state.weaponAmounts.size() == 32 &&
            state.weaponInventorySlots.size() == 32, "Wrong original inventory/counter capacities.");
        Check(state.gold == 0 && state.arrowPoison == 0 && !state.hasMap &&
            state.weaponAmounts[8] == 100 && state.weaponAmounts[20] == 1000 &&
            state.weaponAmounts[23] == 100, "Original new-game interaction defaults changed.");
        for (const auto& object : state.inventory)
        {
            Check(sameObject(object, RawGameObject{}), "Inventory must start empty.");
        }
        for (std::size_t i = 0; i < state.weaponAmounts.size(); ++i)
        {
            Check(state.weaponInventorySlots[i] == 0 &&
                state.weaponAmounts[i] == (i == 8 || i == 23 ? 100 : i == 20 ? 1000 : 0),
                "Unexpected original weapon/resource initialization.");
        }

        for (int bits = 0; bits <= 65535; ++bits)
        {
            const auto value = ReferenceSignedWord(bits);
            auto gold = pickup(CollisionType::Gold, value);
            auto expected = gold;
            expected.health = 0;
            state.gold = 12345;
            Check(HandlePlayerObjectInteraction(gold, state).status == InteractionStatus::Applied &&
                state.gold == ReferenceSignedWord(12345LL + value) && sameObject(gold, expected),
                "Gold pickup must wrap one word and clear only world health.");
            const auto amount = state.gold;
            Check(HandlePlayerObjectInteraction(gold, state).status == InteractionStatus::InactiveObject &&
                state.gold == amount, "Collected gold must not be awarded twice.");

            auto poison = pickup(CollisionType::Poison, value);
            state.arrowPoison = -123;
            HandlePlayerObjectInteraction(poison, state);
            Check(state.arrowPoison == std::max<std::int16_t>(-123, value) && poison.health == 0,
                "Arrow poison pickup must take the signed maximum.");

            state.inventory[0] = pickup(CollisionType::Medicine, 23456);
            auto medicine = pickup(CollisionType::Medicine, value);
            medicine.field_32 = 0;
            const auto result = HandlePlayerObjectInteraction(medicine, state);
            Check(result.status == InteractionStatus::Applied && result.inventorySlot == 1 &&
                state.inventory[0].itemValue == ReferenceSignedWord(23456LL + value) && medicine.health == 0,
                "Medicine must stack with word wrapping, even without a pickup flag.");
        }

        state = {};
        auto map = pickup(CollisionType::PickableMap);
        map.field_32 = 0;
        Check(HandlePlayerObjectInteraction(map, state).status == InteractionStatus::Applied &&
            state.hasMap && map.health == 0, "Map interaction must set ownership without consuming a slot.");
        auto medicine = pickup(CollisionType::Medicine, 25);
        const auto medicineCopy = medicine;
        auto result = HandlePlayerObjectInteraction(medicine, state);
        Check(result.inventorySlot == 1 && sameObject(state.inventory[0], medicineCopy) && medicine.health == 0,
            "The first medicine must copy all 54 bytes into the first slot.");

        state = {};
        state.inventory[0].health = -1;
        state.inventory[1] = pickup(CollisionType::Medicine, 40);
        state.inventory[1].health = 1;
        auto key = pickup(CollisionType::Key, 7);
        key.field_04 = 0xab14;
        key.spriteScale = 0xdc21;
        auto expectedKey = key;
        expectedKey.field_04 = 0xab01;
        expectedKey.doorLockOrGroupBitmask = 0xdc;
        result = HandlePlayerObjectInteraction(key, state);
        Check(result.status == InteractionStatus::Applied && result.inventorySlot == 3 &&
            sameObject(state.inventory[2], expectedKey) && key.health == 0 &&
            key.field_04 == 0xab14 && key.doorLockOrGroupBitmask == 0,
            "Inventory occupancy/copy normalization must match the original and not normalize the world object.");
        auto secondKey = pickup(CollisionType::Key, 7);
        Check(HandlePlayerObjectInteraction(secondKey, state).inventorySlot == 4,
            "Keys must occupy individual slots, not stack by key index.");
        medicine = pickup(CollisionType::Medicine, 5);
        Check(HandlePlayerObjectInteraction(medicine, state).inventorySlot == 5 &&
            state.inventory[1].itemValue == 40,
            "Medicine may stack only with health exactly 1000, not merely positive health.");

        for (auto& stored : state.inventory)
        {
            stored = pickup(CollisionType::Key);
        }
        key = pickup(CollisionType::Key);
        const auto fullInventory = state.inventory;
        const auto uncollectedKey = key;
        Check(HandlePlayerObjectInteraction(key, state).status == InteractionStatus::InventoryFull &&
            sameObject(key, uncollectedKey) &&
            std::memcmp(state.inventory.data(), fullInventory.data(), sizeof(state.inventory)) == 0,
            "Full inventory must not overwrite any slot or remove the world pickup.");
        state.inventory.back() = pickup(CollisionType::Medicine, 20);
        medicine = pickup(CollisionType::Medicine, 10);
        Check(HandlePlayerObjectInteraction(medicine, state).inventorySlot == 14 &&
            state.inventory.back().itemValue == 30,
            "Medicine must still stack when every inventory slot is occupied.");
        state.inventory.back() = {};
        key = pickup(CollisionType::Key);
        Check(HandlePlayerObjectInteraction(key, state).inventorySlot == 14,
            "The final inventory slot must remain usable.");
        auto gold = pickup(CollisionType::Gold, 9);
        Check(HandlePlayerObjectInteraction(gold, state).status == InteractionStatus::Applied &&
            state.gold == 9, "Gold must not require a free inventory slot.");

        for (std::int16_t index = 0; index < 32; ++index)
        {
            state = {};
            state.weaponAmounts[index] = 32767;
            auto ammo = pickup(CollisionType::Ammo, index);
            Check(HandlePlayerObjectInteraction(ammo, state).status == InteractionStatus::Applied &&
                state.weaponAmounts[index] == -32768 && ammo.groupIndexOrLock == 1 && ammo.health == 0,
                "Ammo must use the full 32-entry table, default zero quantity to one, and wrap.");
            ammo = pickup(CollisionType::Ammo, index);
            ammo.groupIndexOrLock = -2;
            HandlePlayerObjectInteraction(ammo, state);
            Check(state.weaponAmounts[index] == 32766,
                "Signed ammo quantity must be added without saturation.");
        }
        for (const auto invalid : { -32768, -1, 32, 32767 })
        {
            state = {};
            auto ammo = pickup(CollisionType::Ammo, static_cast<std::int16_t>(invalid));
            const auto before = ammo;
            const auto amounts = state.weaponAmounts;
            bool rejected = false;
            try { HandlePlayerObjectInteraction(ammo, state); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && sameObject(ammo, before) && state.weaponAmounts == amounts,
                "Invalid ammo indices must fail explicitly before mutation.");
            auto weapon = pickup(CollisionType::Weapon, static_cast<std::int16_t>(invalid));
            if (invalid >= 0)
            {
                rejected = false;
                try { HandlePlayerObjectInteraction(weapon, state); }
                catch (const std::out_of_range&) { rejected = true; }
                Check(rejected && weapon.health == 1000 && state.inventory[0].health == 0,
                    "Invalid weapon indices must not leave partially stored items.");
            }
        }

        for (std::int16_t type = 0; type <= 8; ++type)
        {
            state = {};
            state.weaponAmounts[type] = 123;
            auto weapon = pickup(CollisionType::Weapon, type);
            weapon.field_24 = 0x8002;
            result = HandlePlayerObjectInteraction(weapon, state);
            const auto expectedAmount = type == static_cast<std::int16_t>(WeaponType::Fireball) ? -32766 :
                type == static_cast<std::int16_t>(WeaponType::None) ||
                type == static_cast<std::int16_t>(WeaponType::Crossbow) ? 123 : 360;
            Check(result.inventorySlot == 1 && state.weaponInventorySlots[type] == 1 &&
                state.weaponAmounts[type] == expectedAmount && weapon.health == 0,
                "Weapon collection must set its original slot and type-specific amount.");

            auto duplicate = pickup(CollisionType::Weapon, type);
            duplicate.field_24 = 0xffff;
            duplicate.field_07 = 0xff;
            result = HandlePlayerObjectInteraction(duplicate, state);
            if (type == static_cast<std::int16_t>(WeaponType::Stone) ||
                type == static_cast<std::int16_t>(WeaponType::TrapOrPole))
            {
                Check(result.inventorySlot == 2 && state.weaponInventorySlots[type] == 2 &&
                    duplicate.health == 0, "Stone/trap weapons must permit duplicate inventory objects.");
            }
            else if (type == static_cast<std::int16_t>(WeaponType::Fireball))
            {
                Check(result.inventorySlot == 1 && state.inventory[0].field_24 == 0x8001 &&
                    state.weaponAmounts[type] == -32767 && duplicate.health == 0,
                    "Duplicate fireball weapons must merge their field-24 charges.");
            }
            else
            {
                Check(result.inventorySlot == 1 && duplicate.health == 1000 &&
                    duplicate.collisionType == CollisionType::None &&
                    duplicate.field_07 == 0 && duplicate.field_32 == 0 &&
                    state.inventory[1].health == 0,
                    "Other duplicate weapons become non-colliding world objects, not new inventory items.");
            }
        }
        state = {};
        auto specialWeapon = pickup(CollisionType::Weapon, -1);
        Check(HandlePlayerObjectInteraction(specialWeapon, state).inventorySlot == 1 &&
            state.weaponInventorySlots == GameState{}.weaponInventorySlots,
            "Negative weapon identifiers must skip the original table registration.");
        auto extendedWeapon = pickup(CollisionType::Weapon, 31);
        Check(HandlePlayerObjectInteraction(extendedWeapon, state).inventorySlot == 2 &&
            state.weaponInventorySlots[31] == 2,
            "Recovered tables must not be incorrectly truncated to the nine named weapon types.");

        state = {};
        RawGameObject grouped;
        grouped.health = 1000;
        grouped.collisionType = CollisionType::GroupedSwitch;
        grouped.behavior = BehaviorType::SwitchGrouped;
        grouped.itemValue = 5;
        grouped.groupIndexOrLock = 2;
        grouped.doorLockOrGroupBitmask = 3;
        Check(HandlePlayerObjectInteraction(grouped, state).status == InteractionStatus::Applied &&
            grouped.field_26 == 10 && grouped.field_24 == 5 && grouped.field_04 == 0x0200 &&
            state.switchGroupMasks[2] == 0xff,
            "Grouped interaction must arm the timer, start activation, and defer mask writes.");
        const auto movingSwitch = grouped;
        Check(HandlePlayerObjectInteraction(grouped, state).status == InteractionStatus::NoEffect &&
            sameObject(grouped, movingSwitch), "Moving switches must ignore reactivation.");
        for (int i = 0; i < 10; ++i) { DispatchObjectCase(grouped, state); }
        Check(grouped.blendWeightOrSpriteOpacity == 100 && grouped.field_24 == 4 &&
            state.switchGroupMasks[2] == 0xfc, "Grouped timer must start only at full activation.");
        for (int i = 0; i < 4; ++i) { DispatchObjectCase(grouped, state); }
        Check(grouped.field_24 == 0 && grouped.field_26 == 65526 &&
            state.switchGroupMasks[2] == 0xff, "Interaction and behavior must produce timed mask restoration.");
        for (int i = 0; i < 11; ++i) { DispatchObjectCase(grouped, state); }
        Check(grouped.blendWeightOrSpriteOpacity == 0 && grouped.field_26 == 0,
            "Grouped return animation must stop at the inactive endpoint.");
        grouped.blendWeightOrSpriteOpacity = 100;
        grouped.field_04 = 0x7f11;
        state.switchGroupMasks[2] = 0xfc;
        Check(HandlePlayerObjectInteraction(grouped, state).status == InteractionStatus::Applied &&
            grouped.field_26 == 65526 && grouped.field_24 == 5 && grouped.field_04 == 0x7f11 &&
            state.switchGroupMasks[2] == 0xfc, "Manual grouped return must not restore the mask immediately.");

        state = {};
        grouped = pickup(CollisionType::GroupedSwitch, 5);
        grouped.field_24 = 99;
        const auto switchPreview = grouped;
        result = HandlePlayerObjectInteraction(grouped, state);
        Check(result.inventorySlot == 1 && sameObject(state.inventory[0], switchPreview) &&
            grouped.health == 0 && grouped.field_26 == 10 && grouped.field_24 == 5,
            "Generic switch pickup must copy the pre-interaction snapshot, not the newly mutated world object.");
        for (auto& stored : state.inventory) { stored = pickup(CollisionType::Key); }
        grouped = switchPreview;
        result = HandlePlayerObjectInteraction(grouped, state);
        Check(result.status == InteractionStatus::InventoryFull && grouped.health == 1000 &&
            grouped.field_26 == 10 && grouped.field_24 == 5 &&
            state.inventory[0].collisionType == CollisionType::Key,
            "Original local activation must survive a subsequent generic inventory-full result.");

        state = {};
        key = pickup(CollisionType::Key);
        key.field_32 = 0;
        Check(HandlePlayerObjectInteraction(key, state).status == InteractionStatus::NoEffect &&
            key.health == 1000, "Unpickable generic objects must remain in the world.");
        auto ammo = pickup(CollisionType::Ammo, -1);
        ammo.field_32 = 0;
        Check(HandlePlayerObjectInteraction(ammo, state).status == InteractionStatus::NoEffect,
            "Unpickable ammo must not access its unused amount index.");
        key.health = -1;
        Check(HandlePlayerObjectInteraction(key, state).status == InteractionStatus::InactiveObject,
            "Host interaction requests must reject inactive selected objects.");
        for (const auto type : { CollisionType::BookWithCode, CollisionType::BuyItem,
            CollisionType::WheelToLiftRope, CollisionType::VaseToGrab, CollisionType::MirrorToGrab,
            CollisionType::LevelEnd, CollisionType::Teleporter, CollisionType::SarcophagusWithBones,
            CollisionType::Switch, CollisionType::Door, CollisionType::PictureMap, CollisionType::Lorry })
        {
            auto unsupported = pickup(type);
            const auto before = unsupported;
            Check(HandlePlayerObjectInteraction(unsupported, state).status == InteractionStatus::UnimplementedHandler &&
                sameObject(unsupported, before) && state.inventory[0].health == 0,
                "Unimplemented special interactions must not fall through into fake successful pickups.");
        }
        auto generic = pickup(static_cast<CollisionType>(255));
        Check(HandlePlayerObjectInteraction(generic, state).inventorySlot == 1,
            "Collision bytes without a specialized branch must preserve the original generic pickup tail.");
        for (const auto type : { CollisionType::PuzzleGameTile, CollisionType::PuzzleSpool,
            CollisionType::GrandfatherClockItem })
        {
            auto local = pickup(type);
            local.field_32 = 0;
            local.velocityX = 32767;
            local.field_24 = 1;
            Check(HandlePlayerObjectInteraction(local, state).status == InteractionStatus::Applied,
                "Known object-local interactions must execute.");
            Check(type != CollisionType::PuzzleGameTile || local.field_28 == 1,
                "Puzzle tile interaction must set field 28.");
            Check(type != CollisionType::PuzzleSpool || local.velocityX == 1,
                "Puzzle spool interaction must set velocity X.");
            Check(type != CollisionType::GrandfatherClockItem || local.velocityX == -32768,
                "Clock interaction must add a wrapped signed word.");
        }

        state = {};
        gold = pickup(CollisionType::Gold, 10);
        gold.behavior = BehaviorType::IdleAnimation;
        DispatchObjectCase(gold, state);
        Check(gold.health == 1000 && state.gold == 0 && state.inventory[0].health == 0,
            "Per-frame behavior must never implicitly request interaction.");
        state.behaviorDispatchEnabled = false;
        HandlePlayerObjectInteraction(gold, state);
        Check(state.gold == 10 && gold.health == 0,
            "Explicit interaction must not be gated by the separate behavior-cascade flag.");
        GameState independent;
        Check(independent.gold == 0 && independent.arrowPoison == 0 &&
            !independent.hasMap && independent.inventory[0].health == 0,
            "Interaction state must belong to each game context.");
    }

    void TestGameMath()
    {
        for (int bits = 0; bits <= 65535; ++bits)
        {
            const auto value = ReferenceSignedWord(bits);
            Check(WrapSignedWord(bits) == value && SignWord(value) == (value < 0 ? -1 : value > 0 ? 1 : 0),
                "Signed-word conversion/sign differs from the original.");
            for (const auto step : { 0, 1, 2, 32767, -32768, -1 })
            {
                for (const auto target : { 0, 32767, -32768 })
                {
                    Check(GetStepToward(value, static_cast<std::int16_t>(target), static_cast<std::int16_t>(step)) ==
                        ReferenceStep(value, static_cast<std::int16_t>(target), static_cast<std::int16_t>(step)),
                        "Step helper differs at a signed/absolute-value boundary.");
                }
            }
            std::int16_t whole = 32767;
            std::int16_t fraction = 250;
            const auto sumBits = static_cast<std::uint16_t>((250u + static_cast<unsigned int>(bits)) % 65536);
            const int highByte = sumBits / 256;
            const int carry = highByte >= 128 ? highByte - 256 : highByte;
            AddFixedPoint8(whole, fraction, value);
            Check(whole == ReferenceSignedWord(32767LL + carry) && fraction == sumBits % 256,
                "Fixed-point helper must use the signed high-byte carry and retain only the low byte.");
        }
        std::int16_t aliased = 300;
        AddFixedPoint8(aliased, aliased, 1);
        Check(aliased == 46, "Fixed-point reference update order must match the original when arguments alias.");
    }

    void TestSmokeFading()
    {
        RawGameObject object;
        for (std::size_t offset = 0; offset < sizeof(object); offset += 2)
        {
            object.WriteWord(offset, static_cast<std::uint16_t>(0x9876u + offset));
        }
        object.behavior = BehaviorType::SmokeFading;
        object.health = 40;
        object.posY = 100;
        object.itemValue = 250;
        object.velocityY = 300;
        object.velocityZ = 3;
        object.spriteScale = 0xc734;
        GameState state{ 2, true };
        auto expected = object;
        expected.posY = 103;
        expected.itemValue = 82;
        expected.blendWeightOrSpriteOpacity = 5;
        expected.spriteScale = 0xcd34;
        expected.velocityY = 296;
        const auto result = DispatchObjectCase(object, state);
        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1a9d &&
            result.behavior == BehaviorType::SmokeFading, "SmokeFading did not execute.");
        Check(object.ReadBytes() == expected.ReadBytes() && state.collisionMap == nullptr &&
            state.tickCount == 2 && state.behaviorDispatchEnabled,
            "SmokeFading must modify only Y, fraction, opacity, scale high byte, and vertical velocity.");

        for (int bits = 0; bits <= 65535; ++bits)
        {
            for (const auto ticks : { 0, 1, 16384, 32767 })
            {
                object.posY = ReferenceSignedWord(bits * 3LL);
                object.itemValue = ReferenceSignedWord(bits * 19LL);
                object.velocityY = ReferenceSignedWord(bits);
                object.velocityZ = ReferenceSignedWord(bits * 5LL);
                object.health = ReferenceSignedWord(bits);
                object.spriteScale = static_cast<std::uint16_t>(bits ^ 0xc7a5);
                state.tickCount = static_cast<std::int16_t>(ticks);
                auto words = object.ReadUnsignedWords();
                const auto sum = (static_cast<std::uint64_t>(words[10]) * ticks + words[17]) % 65536;
                const auto carry = sum / 256 >= 128 ? static_cast<std::int64_t>(sum / 256) - 256 :
                    static_cast<std::int64_t>(sum / 256);
                words[7] = static_cast<std::uint16_t>(ReferenceSignedWord(words[7] + carry));
                words[17] = static_cast<std::uint16_t>(sum % 256);
                const auto shiftedHealth = static_cast<std::uint16_t>((words[5] >> 2) |
                    ((words[5] & 0x8000) != 0 ? 0xc000 : 0));
                words[21] = ReferenceSignedWord(shiftedHealth) > 5 ? 5 : shiftedHealth;
                const auto highByte = words[26] / 256;
                if (highByte < 200)
                {
                    const auto newHighByte = (highByte + static_cast<std::uint64_t>(words[11]) * ticks) % 256;
                    words[26] = static_cast<std::uint16_t>((words[26] % 256) + newHighByte * 256);
                }
                const auto velocity = ReferenceSignedWord(words[10]);
                words[10] = static_cast<std::uint16_t>(ReferenceSignedWord(static_cast<std::int64_t>(velocity) +
                    ReferenceStep(velocity, 0, ReferenceSignedWord(ticks * 2LL))));
                Check(Function_SmokeFading(object, state).status == DispatchStatus::ExecutedCase &&
                    object.ReadUnsignedWords() == words,
                    "SmokeFading differs from the original word/byte arithmetic or changed unrelated data.");
            }
        }
        object.health = 0;
        const auto inactive = object.ReadBytes();
        Check(DispatchObjectCase(object, state).status == DispatchStatus::InactiveObject &&
            object.ReadBytes() == inactive, "Inactive smoke must not be updated.");
        object.health = 1;
        state.behaviorDispatchEnabled = false;
        const auto disabled = object.ReadBytes();
        Check(DispatchObjectCase(object, state).status == DispatchStatus::DispatchDisabled &&
            object.ReadBytes() == disabled, "Disabled smoke must not be updated.");
    }

    void TestSwitches()
    {
        GameState initial;
        for (const auto mask : initial.switchGroupMasks)
        {
            Check(mask == 0xff, "All five group masks must initialize to FF as in the executable.");
        }
        auto independent = initial;
        independent.switchGroupMasks[0] = 0;
        Check(initial.switchGroupMasks[0] == 0xff, "Game states must not share group-mask storage.");
        initial.switchGroupMasks[1] = 0x55;
        initial.switchGroupMasks[2] = 0xaa;
        initial.switchGroupMasks[3] = 0;
        initial.switchGroupMasks[4] = 0x33;

        const auto checkUpdate = [&](RawGameObject object, std::int16_t ticks)
        {
            auto expected = object;
            auto state = initial;
            state.tickCount = ticks;
            auto expectedState = state;
            const bool grouped = object.behavior == BehaviorType::SwitchGrouped;
            if (!grouped && expected.field_24 == 0) { expected.field_24 = 10; }
            const auto sum = static_cast<std::int64_t>(static_cast<std::uint16_t>(object.blendWeightOrSpriteOpacity)) +
                static_cast<std::int64_t>(object.field_26) * static_cast<std::uint16_t>(ticks);
            auto phase = ReferenceSignedWord(sum);
            if (phase < 0) { phase = 0; expected.field_26 = 0; }
            if (phase > 100)
            {
                phase = 100;
                expected.field_26 = grouped ? 0 :
                    static_cast<std::uint16_t>(ReferenceSignedWord(-static_cast<std::int64_t>(expected.field_24)));
            }
            expected.blendWeightOrSpriteOpacity = phase;
            if (grouped)
            {
                expected.collisionType = CollisionType::GroupedSwitch;
                const auto mask = static_cast<std::uint16_t>(object.doorLockOrGroupBitmask) % 256;
                auto& group = expectedState.switchGroupMasks[object.groupIndexOrLock];
                if (phase == 100) { group = static_cast<std::uint8_t>(group & (255 - mask)); }
                if (phase == 0) { group = static_cast<std::uint8_t>(group | mask); }
                if (phase == 100 && ReferenceSignedWord(expected.field_24) > 0 && object.itemValue > 0)
                {
                    const auto countdown = ReferenceSignedWord(static_cast<std::int64_t>(expected.field_24) - ticks);
                    expected.field_24 = countdown < 1 ? 0 : static_cast<std::uint16_t>(countdown);
                    if (countdown < 1)
                    {
                        expected.field_26 = 0xfff6;
                        group = static_cast<std::uint8_t>(group | mask);
                    }
                }
            }
            else if (phase == 100 && object.spriteScale % 256 != 0)
            {
                expected.field_26 = 0;
            }
            const auto result = DispatchObjectCase(object, state);
            Check(result.status == DispatchStatus::ExecutedCase && result.behavior == expected.behavior &&
                result.moduleOffset == (grouped ? 0x2065 : 0x1fdd),
                "Switch case did not execute at its original address.");
            Check(object.ReadBytes() == expected.ReadBytes(),
                "Switch differs from original word arithmetic or changed an unrelated object field.");
            Check(std::equal(std::begin(state.switchGroupMasks), std::end(state.switchGroupMasks),
                std::begin(expectedState.switchGroupMasks)) && state.tickCount == ticks &&
                state.behaviorDispatchEnabled && state.collisionMap == nullptr,
                "Switch changed the wrong group bits or unrelated context state.");
        };

        RawGameObject object;
        auto* bytes = reinterpret_cast<unsigned char*>(&object);
        for (std::size_t i = 0; i < sizeof(object); ++i) { bytes[i] = static_cast<unsigned char>(0xa5 + i); }
        object.health = 1;
        object.itemValue = 1;
        for (const auto behavior : { BehaviorType::Switch1, BehaviorType::SwitchGrouped })
        {
            object.behavior = behavior;
            for (int bits = 0; bits <= 65535; ++bits)
            {
                object.groupIndexOrLock = static_cast<std::int16_t>(bits % 5);
                object.doorLockOrGroupBitmask = ReferenceSignedWord(bits);
                for (const auto ticks : { 0, 1, 32767 })
                {
                    object.spriteScale = bits % 3 == 0 ? 1 : bits % 3 == 1 ? 0x100 : 0;
                    object.field_24 = 10;
                    object.field_26 = static_cast<std::uint16_t>(bits);
                    object.blendWeightOrSpriteOpacity = 95;
                    checkUpdate(object, static_cast<std::int16_t>(ticks));
                    object.field_26 = 10;
                    object.blendWeightOrSpriteOpacity = ReferenceSignedWord(bits);
                    checkUpdate(object, static_cast<std::int16_t>(ticks));
                    object.spriteScale = 0;
                    object.field_24 = static_cast<std::uint16_t>(bits);
                    object.field_26 = behavior == BehaviorType::SwitchGrouped ? 0 : 1;
                    object.blendWeightOrSpriteOpacity = 100;
                    checkUpdate(object, static_cast<std::int16_t>(ticks));
                }
            }
            for (const auto enable : { -32768, -1, 0, 1, 32767 })
            {
                object.itemValue = static_cast<std::int16_t>(enable);
                object.field_24 = 3;
                object.field_26 = 0;
                object.blendWeightOrSpriteOpacity = 100;
                checkUpdate(object, 3);
            }
            object.itemValue = 1;
        }

        GameState shared;
        shared.switchGroupMasks[2] = 3;
        RawGameObject first;
        first.behavior = BehaviorType::SwitchGrouped;
        first.health = 1;
        first.groupIndexOrLock = 2;
        first.doorLockOrGroupBitmask = 1;
        first.itemValue = 1;
        first.field_24 = 3;
        first.field_26 = 10;
        first.blendWeightOrSpriteOpacity = 90;
        auto second = first;
        second.doorLockOrGroupBitmask = 2;
        second.itemValue = 0;
        DispatchObjectCase(first, shared);
        DispatchObjectCase(second, shared);
        Check(shared.switchGroupMasks[2] == 0 && first.field_24 == 2,
            "Two grouped switches must combine to clear their shared group.");
        DispatchObjectCase(first, shared);
        Check(first.field_24 == 1 && shared.switchGroupMasks[2] == 0,
            "An active switch must count elapsed ticks while holding its bits clear.");
        DispatchObjectCase(first, shared);
        Check(first.field_24 == 0 && first.field_26 == 0xfff6 &&
            first.blendWeightOrSpriteOpacity == 100 && shared.switchGroupMasks[2] == 1,
            "Timer expiry must restore bits immediately, before the return animation.");
        DispatchObjectCase(first, shared);
        DispatchObjectCase(second, shared);
        Check(first.blendWeightOrSpriteOpacity == 90 && shared.switchGroupMasks[2] == 1,
            "Another active switch must not clear an expired switch's different bits.");

        for (const auto invalid : { -32768, -1, 5, 32767 })
        {
            first.groupIndexOrLock = static_cast<std::int16_t>(invalid);
            const auto before = first.ReadBytes();
            const auto masksBefore = shared;
            bool rejected = false;
            try { DispatchObjectCase(first, shared); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && first.ReadBytes() == before &&
                std::equal(std::begin(shared.switchGroupMasks), std::end(shared.switchGroupMasks),
                    std::begin(masksBefore.switchGroupMasks)),
                "Invalid group indices must fail explicitly without modifying object or masks.");
        }
        for (const auto behavior : { BehaviorType::Switch1, BehaviorType::SwitchGrouped })
        {
            first.behavior = behavior;
            first.health = 0;
            const auto before = first.ReadBytes();
            const auto masksBefore = shared;
            Check(DispatchObjectCase(first, shared).status == DispatchStatus::InactiveObject &&
                first.ReadBytes() == before, "Inactive switches must not update or access group state.");
            first.health = 1;
            shared.behaviorDispatchEnabled = false;
            const auto disabled = first.ReadBytes();
            Check(DispatchObjectCase(first, shared).status == DispatchStatus::DispatchDisabled &&
                first.ReadBytes() == disabled &&
                std::equal(std::begin(shared.switchGroupMasks), std::end(shared.switchGroupMasks),
                    std::begin(masksBefore.switchGroupMasks)),
                "Disabled switches must not update or access group state.");
            shared.behaviorDispatchEnabled = true;
        }
    }

    void TestIdleAnimation()
    {
        RawGameObject object;
        for (std::size_t offset = 0; offset < sizeof(object); offset += 2)
        {
            object.WriteWord(offset, static_cast<std::uint16_t>(0x9876u + offset));
        }
        object.behavior = BehaviorType::IdleAnimation;
        object.health = 1;
        object.itemValue = 3;
        object.blendWeightOrSpriteOpacity = 195;
        GameState state{ 4, true };
        auto expected = object;
        expected.blendWeightOrSpriteOpacity = 7;
        const auto result = DispatchObjectCase(object, state);
        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1666 &&
            result.behavior == BehaviorType::IdleAnimation, "IdleAnimation did not execute.");
        Check(object.ReadBytes() == expected.ReadBytes(),
            "IdleAnimation must update only the blend-phase word.");
        Check(state.tickCount == 4 && state.behaviorDispatchEnabled && state.collisionMap == nullptr,
            "IdleAnimation must not modify shared state or require a collision map.");

        const auto checkArithmetic = [&](std::uint16_t rate, std::uint16_t phase, std::int16_t ticks)
        {
            object.WriteWord(34, rate);
            object.WriteWord(42, phase);
            state.tickCount = ticks;
            const auto lowWord = (static_cast<std::uint64_t>(rate) * static_cast<std::uint16_t>(ticks) +
                phase + 200) % 65536;
            const auto signedSum = lowWord >= 32768 ? static_cast<std::int64_t>(lowWord) - 65536 :
                static_cast<std::int64_t>(lowWord);
            const auto remainder = signedSum < 0 ? -((-signedSum) % 200) : signedSum % 200;
            auto expectedObject = object;
            expectedObject.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(remainder);
            Check(Function_IdleAnimation(object, state).status == DispatchStatus::ExecutedCase &&
                object.ReadBytes() == expectedObject.ReadBytes(),
                "IdleAnimation differs from the original low-word multiply/add and signed remainder.");
        };
        for (const auto rate : { 0, 1, 3, 200, 0x7fff, 0x8000, 0xffff })
        {
            for (const auto phase : { 0, 100, 195, 199, 200, 0x7fff, 0x8000, 0xffff })
            {
                for (const auto ticks : { 0, 1, 4, 15, 32767 })
                {
                    checkArithmetic(static_cast<std::uint16_t>(rate), static_cast<std::uint16_t>(phase),
                        static_cast<std::int16_t>(ticks));
                }
            }
        }
        for (int bits = 0; bits <= 65535; ++bits)
        {
            for (const auto ticks : { 1, 32767 })
            {
                checkArithmetic(3, static_cast<std::uint16_t>(bits), static_cast<std::int16_t>(ticks));
                checkArithmetic(0xffff, static_cast<std::uint16_t>(bits), static_cast<std::int16_t>(ticks));
                checkArithmetic(static_cast<std::uint16_t>(bits), 195, static_cast<std::int16_t>(ticks));
                checkArithmetic(static_cast<std::uint16_t>(bits), 0x8000, static_cast<std::int16_t>(ticks));
            }
        }
        object.itemValue = 0;
        object.blendWeightOrSpriteOpacity = -300;
        state.tickCount = 0;
        Function_IdleAnimation(object, state);
        Check(object.blendWeightOrSpriteOpacity == -100, "Negative remainder must not be normalized positive.");
        object.health = 0;
        const auto inactive = object.ReadBytes();
        Check(DispatchObjectCase(object, state).status == DispatchStatus::InactiveObject &&
            object.ReadBytes() == inactive, "Inactive IdleAnimation must not change the phase.");
        object.health = 1;
        state.behaviorDispatchEnabled = false;
        const auto disabled = object.ReadBytes();
        Check(DispatchObjectCase(object, state).status == DispatchStatus::DispatchDisabled &&
            object.ReadBytes() == disabled, "Disabled IdleAnimation must not change the phase.");
    }

    void TestCollisionMap()
    {
        auto level = CreateDummyLevel();
        Check(level.collisionMap != nullptr, "Dummy level must own a heap-allocated collision map.");
        auto& map = *level.collisionMap;
        Check(sizeof(map) == 200 * 200 + 4, "Dummy allocation must contain only its header and pixels.");
        Check(map.widthMinusOne == 199 && map.heightMinusOne == 199,
            "Dummy level must contain a 200x200 collision map.");
        Check((static_cast<std::size_t>(map.widthMinusOne) + 1) * (map.heightMinusOne + 1) == 40000,
            "Dummy level must expose exactly 40000 logical collision pixels.");
        for (const auto value : map.data)
        {
            Check(value == 0, "Dummy level must initialize all collision-map storage to zero.");
        }
        auto* bytes = reinterpret_cast<unsigned char*>(&map);
        Check(map.data == bytes + 4,
            "Map pixels must begin immediately after the four-byte header.");
        map.widthMinusOne = 7;
        std::fill(std::begin(map.data), std::end(map.data), std::uint8_t{0xa5});
        GameState context;
        context.collisionMap = &map;
        const auto before = map;
        const auto* beforeBytes = reinterpret_cast<const unsigned char*>(&before);
        context.SetCollisionMapPixelAtWorldXZ(137, 63, 0x1238);
        const std::size_t expected = 4 + 161 * 8 + 1;
        for (std::size_t i = 0; i < sizeof(map); ++i)
        {
            Check(bytes[i] == (i == expected ? 0x38 : beforeBytes[i]),
                "Point writer must overwrite exactly one byte with the low argument byte.");
        }

        RawGameObject object;
        object.behavior = BehaviorType::Pillar;
        object.WriteWord(10, 1);
        object.WriteWord(12, 137);
        object.WriteWord(14, 0x8000);
        object.WriteWord(16, 63);
        const auto objectBefore = object.ReadBytes();
        for (const auto ticks : { 0, 1, 15 })
        {
            context.tickCount = static_cast<std::int16_t>(ticks);
            bytes[expected] = 0xff;
            Check(DispatchObjectCase(object, context).status == DispatchStatus::ExecutedCase,
                "Pillar did not execute its recovered case.");
            Check(bytes[expected] == 0x38 && object.ReadBytes() == objectBefore,
                "Pillar must use X/Z, ignore Y/ticks, and leave the object unchanged.");
        }
        context.collisionMap = nullptr;
        object.WriteWord(10, 0);
        Check(DispatchObjectCase(object, context).status == DispatchStatus::InactiveObject,
            "Inactive Pillar must not access the map.");
        object.WriteWord(10, 1);
        context.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(object, context).status == DispatchStatus::DispatchDisabled,
            "Disabled Pillar must not access the map.");
        context.behaviorDispatchEnabled = true;
        bool rejected = false;
        try { DispatchObjectCase(object, context); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Missing map memory must be an explicit error.");

        // Independent modular-arithmetic oracle covers every signed X and Z input.
        const auto signed16 = [](std::int64_t value) -> std::int64_t
        {
            value = (value % 65536 + 65536) % 65536;
            return value >= 32768 ? value - 65536 : value;
        };
        context.collisionMap = &map;
        for (const auto strideHeader : { 0, 7, 0x7fff, 0xffff })
        {
            map.widthMinusOne = static_cast<std::uint16_t>(strideHeader);
            const auto stride = (strideHeader + 1) % 65536;
            for (int raw = 0; raw < 65536; ++raw)
            {
                const auto coordinate = static_cast<std::int16_t>(signed16(raw));
                for (const bool varyX : { false, true })
                {
                    const std::int16_t x = varyX ? coordinate : 137;
                    const std::int16_t z = varyX ? 63 : coordinate;
                    const auto column = signed16(static_cast<std::int64_t>(x) - 112) / 25;
                    const auto row = signed16(162 - signed16(static_cast<std::int64_t>(z) - 38) / 25);
                    const auto originalAddress =
                        (65534 + row * stride + column + 4 + 0x100000000LL) % 65536;
                    const auto address = static_cast<std::size_t>((originalAddress - 65534 + 65536) % 65536);
                    if (address >= sizeof(map))
                    {
                        bool outOfAllocation = false;
                        try { context.SetCollisionMapPixelAtWorldXZ(x, z, 0x1ab); }
                        catch (const std::out_of_range&) { outOfAllocation = true; }
                        Check(outOfAllocation, "Writes beyond the actual allocation must fail explicitly.");
                        continue;
                    }
                    const auto previous = bytes[address];
                    context.SetCollisionMapPixelAtWorldXZ(x, z, 0x1ab);
                    Check(bytes[address] == 0xab,
                        "Point writer differs from 16-bit coordinate/address arithmetic.");
                    bytes[address] = previous;
                }
            }
        }
        map.widthMinusOne = 7;
        context.SetCollisionMapPixelAtWorldXZ(62, 4088, 0x12);
        Check(bytes[2] == 0x12, "Original offset wrapping must still be able to reach the header.");
        rejected = false;
        try { context.SetCollisionMapPixelAtWorldXZ(-13, 4088, 0x34); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "A wrapped address outside the allocated map must fail explicitly.");
        const auto fresh = CreateDummyLevel();
        Check(fresh.collisionMap->widthMinusOne == 199 && fresh.collisionMap->data[expected - 4] == 0 &&
            fresh.collisionMap.get() != level.collisionMap.get(),
            "Dummy levels must not share mutable collision-map state.");
    }

    void TestCollisionMapReader()
    {
        auto level = CreateDummyLevel();
        auto& map = *level.collisionMap;
        GameState context;
        context.collisionMap = &map;
        const GameState& reader = context;
        Check(reader.GetCollisionMapPixelAtWorldXZ(137, 63) == 0,
            "Reader must return zero from a fresh dummy map.");
        context.SetCollisionMapPixelAtWorldXZ(137, 63, 0x1238);
        Check(reader.GetCollisionMapPixelAtWorldXZ(137, 63) == 0x38,
            "Reader and writer must address the same world-coordinate pixel.");
        for (std::size_t i = 0; i < CollisionMap::VARIABLE_SIZE; ++i)
        {
            map.data[i] = static_cast<std::uint8_t>(i * 37 + 11);
        }
        map.heightMinusOne = 0;
        const auto* bytes = reinterpret_cast<const unsigned char*>(&map);
        const auto signed16 = [](std::int64_t value) -> std::int64_t
        {
            value = (value % 65536 + 65536) % 65536;
            return value >= 32768 ? value - 65536 : value;
        };
        for (const auto widthHeader : { 0, 7, 199, 0x7fff, 0xfffe, 0xffff })
        {
            map.widthMinusOne = static_cast<std::uint16_t>(widthHeader);
            const auto before = map;
            const auto checkRead = [&](std::int16_t x, std::int16_t z)
            {
                const auto column = signed16(static_cast<std::int64_t>(x) - 112) / 25;
                const auto row = signed16(162 - signed16(static_cast<std::int64_t>(z) - 38) / 25);
                if (column < 0 || column > 319 || row < 0 || row > 239)
                {
                    Check(reader.GetCollisionMapPixelAtWorldXZ(x, z) == 0,
                        "Original hardcoded coordinate bounds must return zero.");
                    return;
                }
                const auto stride = (widthHeader + 1) % 65536;
                const auto address = static_cast<std::size_t>((4 + row * stride + column) % 65536);
                if (address >= sizeof(map))
                {
                    bool rejected = false;
                    try { reader.GetCollisionMapPixelAtWorldXZ(x, z); }
                    catch (const std::out_of_range&) { rejected = true; }
                    Check(rejected, "Reads beyond the host allocation must fail explicitly.");
                }
                else
                {
                    Check(reader.GetCollisionMapPixelAtWorldXZ(x, z) == bytes[address],
                        "Reader differs from the original coordinate, stride, or offset arithmetic.");
                }
            };
            for (int raw = 0; raw < 65536; ++raw)
            {
                const auto coordinate = static_cast<std::int16_t>(signed16(raw));
                checkRead(coordinate, 63);
                checkRead(137, coordinate);
            }
            for (int row = -1; row <= 240; ++row)
            {
                for (int column = -1; column <= 320; ++column)
                {
                    checkRead(static_cast<std::int16_t>(112 + column * 25),
                        static_cast<std::int16_t>(38 + (162 - row) * 25));
                }
            }
            const auto* beforeBytes = reinterpret_cast<const unsigned char*>(&before);
            Check(std::equal(bytes, bytes + sizeof(map), beforeBytes),
                "Reading pixels must not modify the header or data.");
        }
        context.collisionMap = nullptr;
        bool rejected = false;
        try { reader.GetCollisionMapPixelAtWorldXZ(137, 63); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Reading without collision-map memory must fail explicitly.");
    }

    void TestGatesAndWords()
    {
        RawGameObject object;
        GameState context;
        object.behavior = BehaviorType::BatIndoor;
        for (const auto health : { 0, 0xffff, 0x8000 })
        {
            object.WriteWord(10, static_cast<std::uint16_t>(health));
            Check(DispatchObjectCase(object, context).status == DispatchStatus::InactiveObject,
                "Health gate must use a signed > 0 check, not != 0.");
        }
        object.WriteWord(10, 1);
        context.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(object, context).status == DispatchStatus::DispatchDisabled,
            "Disabled behavior dispatch was executed.");
        context.behaviorDispatchEnabled = true;
        Check(DispatchObjectCase(object, context).moduleOffset == 0x161d, "BatIndoor anchor changed.");
        object.behavior = BehaviorType::Behavior_300;
        Check(DispatchObjectCase(object, context).moduleOffset == 0x5108, "Behavior 300 must not be truncated.");
        object.behavior = BehaviorType::DisabledSentinel;
        Check(DispatchObjectCase(object, context).moduleOffset == 0x1156, "FFFF sentinel changed.");
        object.WriteWord(8, 0x010e);
        const auto unmapped = DispatchObjectCase(object, context);
        Check(unmapped.status == DispatchStatus::NoSpecificCase && unmapped.behavior == object.behavior &&
            FindBehavior(object.behavior) == nullptr,
            "A nonzero high byte must not route to BatIndoor.");
        object.WriteWord(12, 0xff9c);
        Check(object.ReadSignedWord(12) == -100, "Signed little-endian position was misread.");
        object.WriteWord(52, 0xffff);
        Check(object.ReadWord(52) == 65535 && object.ReadSignedWord(52) == -1, "Last word was misread.");
        bool rejected = false;
        try { object.ReadWord(53); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "Out-of-bounds word read was accepted.");
        rejected = false;
        try { object.WriteWord(std::numeric_limits<std::size_t>::max(), 1); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "Overflowing word offset was accepted.");
        context.tickCount = -1;
        rejected = false;
        try { DispatchObjectCase(object, context); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Invalid tick count was accepted.");
    }

    void TestHeightfields()
    {
        auto level = CreateDummyLevel();
        GameState state;
        state.level = &level;
        std::array<std::uint8_t, Level::GeometryStreamSize + 5> data{};
        data[0] = 49;
        data[2] = 39;
        constexpr auto planeSize = Level::Width * Level::Height;
        for (std::size_t plane = 0; plane < 7; ++plane)
        {
            for (std::size_t i = 0; i < planeSize; ++i)
            {
                data[4 + plane * planeSize + i] = static_cast<std::uint8_t>(plane * 31 + i * 7);
            }
        }
        level.collisionMap->data[127] = 123;
        level.LoadGeometry(data.data(), data.size());
        const auto checkPlanes = [&]()
        {
            for (std::size_t row = 0; row < Level::Height; ++row)
            {
                for (std::size_t column = 0; column < Level::Width; ++column)
                {
                    const auto index = row * Level::Width + column;
                    Check(level.ceilingObjects[row][column] == static_cast<std::uint8_t>(62 + index * 7) &&
                        level.floorHeights[row][column] == static_cast<std::uint8_t>(93 + index * 7) &&
                        level.roomHeights[row][column] == static_cast<std::uint8_t>(124 + index * 7) &&
                        level.surfaceFlags[row][column] == static_cast<std::uint8_t>(155 + index * 7),
                        "Geometry plane offsets, orientation, or retained values differ from the renderer.");
                }
            }
            Check(level.collisionMap->data[127] == 123, "Geometry loading modified the independent collision map.");
        };
        checkPlanes();
        for (int invalid = 0; invalid < 4; ++invalid)
        {
            bool rejected = false;
            data[0] = invalid == 2 ? 48 : 49;
            data[3] = invalid == 3 ? 1 : 0;
            try
            {
                level.LoadGeometry(invalid == 0 ? nullptr : data.data(),
                    invalid == 1 ? Level::GeometryStreamSize - 1 : data.size());
            }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "Invalid or truncated geometry was accepted.");
            checkPlanes();
        }

        level = CreateDummyLevel();
        const auto referenceHeight = [](const std::array<int, 4>& corners, int dx, int dz)
        {
            const auto wx = static_cast<std::uint64_t>(dx * 661);
            const auto wz = static_cast<std::uint64_t>(dz * 661);
            const auto lower = (static_cast<std::uint16_t>(corners[3]) * wx +
                static_cast<std::uint16_t>(corners[2]) * (65535 - wx)) / 65536;
            const auto upper = (static_cast<std::uint16_t>(corners[1]) * wx +
                static_cast<std::uint16_t>(corners[0]) * (65535 - wx)) / 65536;
            return static_cast<int>((lower * (65535 - wz) + upper * wz) / 65536);
        };
        const std::array<int, 4> floor = { 10, 40, 70, 110 };
        const std::array<int, 4> room = { 22, 55, 66, 123 };
        for (std::size_t i = 0; i < 4; ++i)
        {
            level.floorHeights[13 + i / 2][11 + i % 2] = static_cast<std::uint8_t>(floor[i]);
            level.roomHeights[13 + i / 2][11 + i % 2] = static_cast<std::uint8_t>(room[i]);
        }
        for (int dx = 0; dx < 100; ++dx)
        {
            for (int dz = 0; dz < 100; ++dz)
            {
                const auto x = static_cast<std::int16_t>(1200 + dx);
                const auto z = static_cast<std::int16_t>(2700 + dz);
                Check(state.GetFloorHeight(x, 0, z) == -2 * referenceHeight(floor, dx, dz) &&
                    state.GetRoomHeight(x, z) == 2 * referenceHeight(room, dx, dz),
                    "Native fixed-point interpolation or Pascal row/column conversion is incorrect.");
            }
        }
        for (int raw = 0; raw <= 255; ++raw)
        {
            for (std::size_t i = 0; i < 4; ++i)
            {
                level.floorHeights[i / 2][i % 2] = static_cast<std::uint8_t>(raw);
                level.roomHeights[i / 2][i % 2] = static_cast<std::uint8_t>(raw);
            }
            Check(state.GetFloorHeight(100, 0, 4000) == -2 * std::max(0, raw - 2) &&
                state.GetRoomHeight(100, 4000) == 2 * std::max(0, raw - 2),
                "Native 65535-weight/truncation bias was replaced by floating-point interpolation.");
        }
        level.ceilingObjects[0][0] = 0;
        Check(state.GetRoomHeight(100, 4000) == 20000, "Missing ceiling must return the native relative-height sentinel.");
        level.ceilingObjects[39][49] = 0;
        Check(state.GetRoomHeight(5000, 100) == 20000,
            "No-ceiling lookup must not require unused neighboring samples at the level edge.");
        Check(state.GetFloorHeight(4900, 0, 200) == 0 && state.GetRoomHeight(4900, 200) == 96,
            "Last complete interpolation cell was rejected or mapped to the wrong row.");
        for (const auto x : { 0, 5000, 12799, 12800, -12900 })
        {
            bool rejected = false;
            bool overflow = false;
            try { state.GetFloorHeight(static_cast<std::int16_t>(x), 0, 4000); }
            catch (const std::overflow_error&) { rejected = true; overflow = true; }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && overflow == (x == 12800 || x == -12900),
                "Height lookup failed to distinguish native byte-division overflow from sample bounds.");
        }
        bool rejected = false;
        try { state.GetRoomHeight(100, -12900); }
        catch (const std::overflow_error&) { rejected = true; }
        Check(rejected, "Room-height Z division must also detect native quotient overflow.");
        state.level = nullptr;
        rejected = false;
        try { state.GetRoomHeight(100, 4000); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Missing room geometry must not silently return zero.");
        state.level = &level;

        for (std::size_t i = 0; i < 4; ++i)
        {
            level.floorHeights[i / 2][i % 2] = 100;
            level.roomHeights[i / 2][i % 2] = 50;
        }
        level.surfaceFlags[0][0] = 1;
        rejected = false;
        try { state.GetFloorHeight(150, 0, 4050); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Special-surface geometry must require its separate height/value metadata.");
        for (std::size_t slot = 0; slot < level.heightSurfaces.size(); ++slot)
        {
            level.heightSurfaces[slot] = Level::HeightSurface{
                static_cast<std::int16_t>(100 + slot), static_cast<std::uint8_t>(7 + slot) };
            level.surfaceFlags[0][0] = static_cast<std::uint8_t>(0x20u | (slot + 1));
            Check(state.GetFloorHeight(150, 0, 4050) == -100 - static_cast<int>(slot) &&
                state.floorSurfaceValue == 7 + static_cast<int>(slot),
                "Floor surface slots, flag mask, or shared lookup value are incorrect.");
        }
        level.surfaceFlags[0][0] = 1;
        for (const auto depth : { 0, 900, 901, -200, -32768 })
        {
            level.heightSurfaces[0] = Level::HeightSurface{ static_cast<std::int16_t>(depth), 42 };
            const auto flat = ReferenceSignedWord(-depth);
            Check(state.GetFloorHeight(150, 0, 4050) == (flat < -900 ? -196 : flat) &&
                state.floorSurfaceValue == 42, "Special-surface fallback threshold or signed negation is incorrect.");
        }
        level.surfaceFlags[0][0] = 5;
        Check(state.GetFloorHeight(150, 0, 4050) == -196 && state.floorSurfaceValue == -1,
            "Non-surface terrain must reset the surface lookup value.");
        level.surfaceFlags[0][0] = 28;
        Check(state.GetFloorHeight(150, -90, 4050) == -96 &&
            state.GetFloorHeight(150, -91, 4050) == -196,
            "Stacked-floor selection must use signed maximum, strict threshold, and arithmetic half-height.");
        for (std::size_t i = 0; i < 4; ++i) { level.floorHeights[i / 2][i % 2] = 20; }
        Check(state.GetFloorHeight(150, 100, 4050) == 64,
            "Negative stacked-floor corners must retain the native unsigned multiplication semantics.");
        for (std::size_t i = 0; i < 4; ++i) { level.floorHeights[i / 2][i % 2] = 100; }
        level.surfaceFlags[0][0] = 16;
        state.floorRepeatHeight = 50;
        Check(state.GetFloorHeight(150, -100, 4050) == -96 &&
            state.GetFloorHeight(150, 0, 4050) == 0 &&
            state.GetFloorHeight(150, -800, 4050) == -506,
            "Repeated-floor stepping or signed 0..255 clamp differs from the executable.");
        level.surfaceFlags[0][0] = 0;
        Check(state.GetFloorHeight(150, -100, 4050) == -196,
            "Zero surface flags must bypass repeated-floor adjustment.");
        level.surfaceFlags[0][0] = 28;
        for (std::size_t i = 0; i < 4; ++i) { level.floorHeights[i / 2][i % 2] = 20; }
        Check(state.GetFloorHeight(150, 100, 4050) == 0,
            "Negative stacked-floor corners must use signed remainder before repeated-floor stepping.");
    }

    void TestWaterDrop()
    {
        auto level = CreateDummyLevel();
        GameState state;
        state.level = &level;
        RawGameObject drop;
        drop.behavior = BehaviorType::WaterDrop;
        drop.health = 1000;
        drop.posX = 150;
        drop.posZ = 4050;
        drop.posY = 100;
        drop.velocityY = -2;
        drop.field_26 = 4;
        drop.spriteScale = 0x02ab;
        drop.doorLockOrGroupBitmask = 2;
        auto expected = drop;
        expected.posY = 94;
        expected.spriteScale = 0x05ab;
        expected.displayType = DisplayType::SpriteTransparent;
        expected.blendWeightOrSpriteOpacity = 4;
        state.tickCount = 3;
        Check(DispatchObjectCase(drop, state).status == DispatchStatus::ExecutedCase &&
            drop.ReadBytes() == expected.ReadBytes(),
            "WaterDrop growth must integrate old velocity first and preserve unrelated bytes.");
        DispatchObjectCase(drop, state);
        Check(drop.posY == 88 && drop.velocityY == -5 && drop.spriteScale == 0x05ab,
            "WaterDrop did not accelerate only after exceeding its target size.");
        drop.posY = 1000;
        drop.velocityY = -15;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 955 && drop.velocityY == -16, "WaterDrop terminal velocity or integration order differs.");
        drop.posY = 100;
        drop.velocityY = -30;
        drop.field_24 = 1;
        state.tickCount = 3;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 10 && drop.velocityY == -30 && drop.field_24 == 65534 &&
            drop.spriteScale == 0x05ab, "WaterDrop waiting must wrap and suppress both growth and velocity clamp.");

        drop.posY = 100;
        drop.velocityY = 0;
        drop.field_24 = 0xffff;
        drop.spriteScale = 0xffab;
        drop.doorLockOrGroupBitmask = 255;
        state.tickCount = 258;
        DispatchObjectCase(drop, state);
        Check(drop.spriteScale == 0x01ab && drop.field_24 == 0xffff,
            "WaterDrop scale growth must wrap its high byte using the low tick byte and signed timer test.");
        drop.spriteScale = 0;
        drop.doorLockOrGroupBitmask = -1;
        drop.posY = -100;
        drop.velocityY = -32768;
        state.tickCount = 1;
        DispatchObjectCase(drop, state);
        Check(drop.velocityY == 32767 && drop.posY == 32668,
            "WaterDrop acceleration must wrap before applying its signed terminal-speed comparison.");

        drop.posY = 1;
        drop.velocityY = -1;
        drop.field_24 = 100;
        drop.field_26 = 4;
        drop.animFrame = 9;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 0 && drop.field_26 == 4 && drop.animFrame == 9,
            "WaterDrop equality with floor must not count as impact.");
        drop.spriteScale = 0x17ab;
        drop.itemValue = 20;
        drop.winX = 1;
        state.tickCount = 7;
        for (int update = 1; update <= 4; ++update)
        {
            DispatchObjectCase(drop, state);
            if (update < 4)
            {
                Check(drop.posY == -1 && drop.velocityY == 0 && drop.animFrame == 2 &&
                    drop.field_26 == 4 - update && drop.winX == 0,
                    "WaterDrop splash must last four updates, not four ticks.");
            }
        }
        Check(drop.posY == 96 && drop.velocityY == 0 && drop.animFrame == 1 && drop.field_26 == 4 &&
            drop.field_24 == 20 && drop.spriteScale == 0xab && drop.winX == 1 && drop.health == 1000,
            "WaterDrop ceiling respawn did not restore timer, frame, size, and splash latch.");
        drop.posY = -1;
        drop.field_26 = 0;
        level.ceilingObjects[0][0] = 0;
        state.tickCount = 0;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 20000 && drop.field_26 == 4 && drop.field_24 == 20,
            "WaterDrop zero-tick impact must still decrement its splash counter and use no-ceiling height.");

        level = CreateDummyLevel();
        drop.posY = 96;
        drop.velocityY = 0;
        drop.field_24 = 20;
        drop.field_26 = 4;
        drop.spriteScale = 0xab;
        drop.doorLockOrGroupBitmask = 2;
        drop.animFrame = 1;
        state.tickCount = 1;
        bool grew = false;
        bool fell = false;
        bool splashed = false;
        int cycleUpdates = 0;
        do
        {
            DispatchObjectCase(drop, state);
            ++cycleUpdates;
            grew = grew || drop.spriteScale == 0x03ab;
            fell = fell || (drop.posY < 96 && drop.posY >= 0 && drop.velocityY < 0);
            splashed = splashed || drop.animFrame == 2;
        } while (cycleUpdates < 200 && !(splashed && drop.animFrame == 1));
        Check(grew && fell && splashed && cycleUpdates == 41 && drop.posY == 96 &&
            drop.field_24 == 20 && drop.field_26 == 4 && drop.spriteScale == 0xab,
            "WaterDrop did not complete the exact wait/grow/fall/four-update-splash/ceiling-reset cycle.");

        for (std::size_t i = 0; i < 4; ++i)
        {
            level.floorHeights[i / 2][i % 2] = 100;
            level.roomHeights[i / 2][i % 2] = 50;
        }
        level.surfaceFlags[0][0] = 28;
        drop.posY = -89;
        drop.velocityY = -20;
        drop.field_24 = 100;
        drop.field_26 = 4;
        state.tickCount = 1;
        DispatchObjectCase(drop, state);
        Check(drop.posY == -97 && drop.animFrame == 2 && drop.field_26 == 3,
            "WaterDrop floor lookup must use the position before motion, including stacked-floor selection.");

        const auto before = drop.ReadBytes();
        state.level = nullptr;
        bool rejected = false;
        try { DispatchObjectCase(drop, state); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && drop.ReadBytes() == before, "Missing level geometry must fail before mutating WaterDrop.");
        state.level = &level;
        level.surfaceFlags[0][0] = 1;
        rejected = false;
        try { DispatchObjectCase(drop, state); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && drop.ReadBytes() == before, "Missing surface metadata must fail before mutating WaterDrop.");
        state.level = nullptr;
        state.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(drop, state).status == DispatchStatus::DispatchDisabled,
            "Disabled WaterDrop must not query missing geometry.");
        state.behaviorDispatchEnabled = true;
        drop.health = 0;
        Check(DispatchObjectCase(drop, state).status == DispatchStatus::InactiveObject,
            "Inactive WaterDrop must not query missing geometry.");
    }

    void TestSoundEffects(const char* tablePath)
    {
        struct TestAudioHandler final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            std::uint16_t sampleCount = 129;
            std::size_t lastChannel = SoundChannelCount;
            std::uint8_t lastSample = 0;
            std::uint8_t lastVolume = 0;
            unsigned int starts = 0;
            bool failStart = false;
            std::uint16_t GetSampleCount() const override { return sampleCount; }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(!playing.at(channel), "Audio admission attempted to interrupt an occupied channel.");
                if (failStart) { throw std::runtime_error("Test audio backend failure."); }
                lastChannel = channel;
                lastSample = sample;
                lastVolume = volume;
                ++starts;
                playing[channel] = true;
            }
        } audio;
        GameState state;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 2, 1) == SoundPlaybackStatus::AudioUnavailable,
            "Unconfigured audio must report unavailable, not successful playback.");
        state.audioHandler = &audio;
        bool rejected = false;
        try { state.PlaySoundEffect(SoundEffect::Drop, 255, 2, 1); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "An attached audio backend requires its sound-effect table.");

        std::ifstream file(tablePath, std::ios::binary);
        Check(static_cast<bool>(file), "Cannot open original S_TAB.PRB.");
        const std::vector<std::uint8_t> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        state.LoadSoundEffects(bytes.data(), bytes.size());
        Check(state.soundEffectsLoaded && state.soundEffects[4].sampleCount == 2 &&
            state.soundEffects[4].samples[0] == 5 && state.soundEffects[4].samples[1] == 38,
            "Author-confirmed Drop effect must use the original two sample references.");
        const auto loaded = state.soundEffects;
        for (int invalid = 0; invalid < 6; ++invalid)
        {
            auto damaged = bytes;
            if (invalid == 2) { damaged[0] = 128; }
            if (invalid == 3) { damaged[2] = 4; }
            if (invalid == 4) { damaged[4 + 128 * 5] = 5; }
            if (invalid == 5) { damaged.push_back(0); }
            rejected = false;
            try
            {
                state.LoadSoundEffects(invalid == 0 ? nullptr : damaged.data(),
                    invalid == 1 ? damaged.size() - 1 : damaged.size());
            }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected && std::memcmp(loaded.data(), state.soundEffects.data(), sizeof(loaded)) == 0,
                "Invalid sound table was accepted or partially overwrote the loaded definitions.");
        }
        for (const auto id : { 0, 130, 65535 })
        {
            rejected = false;
            try { state.PlaySoundEffect(static_cast<SoundEffect>(id), 255, 2, 1); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected, "Invalid effect enum must be an explicit input error.");
        }

        auto& definition = state.soundEffects[4];
        for (std::uint8_t count = 1; count <= 4; ++count)
        {
            definition = { count, { 1, 2, 3, 4 } };
            for (std::uint32_t seed = 0; seed <= 65535; ++seed)
            {
                audio.playing.fill(false);
                state.randomSeed = seed;
                const auto next = static_cast<std::uint32_t>(
                    (static_cast<std::uint64_t>(seed) * 134775813u + 1u) & 0xffffffffu);
                Check(state.PlaySoundEffect(SoundEffect::Drop, 201, 2, 1) == SoundPlaybackStatus::Started &&
                    state.randomSeed == next && audio.lastSample == 1 + (next >> 16) % count &&
                    audio.lastChannel == 3 && audio.lastVolume == 201,
                    "Sound random selection, native highest-free channel, or sample/volume forwarding differs.");
            }
        }
        for (const auto seed : { 0x80000000u, 0xffffffffu, 0x12345678u })
        {
            audio.playing.fill(false);
            state.randomSeed = seed;
            const auto next = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(seed) * 134775813u + 1u) & 0xffffffffu);
            state.PlaySoundEffect(SoundEffect::Drop, 255, 2, 1);
            Check(state.randomSeed == next && audio.lastSample == 1 + (next >> 16) % 4,
                "Sound RNG must retain high seed bits and 32-bit overflow.");
        }
        state.LoadSoundEffects(bytes.data(), bytes.size());
        state.randomSeed = 0;
        for (std::int16_t source = 1; source <= 4; ++source)
        {
            audio.playing.fill(false);
            Check(state.PlaySoundEffect(SoundEffect::Drop, 255, source, 1) == SoundPlaybackStatus::Started &&
                (audio.lastSample == 5 || audio.lastSample == 38), "Original Drop variants were not forwarded.");
        }

        for (unsigned int occupancy = 0; occupancy < 16; ++occupancy)
        {
            for (const auto limit : { -3, -1, 0, 1, 2, 4, -32768 })
            {
                int freeCount = 0;
                int sameCount = 0;
                std::size_t lastFree = SoundChannelCount;
                for (std::size_t channel = 0; channel < SoundChannelCount; ++channel)
                {
                    audio.playing[channel] = (occupancy & (1u << channel)) != 0;
                    state.soundChannelSources[channel] = channel % 2 == 0 ? 7 : 8;
                    if (!audio.playing[channel]) { ++freeCount; lastFree = channel; }
                    else if (state.soundChannelSources[channel] == 7) { ++sameCount; }
                }
                const auto expected = limit < 0 && freeCount < 2 ? SoundPlaybackStatus::ReservedChannel :
                    freeCount == 0 ? SoundPlaybackStatus::NoFreeChannel :
                    ReferenceSignedWord(limit < 0 ? -limit : limit) <= sameCount ?
                    SoundPlaybackStatus::SourceLimitReached : SoundPlaybackStatus::Started;
                const auto starts = audio.starts;
                Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, static_cast<std::int16_t>(limit)) == expected,
                    "Four-channel occupancy, signed source limit, or reserved-channel rule differs.");
                Check(audio.starts == starts + (expected == SoundPlaybackStatus::Started ? 1u : 0u),
                    "Rejected playback touched the audio backend.");
                if (expected == SoundPlaybackStatus::Started)
                {
                    Check(audio.lastChannel == lastFree && state.soundChannelSources[lastFree] == 7,
                        "Playback did not select and tag the last free channel.");
                }
            }
        }
        audio.playing.fill(false);
        state.soundEffects[4] = { 1, { 5, 0, 0, 0 } };
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::Started,
            "Initial per-source sound was rejected.");
        state.soundEffects[4] = { 1, { 38, 0, 0, 0 } };
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::SourceLimitReached,
            "Source concurrency must count other samples, not just duplicates of the selected sample.");
        audio.playing[3] = false;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::Started,
            "Finished audio channel was not reusable.");

        audio.playing.fill(false);
        const auto starts = audio.starts;
        state.randomSeed = 0;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 0, 7, 1) == SoundPlaybackStatus::Inaudible &&
            state.randomSeed == 1, "Inaudible requests must still consume native sample selection.");
        state.soundEnabled = false;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::Disabled,
            "Disabled sound must be reported explicitly.");
        state.soundEnabled = true;
        audio.sampleCount = 4;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::NoSample,
            "Missing backend sample must not be passed to PlaySample.");
        audio.sampleCount = 129;
        state.soundEffects[4].sampleCount = 0;
        const auto seedBefore = state.randomSeed;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::NoSample &&
            state.randomSeed == seedBefore && audio.starts == starts,
            "Empty sample lists must not draw random values or start audio.");
        state.soundEffects[4] = { 1, { 0, 0, 0, 0 } };
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::NoSample,
            "Zero sample reference must follow the original no-sample gate.");
        state.LoadSoundEffects(bytes.data(), bytes.size());
        audio.failStart = true;
        const auto sources = state.soundChannelSources;
        rejected = false;
        try { state.PlaySoundEffect(SoundEffect::Drop, 255, 9, 1); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && sources == state.soundChannelSources, "Backend errors must propagate without publishing a source.");
        audio.failStart = false;
        RawGameObject foreign;
        rejected = false;
        try { state.PlaySoundEffect(SoundEffect::Drop, 255, foreign, 1); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "An object outside worldObjects cannot supply an original source slot.");

        const auto referenceVolume = [](int x, int z, int scale, int listenerX, int listenerZ)
        {
            const auto absolute = [](int value)
            {
                const auto word = ReferenceSignedWord(value);
                return word < 0 ? ReferenceSignedWord(-word) : word;
            };
            const auto distance = ReferenceSignedWord(absolute(listenerX - x) + absolute(listenerZ - z));
            const auto scaled = ReferenceSignedWord(distance * scale);
            const auto magnitude = static_cast<std::uint16_t>(absolute(scaled));
            return std::clamp(355 - static_cast<int>(magnitude / 2), 0, 255);
        };
        state.soundListenerX = -1234;
        state.soundListenerZ = 2700;
        for (int coordinate = -32768; coordinate <= 32767; ++coordinate)
        {
            for (const auto scale : { 1, 3 })
            {
                const auto value = static_cast<std::int16_t>(coordinate);
                Check(state.GetSoundVolumeAt(value, -300, static_cast<std::int16_t>(scale)) ==
                    referenceVolume(value, -300, scale, -1234, 2700) &&
                    state.GetSoundVolumeAt(300, value, static_cast<std::int16_t>(scale)) ==
                    referenceVolume(300, value, scale, -1234, 2700),
                    "Positional volume differs in native absolute value, Manhattan distance, scaling, or word wrapping.");
            }
        }
        state.soundListenerX = 0;
        state.soundListenerZ = 0;
        Check(state.GetSoundVolumeAt(200, 0) == 255 && state.GetSoundVolumeAt(201, 0) == 255 &&
            state.GetSoundVolumeAt(202, 0) == 254 && state.GetSoundVolumeAt(708, 0) == 1 &&
            state.GetSoundVolumeAt(710, 0) == 0, "Native sound-volume thresholds changed.");
        state.positionalSoundsSuppressed = true;
        Check(state.GetSoundVolumeAt(0, 0) == 0 && state.GetSoundVolumeAt(0, 0, 3) == 0,
            "DS:D4FA positional suppression must force distance 10000.");
        state.positionalSoundsSuppressed = false;

        auto level = CreateDummyLevel();
        state.level = &level;
        auto& drop = state.worldObjects[2];
        drop.behavior = BehaviorType::WaterDrop;
        drop.health = 1000;
        drop.posX = 150;
        drop.posZ = 4050;
        drop.posY = -1;
        drop.field_26 = 4;
        drop.winX = 1;
        state.soundListenerX = drop.posX;
        state.soundListenerZ = drop.posZ;
        audio.playing.fill(false);
        const auto beforeImpact = audio.starts;
        DispatchObjectCase(drop, state);
        Check(audio.starts == beforeImpact + 1 && state.soundChannelSources[3] == 2 &&
            audio.lastVolume == 255 && drop.winX == 0,
            "WaterDrop impact must play the named Drop effect using its original object slot.");
        for (int i = 0; i < 3; ++i) { DispatchObjectCase(drop, state); }
        Check(audio.starts == beforeImpact + 1 && drop.winX == 1 && drop.posY == 96,
            "Splash updates must not repeat audio; ceiling reset must rearm it.");
        audio.playing.fill(false);
        drop.posY = -1;
        DispatchObjectCase(drop, state);
        Check(audio.starts == beforeImpact + 2, "Next WaterDrop cycle did not play its splash sound.");
    }

    void TestAgainstOriginal(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        Check(static_cast<bool>(file), "Cannot open the original executable for differential routing tests.");
        const std::vector<std::uint8_t> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        Check(bytes.size() == 203760 && bytes[0] == 'M' && bytes[1] == 'Z',
            "Unexpected original executable.");
        const auto word = [&](std::size_t at) -> int
        {
            Check(at < bytes.size() - 1, "Word is outside the executable.");
            return bytes[at] | (static_cast<int>(bytes[at + 1]) << 8);
        };
        const auto header = static_cast<std::size_t>(word(8) * 16);
        const auto originalEntry = [&](int behavior)
        {
            int ip = 0x1151;
            int compared = -1;
            for (int step = 0; step < 1000; ++step)
            {
                if (ip == 0x5110) { return ip; }
                const auto at = header + static_cast<std::size_t>(ip);
                Check(at < bytes.size() - 3, "Selector escaped executable bounds.");
                const int op = bytes[at];
                if (op == 0x3d)
                {
                    compared = word(at + 1);
                    ip += 3;
                }
                else if (op == 0x83 && bytes[at + 1] == 0xf8)
                {
                    const int immediate = bytes[at + 2];
                    compared = (immediate < 128 ? immediate : immediate - 256) & 0xffff;
                    ip += 3;
                }
                else if (op == 0xe9)
                {
                    const int delta = word(at + 1);
                    ip = (ip + 3 + (delta < 32768 ? delta : delta - 65536)) & 0xffff;
                }
                else if (op == 0xeb || op == 0x74 || op == 0x75)
                {
                    const int delta = bytes[at + 1];
                    Check(op == 0xeb || compared >= 0, "Selector branch lacks a comparison.");
                    const bool take = op == 0xeb || (op == 0x74 ? behavior == compared : behavior != compared);
                    ip = (ip + 2 + (take ? (delta < 128 ? delta : delta - 256) : 0)) & 0xffff;
                }
                else
                {
                    return ip;
                }
            }
            throw std::runtime_error("Selector interpreter exceeded its instruction bound.");
        };
        RawGameObject object;
        object.WriteWord(10, 1);
        GameState context;
        auto level = CreateDummyLevel();
        context.collisionMap = level.collisionMap.get();
        context.level = &level;
        object.posX = 150;
        object.posZ = 4050;
        for (int behavior = 0; behavior <= 65535; ++behavior)
        {
            object.behavior = static_cast<BehaviorType>(behavior);
            const auto result = DispatchObjectCase(object, context);
            Check(result.moduleOffset == originalEntry(behavior) && result.behavior == object.behavior,
                "C++ routing differs from the original executable's selector instructions.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        TestObjectLayout();
        TestCases();
        TestGatesAndWords();
        TestCollisionMap();
        TestCollisionMapReader();
        TestIdleAnimation();
        TestGameMath();
        TestSmokeFading();
        TestSwitches();
        TestObjectInteractions();
        TestPlayerInteractionLoop();
        TestHeightfields();
        TestWaterDrop();
        Check(argc == 3, "Expected original executable and sound-table paths.");
        TestSoundEffects(argv[2]);
        TestAgainstOriginal(argv[1]);
        std::cout << "PASS: 190 catalog routes and all 65536 selector words match the original; "
            "signed gates, object state, collision-map reads/writes, Pillar, IdleAnimation, SmokeFading, switches, "
            "inventory/pickup interactions, collision/input routing, geometry streams, height lookups, WaterDrop, sound "
            "variants/attenuation/channel admission, "
            "and exhaustive arithmetic checks verified.\n";
        return 0;
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
