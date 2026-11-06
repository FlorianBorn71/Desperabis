#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    bool Same(const RawGameObject& a, const RawGameObject& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }

    std::int16_t Word(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    struct Context : GameContext
    {
        int destroyed = 0;
        void OnObjectDestroyed(int) override { ++destroyed; }
    };

    struct Host final : Context
    {
        std::filesystem::path assets;
        explicit Host(const std::filesystem::path& path) : assets(path) {}
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view name) override
        {
            std::ifstream file(assets / name, std::ios::binary);
            if (!file) { throw std::runtime_error("Missing swamp fixture: " + std::string(name)); }
            return {std::istreambuf_iterator<char>(file), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            LoadLevelObjects(request.levelNumber, data);
            InitializeLevelPlayerSpawn(data, {0,0,100});
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestSwitch()
    {
        Context context;
        auto& object = context.worldObjects[2];
        for (const int current : {-32768,-512,-30,0,128,511,32767})
        {
            for (const int target : {-32768,-512,-30,0,128,511,32767})
            {
                for (const int rate : {-32768,-40,-6,0,6,32767})
                {
                    for (const int ticks : {0,1,3,200})
                    {
                        object = {};
                        object.health = 1000;
                        object.behavior = BehaviorType::Switch3;
                        object.collisionType = CollisionType::Switch;
                        object.collisionMask = 7;
                        object.winY = static_cast<std::int16_t>(current);
                        object.velocityY = static_cast<std::int16_t>(target);
                        object.generalPurpose2 = static_cast<std::uint16_t>(rate);
                        auto expected = object;
                        const int delta = Word(target - current);
                        const int step = Word(std::abs(Word(static_cast<std::int64_t>(rate) * ticks)));
                        const int amount = Word(std::abs(delta)) <= step ? delta : ((delta > 0) - (delta < 0)) * step;
                        expected.winY = Word(current + amount);
                        if (expected.winY == target)
                        {
                            expected.behavior = BehaviorType::None;
                            expected.collisionType = CollisionType::None;
                        }
                        context.tickCount = static_cast<std::int16_t>(ticks);
                        const auto result = DispatchObjectCase(object, context);
                        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x20d7 &&
                            Same(object, expected), "Rotating switches must preserve signed-word step/wrap and retire exactly at the target, without angle normalization.");
                    }
                }
            }
        }
    }

    RawGameObject Key()
    {
        RawGameObject object;
        object.health = 1000;
        object.behavior = BehaviorType::UnreachableKey;
        object.collisionType = CollisionType::Key;
        object.pickupType = PickupType::Type_4;
        object.itemValue = 3;
        object.posX = 1750; object.posY = -198; object.posZ = 924;
        object.collisionRadiusXZ = 12; object.collisionRadiusY = 5;
        return object;
    }

    void TestKey()
    {
        Context context;
        auto& key = context.worldObjects[2];
        auto& contact = context.worldObjects[3];
        contact = Key();
        contact.behavior = BehaviorType::WeaponDamageCollider;
        contact.pickupType = PickupType::None;
        contact.collisionMask = 254;
        context.tickCount = 0;
        for (int damage = 0; damage <= 255; ++damage)
        {
            key = Key();
            key.collisionMask = 255;
            contact.behaviorParameter = static_cast<std::uint8_t>(damage);
            context.inventory = {};
            const auto destroyed = context.destroyed;
            DispatchObjectCase(key, context);
            Check(key.collisionMask == 0 && (key.health == 0) == (damage == 5) &&
                context.destroyed == destroyed + (damage == 5 ? 1 : 0),
                "Only a damage-five collider retrieves the hut key, even at zero behavior ticks; pickup must use lifecycle hooks.");
            if (damage == 5)
            {
                const auto expected = Key();
                Check(Same(context.inventory[0], expected) && contact.health == 1000,
                    "Automatic retrieval must copy the complete key record with mask zero and must not consume the weapon contact.");
            }
        }
        key = Key();
        contact.behaviorParameter = 5;
        for (auto& item : context.inventory) { item.health = -1; }
        DispatchObjectCase(key, context);
        Check(key.health == 1000, "Full inventory must leave the unreachable key in the world for a later attempt.");
        context.inventory[5] = {};
        DispatchObjectCase(key, context);
        Check(key.health == 0 && Same(context.inventory[5], Key()),
            "Retrieval must succeed into the first truly empty slot when space becomes available.");

        context.inventory = {};
        key = Key();
        context.objectCollisionsEnabled = false;
        DispatchObjectCase(key, context);
        Check(key.health == 1000 && key.collisionMask == 0, "Collision-disabled key queries must still clear their temporary mask.");
        context.objectCollisionsEnabled = true;
        contact.behavior = BehaviorType::BouncingObject;
        DispatchObjectCase(key, context);
        Check(key.health == 1000, "Damage byte five alone must not qualify a non-weapon object.");
        contact.behavior = BehaviorType::WeaponDamageCollider;
        auto& later = context.worldObjects[4];
        later = contact;
        contact.behaviorParameter = 4;
        DispatchObjectCase(key, context);
        Check(key.health == 1000, "Native first-contact ordering must not search beyond a nonqualifying first hit.");
        contact.health = 0;
        DispatchObjectCase(key, context);
        Check(key.health == 0, "Inactive earlier contacts must not mask a later valid weapon contact.");
        key = Key();
        later.posX = Word(key.posX + key.collisionRadiusXZ + later.collisionRadiusXZ);
        DispatchObjectCase(key, context);
        Check(key.health == 1000, "Merely touching the native collision boundary must not retrieve the key.");
    }

    void TestAuthoredSwitches(Host& host)
    {
        const auto original = host.worldObjects;
        for (const auto slot : {60,65})
        {
            host.worldObjects = original;
            auto& lever = host.worldObjects[slot];
            const auto before = lever;
            host.tickCount = 10;
            DispatchObjectCase(lever, host);
            Check(Same(lever, before), "Original rotating switches must wait for interaction to set their rate.");
            const int doorSlot = slot == 60 ? 59 : 24;
            Check(HandlePlayerObjectInteraction(lever, host).status == InteractionStatus::Applied &&
                lever.generalPurpose2 == 6 && host.worldObjects[doorSlot].generalPurpose2 == 3,
                "Both original switches must activate their authored door groups through ordinary interaction.");
            host.tickCount = 1;
            for (int tick = 0; tick < 26; ++tick) { DispatchObjectCase(lever, host); }
            Check(lever.winY == -28 && lever.behavior == BehaviorType::Switch3,
                "Original switch must remain active until it reaches its exact -30 target.");
            DispatchObjectCase(lever, host);
            Check(lever.winY == -30 && lever.behavior == BehaviorType::None &&
                lever.collisionType == CollisionType::None && lever.collisionMask == before.collisionMask,
                "Original switch must stop after 27 six-unit steps without changing its collision mask.");
        }
        host.worldObjects = original;
    }

    void TestAuthoredFireflies(Host& host)
    {
        constexpr std::array<int, 6> slots{82,83,84,87,88,89};
        for (const auto slot : slots)
        {
            auto& object = host.worldObjects[slot];
            Check(object.behavior == BehaviorType::FireFly && object.health > 1000,
                "All six original fireflies must retain their activation-group health.");
            const auto position = std::array{object.posX, object.posY, object.posZ};
            host.tickCount = 5;
            Check(UpdateObject(object, host).status == DispatchStatus::ExecutedCase &&
                position == std::array{object.posX, object.posY, object.posZ},
                "Dormant original fireflies must not move before their native activation.");
        }
        host.ActivateObjectGroup(10);
        host.ActivateObjectGroup(11);
        host.tickCount = 1;
        std::array<RawGameObject, 6> active{};
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            auto& object = host.worldObjects[slots[i]];
            Check(object.health == 1000, "Both native firefly groups must activate through the shared group path.");
            active[i] = object;
        }
        for (int frame = 0; frame < 120; ++frame)
        {
            for (const auto slot : slots)
            {
                auto& object = host.worldObjects[slot];
                Check(UpdateObject(object, host).status == DispatchStatus::ExecutedCase &&
                    object.behavior == BehaviorType::FireFly && object.health > 0 &&
                    object.itemValue >= 0 && object.itemValue <= 4,
                    "Each original firefly must simulate on the real swamp collision map with bounded native speed.");
            }
        }
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const auto& object = host.worldObjects[slots[i]];
            Check(object.posX != active[i].posX || object.posY != active[i].posY || object.posZ != active[i].posZ,
                "Activated original fireflies must actually move.");
        }
        const auto save = host.CaptureSaveGame("Swamp flight");
        Host loaded(host.assets);
        loaded.LoadGame(save);
        loaded.tickCount = 1;
        for (const auto slot : slots)
        {
            Check(Same(host.worldObjects[slot], loaded.worldObjects[slot]),
                "Saving/reloading must preserve every firefly object byte.");
        }
        // The native save format does not store the RNG seed.
        loaded.randomSeed = host.randomSeed;
        for (int frame = 0; frame < 30; ++frame)
        {
            for (const auto slot : slots)
            {
                UpdateObject(host.worldObjects[slot], host);
                UpdateObject(loaded.worldObjects[slot], loaded);
                Check(Same(host.worldObjects[slot], loaded.worldObjects[slot]) && host.randomSeed == loaded.randomSeed,
                    "Restored firefly records must continue identically under the same RNG stream.");
            }
        }
    }

    void TestAuthoredKey(Host& host)
    {
        auto& key = host.worldObjects[17];
        Check(key.behavior == BehaviorType::UnreachableKey && key.itemValue == 3, "Expected original hut-key fixture.");
        for (int slot = 2; slot <= 164; ++slot)
        {
            if (slot != 17) { host.SetObjectHealth(host.worldObjects[slot], 0); }
        }
        auto& player = host.worldObjects[1];
        player.posX = Word(key.posX - 55);
        player.posY = Word(key.posY + 30 - player.collisionRadiusY - 2);
        player.posZ = key.posZ;
        player.velocityX = 100; player.velocityZ = 0;
        host.inventory = {};
        auto& shovel = host.inventory[1];
        shovel.health = 1000; shovel.collisionType = CollisionType::Weapon;
        shovel.itemValue = static_cast<std::int16_t>(WeaponType::Showel); shovel.generalPurpose1 = 100;
        host.selectedWeapon = host.requestedWeapon = WeaponType::Showel;
        host.weaponHand = host.requestedWeaponHand = 2;
        host.weaponReady = true;
        host.RefreshWeaponInventory();
        Check(host.AttackWithSelectedWeapon() == WeaponActionStatus::Applied, "The ordinary shovel attack must start.");
        host.UpdateWeaponAnimation(6);
        const auto& contact = host.worldObjects[2];
        Check(contact.behavior == BehaviorType::WeaponDamageCollider && contact.behaviorParameter == 5 &&
            contact.posX == key.posX && contact.posY == key.posY && contact.posZ == key.posZ,
            "The actual shovel animation must create its damage-five contact at the original key.");
        const auto destroyed = host.destroyed;
        UpdateObject(key, host);
        Check(key.health == 0 && host.inventory[0].collisionType == CollisionType::Key &&
            host.inventory[0].itemValue == 3 && host.destroyed == destroyed + 1,
            "A real weapon animation must retrieve the original hut key automatically through inventory/lifecycle handling.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: swamp-tests <original-assets>");
        TestSwitch();
        TestKey();
        Host host(argv[1]);
        host.StartLevel(24);
        TestAuthoredSwitches(host);
        TestAuthoredFireflies(host);
        TestAuthoredKey(host);
        std::cout << "PASS: swamp rotating switches, hut-key contact/pickup and all nine authored objects with saved flight continuation.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
