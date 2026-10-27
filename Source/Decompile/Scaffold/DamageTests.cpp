#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    bool Equal(const RawGameObject& a, const RawGameObject& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }

    std::int16_t Word(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1) & 0xffffffffu);
        return range == 0 ? 0 : (seed >> 16) % range;
    }

    struct Fixture
    {
        Level level = CreateDummyLevel();
        GameContext context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            level.heightSurfaces[0] = Level::HeightSurface{ 0, 0 };
            context.soundListenerX = 1400;
            context.soundListenerZ = 2000;
        }
        RawGameObject& Target() { return context.worldObjects[20]; }
        RawGameObject& Contact() { return context.worldObjects[10]; }
        void Set(CollisionType type, std::int16_t strength = 10)
        {
            Target() = {};
            Target().health = 1000;
            Target().collisionType = type;
            Target().behavior = BehaviorType::DisabledSentinel;
            Target().groupIndexOrLock = strength;
            Target().posX = 1500;
            Target().posY = 25;
            Target().posZ = 2000;
            Target().field_07 = 3;
            Target().animFrame = 2;
            Target().modelFrame2 = 3;
            Target().modelAnim = 1;
            Target().modelId = 38;
            Target().collisionRadiusXZ = 20;
            Target().collisionRadiusY = 10;
            Target().field_32 = 99;
            Target().spriteScale = 17;
            Target().renderSortKey = -1234;
            Target().renderPlacementMode = 7;
            Target().winX = 31;
            Target().winY = 37;
            Target().winZ = 41;
            Target().field_24 = 29;
            Target().field_26 = 31;
            Target().field_28 = 0;
            Target().itemValue = 2;
            Target().behaviorParameter = 3;
            Target().blendWeightOrSpriteOpacity = 7;
            Target().displayType = DisplayType::VectorNormal;
        }
        void Arm(std::uint8_t damage)
        {
            Contact() = {};
            Contact().health = 10;
            Contact().behavior = BehaviorType::WeaponDamageCollider;
            Contact().field_07 = 254;
            Contact().behaviorParameter = damage;
            Contact().posX = 1499;
            Contact().posY = 25;
            Contact().posZ = 2001;
            Contact().collisionRadiusXZ = 22;
            Contact().collisionRadiusY = 12;
        }
    };

    void TestCollider()
    {
        for (const auto type : { CollisionType::Skeleton, CollisionType::PushingEnemy,
            CollisionType::SpiderLarge, CollisionType::Devil, CollisionType::DestroyableBarrier,
            CollisionType::CeilingConcreteSlabStake, CollisionType::Dragon, CollisionType::Gold })
        {
            for (const auto damage : std::array<std::uint8_t, 6>{ 0,1,2,5,20,255 })
            {
                Fixture f;
                f.Set(type, 1000);
                f.Arm(damage);
                f.context.tickCount = 0;
                f.context.randomSeed = 1234;
                auto seed = f.context.randomSeed;
                auto expected = f.Contact();
                expected.health = 0;
                const int frame = type == CollisionType::Skeleton || type == CollisionType::PushingEnemy ? 2 :
                    type == CollisionType::SpiderLarge || type == CollisionType::Devil ? 4 :
                    type == CollisionType::DestroyableBarrier || type == CollisionType::CeilingConcreteSlabStake ? 3 : 0;
                if (frame)
                {
                    expected.health = 50;
                    expected.modelId = 39; expected.modelAnim = 3; expected.animFrame = static_cast<std::uint8_t>(frame);
                    expected.modelFrame2 = 1; expected.blendWeightOrSpriteOpacity = 100;
                    expected.displayType = DisplayType::Type30; expected.behavior = BehaviorType::DriftingParticle;
                    expected.velocityX = -1; expected.velocityY = 1; expected.velocityZ = 1;
                    expected.posX = 1500; expected.posZ = 2000;
                    expected.winX = static_cast<std::int16_t>(Next(seed, 512));
                    expected.winY = static_cast<std::int16_t>(Next(seed, 512));
                }
                const auto beforeWorld = f.context.worldObjects;
                if (type == CollisionType::Dragon && damage / 2 > 0) { Next(seed, 3); }
                const auto result = Function_WeaponDamageCollider(f.Contact(), f.context);
                Check(Equal(f.Contact(), expected) && result.status == DispatchStatus::ExecutedCase &&
                    result.behavior == BehaviorType::WeaponDamageCollider && result.moduleOffset == 0x1198,
                    "Collider must consume once, convert in place at the target, and preserve the damage byte/Y.");
                Check(f.Target().groupIndexOrLock == (type == CollisionType::Gold ? 1000 :
                    1000 - (type == CollisionType::Dragon ? damage / 2 : damage)),
                    "Collider must apply native full damage, dragon half damage, or the target's no-effect branch.");
                Check(f.context.randomSeed == seed, "Hit-particle orientation must precede target damage without extra allocation/RNG.");
                for (std::size_t slot = 0; slot < beforeWorld.size(); ++slot)
                {
                    if (slot != 10 && slot != 20)
                    {
                        Check(Equal(beforeWorld[slot], f.context.worldObjects[slot]), "Ordinary weapon impact must not allocate another object.");
                    }
                }
            }
        }
        Fixture f;
        f.Arm(20);
        auto before = f.Contact();
        f.context.worldObjects[1] = before;
        Function_WeaponDamageCollider(f.Contact(), f.context);
        Check(Equal(before, f.Contact()), "Weapon contact must not hit the player or consume itself without a target.");
        f.Set(CollisionType::Skeleton, 60);
        f.context.objectCollisionsEnabled = false;
        Function_WeaponDamageCollider(f.Contact(), f.context);
        Check(Equal(before, f.Contact()) && f.Target().groupIndexOrLock == 60, "Disabled object collision must suppress weapon contacts.");
        f.context.objectCollisionsEnabled = true;
        f.Contact().health = 1;
        const auto result = UpdateObject(f.Contact(), f.context);
        Check(result.status == DispatchStatus::ExecutedCase && f.Contact().health == 50 &&
            f.Target().groupIndexOrLock == 40, "Common countdown reaching zero must still execute the native impact case.");

        f.Set(CollisionType::Skeleton, 20);
        f.Arm(20);
        Function_WeaponDamageCollider(f.Contact(), f.context);
        int fragments = 0;
        for (const auto& object : f.context.worldObjects)
        {
            if (object.behavior == BehaviorType::Behavior_1 && object.health > 0) { ++fragments; }
        }
        Check(f.Target().health == 0 && fragments == 8 && f.Contact().behavior == BehaviorType::DriftingParticle,
            "Fatal skeleton contact must retain its impact particle and create all eight death parts.");
        for (const auto& object : f.context.worldObjects)
        {
            if (object.collisionType == CollisionType::Weapon)
            {
                Check(static_cast<int>(object.displayType) == 4, "The skeleton's dropped sword must retain native display byte 4.");
            }
        }
    }

    void TestDefaultAndActivation()
    {
        const auto hasBranch = [](int type)
        {
            constexpr int types[] = { 4,5,8,10,13,14,20,21,22,26,30,31,33,34,35,36,37,38,
                41,49,59,60,61,63,65,66,68,88,89,91,92,93,105,126,138 };
            return std::find(std::begin(types), std::end(types), type) != std::end(types);
        };
        for (int type = 0; type < 256; ++type)
        {
            if (hasBranch(type)) { continue; }
            for (const auto health : { 999,1000,1001,1999,2000 })
            {
                for (const auto amount : { -32768,-1,0,1,20,200,32767 })
                {
                    Fixture f;
                    f.Set(static_cast<CollisionType>(type));
                    f.Target().health = static_cast<std::int16_t>(health);
                    auto expected = f.Target();
                    if (amount == 0 && health > 1000 && health < 2000)
                    {
                        if (type == 125) { expected.displayType = static_cast<DisplayType>(120); }
                        else if (type == 0 || type == 103 || type == 67 || type == 48 || type == 6 ||
                            type == 2 || type == 76 || type == 47 || type == 9) { expected.health = 1000; }
                    }
                    f.context.DamageObject(f.Target(), static_cast<std::int16_t>(amount));
                    Check(Equal(f.Target(), expected) && f.context.randomSeed == 0,
                        "Unlisted damage targets must restore strength; only exact activation health/type gates may change them.");
                }
            }
        }
        Fixture f;
        for (const auto type : { CollisionType::AttachableDamageObject, CollisionType::Weapon })
        {
            f.Set(type, 20);
            f.Target().health = 1010;
            f.context.DamageObject(f.Target(), 0);
            Check(f.Target().health == 1000 && f.Target().groupIndexOrLock == 20,
                "Activation must wake linked parts and weapons without spending strength.");
        }
        f.Set(CollisionType::BurnableOilPuddle);
        f.Target().health = 1010;
        f.Target().displayType = DisplayType::VectorUnlit;
        auto expected = f.Target();
        f.context.DamageObject(f.Target(), 0);
        Check(Equal(expected, f.Target()), "Only native display byte 4 may ignite an activated oil puddle.");
    }

    void TestArithmeticAndSpecialTargets()
    {
        Fixture f;
        for (unsigned int value = 0; value < 65536; ++value)
        {
            const auto amount = Word(value);
            f.Set(CollisionType::CeilingConcreteSlabStake, 1);
            auto expected = f.Target();
            expected.groupIndexOrLock = Word(1 - amount);
            f.context.DamageObject(f.Target(), amount);
            Check(Equal(expected, f.Target()), "Stake subtraction must retain signed-word overflow.");
            f.Set(CollisionType::RopeSwitch, -1);
            expected = f.Target();
            expected.posY = Word(expected.posY + amount);
            f.context.DamageObject(f.Target(), amount);
            Check(Equal(expected, f.Target()), "Rope damage must add to Y without decrementing nonpositive strength.");
            f.Set(CollisionType::DestroyableBarrier, 1);
            f.Target().field_24 = static_cast<std::uint16_t>(value * 13);
            f.Target().doorLockOrGroupBitmask = 42;
            expected = f.Target();
            expected.groupIndexOrLock = Word(1 - amount);
            const auto accumulator = (value * 13 + 3 * static_cast<unsigned int>(value)) & 65535;
            expected.field_24 = static_cast<std::uint16_t>(accumulator);
            if ((accumulator >> 8) >= 42) { expected.field_07 = 0; expected.field_24 = 32000; }
            f.context.DamageObject(f.Target(), amount);
            Check(Equal(expected, f.Target()), "Barrier accumulation must wrap low-word multiplication and compare the unsigned high byte.");
        }
        for (const auto type : { CollisionType::Weapon, CollisionType::Ammo })
        {
            for (const auto amount : { -1,0,20 })
            {
                f.Set(type);
                auto expected = f.Target();
                if (amount == -1) { expected.groupIndexOrLock = 11; expected.field_07 = 0; expected.health = 10; }
                f.context.DamageObject(f.Target(), static_cast<std::int16_t>(amount));
                Check(Equal(expected, f.Target()), "Only amount -1 destroys weapon/ammo; other amounts restore strength.");
            }
        }
        for (const auto amount : { 100,101 })
        {
            f.Set(CollisionType::MirrorToGrab, 200);
            f.context.DamageObject(f.Target(), static_cast<std::int16_t>(amount));
            Check(f.Target().winX == (amount > 100 ? 128 : 31) && f.Target().groupIndexOrLock == 200 - amount,
                "Mirror tipping must use the strict >100 damage gate.");
        }
        f.Set(CollisionType::KnightShootsArrow);
        auto expected = f.Target();
        f.context.DamageObject(f.Target(), 20);
        Check(Equal(expected, f.Target()), "The arrow-shooting knight only sounds and restores strength.");
        f.Set(CollisionType::Type_138);
        f.Target().doorLockOrGroupBitmask = 300;
        expected = f.Target();
        expected.groupIndexOrLock = -10;
        expected.behavior = BehaviorType::Behavior_300;
        expected.collisionType = CollisionType::None;
        expected.displayType = DisplayType::SpriteTransparent;
        f.context.DamageObject(f.Target(), 20);
        Check(Equal(expected, f.Target()), "Collision 138 copies the complete behavior word.");
        for (const auto amount : { 4,5,6 })
        {
            f.Set(CollisionType::DiggableGround);
            f.context.selectedWeapon = WeaponType::Showel;
            f.context.weaponAnimationRecoil = 0;
            f.context.DamageObject(f.Target(), static_cast<std::int16_t>(amount));
            Check(f.Target().itemValue == (amount == 5 ? 5 : 2) &&
                f.Target().groupIndexOrLock == (amount == 5 ? 5 : 10) &&
                f.context.weaponAnimationRecoil == (amount == 5 ? 2 : 0),
                "Digging must require amount 5, use the object's rate, and set shovel recoil.");
        }
    }

    void TestDeaths()
    {
        Fixture f;
        for (const auto type : { CollisionType::PushingEnemy, CollisionType::SpiderLarge, CollisionType::Harvestman,
            CollisionType::Devil, CollisionType::BatIndoor, CollisionType::RatAndSnake, CollisionType::Cockroach,
            CollisionType::Crab, CollisionType::Dog, CollisionType::AnacondaHead, CollisionType::AnacondaTail,
            CollisionType::AttachableDamageObject, CollisionType::KrakenHead, CollisionType::FlyingArrow,
            CollisionType::DestroyOnCollide })
        {
            f.Set(type);
            f.Target().field_28 = 7;
            f.Target().doorLockOrGroupBitmask = 30;
            f.context.worldObjects[30].field_28 = 100;
            auto expected = f.Target();
            expected.groupIndexOrLock = -10;
            switch (type)
            {
            case CollisionType::PushingEnemy:
                expected.animFrame = 1; expected.modelFrame2 = 2; expected.modelAnim = 3;
                expected.collisionType = CollisionType::None; expected.field_07 = 0;
                expected.field_24 = 1; expected.field_26 = 0;
                expected.blendWeightOrSpriteOpacity = 0; expected.itemValue = 0; expected.renderPlacementMode = 3;
                break;
            case CollisionType::SpiderLarge:
                expected.animFrame = 5; expected.modelFrame2 = 4;
                expected.collisionType = CollisionType::None; expected.field_07 = 0;
                expected.velocityY = 10; expected.posY = 35; expected.field_24 = 1;
                expected.blendWeightOrSpriteOpacity = -100;
                break;
            case CollisionType::Harvestman:
                expected.modelFrame2 = 15; expected.collisionType = CollisionType::None; expected.field_07 = 0;
                expected.field_24 = 1; expected.field_26 = 0; expected.blendWeightOrSpriteOpacity = 0;
                break;
            case CollisionType::Devil:
                expected.modelFrame2 = 2; expected.modelAnim = 1; expected.animFrame = 4;
                expected.collisionType = CollisionType::None; expected.field_07 = 0;
                expected.field_24 = 1; expected.blendWeightOrSpriteOpacity = 100; expected.field_26 = 0;
                break;
            case CollisionType::BatIndoor:
                expected.velocityY = 2; expected.collisionType = CollisionType::None;
                expected.health = 2000; expected.blendWeightOrSpriteOpacity = -100;
                break;
            case CollisionType::RatAndSnake:
                expected.modelFrame2 = 2; expected.animFrame = 3;
                expected.collisionType = CollisionType::None; expected.field_07 = 0; expected.health = 1500;
                expected.velocityY = 8; expected.posY = 33; expected.blendWeightOrSpriteOpacity = -100;
                break;
            case CollisionType::Cockroach:
                expected.collisionType = CollisionType::None; expected.field_07 = 0;
                expected.itemValue = 7; expected.winX = 0; expected.winZ = 0;
                break;
            case CollisionType::Crab:
                expected.field_07 = 0; expected.behavior = BehaviorType::None;
                expected.modelAnim = 2; expected.animFrame = 1; expected.modelFrame2 = 0;
                break;
            case CollisionType::Dog:
                expected.itemValue = 100; expected.modelAnim = 1; expected.animFrame = 3; expected.modelFrame2 = 4;
                expected.blendWeightOrSpriteOpacity = 0; expected.field_26 = 0;
                expected.collisionType = CollisionType::None; expected.field_07 = 0;
                break;
            case CollisionType::AnacondaHead:
                expected.behavior = BehaviorType::Behavior_87; expected.health = 2000;
                expected.modelAnim = 3; expected.animFrame = 3; expected.modelFrame2 = 0; expected.renderPlacementMode = 1;
                break;
            case CollisionType::AnacondaTail:
                expected.health = 2000; expected.modelAnim = 3; expected.animFrame = 1;
                break;
            case CollisionType::AttachableDamageObject:
            case CollisionType::KrakenHead:
                expected.modelFrame2 = type == CollisionType::AttachableDamageObject ? 5 : 2;
                expected.collisionType = CollisionType::None; expected.field_07 = 0; expected.health = 2000;
                expected.blendWeightOrSpriteOpacity = 0; expected.behavior = BehaviorType::AttachableDamageObject;
                break;
            case CollisionType::FlyingArrow: expected.health = 0; break;
            case CollisionType::DestroyOnCollide:
                expected.field_07 = 0; expected.collisionType = CollisionType::None; expected.animFrame = 3;
                break;
            default: throw std::logic_error("Missing native death fixture.");
            }
            f.context.DamageObject(f.Target(), 20);
            Check(Equal(expected, f.Target()), "Native death transition changed an unexpected object byte.");
            Check(f.context.worldObjects[30].field_28 == (type == CollisionType::AttachableDamageObject ? 93 : 100),
                "An attached part must subtract its weight from the linked parent's field_28.");
        }
        for (const auto value : { 7,14,15,16 })
        {
            f.Set(CollisionType::Cockroach);
            f.Target().itemValue = static_cast<std::int16_t>(value);
            f.Target().field_07 = 131;
            f.Target().displayType = DisplayType::Hidden;
            f.context.DamageObject(f.Target(), 20);
            Check(f.Target().itemValue == (value == 15 ? 16 : value) && f.Target().field_07 == 131 &&
                f.Target().health == 0 && f.Target().blendWeightOrSpriteOpacity == (value == 15 ? -100 : 7),
                "Cockroach high phases, special collision mask and hidden death gate differ.");
        }
        f.Set(CollisionType::Dog, 60);
        f.Target().itemValue = 1;
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().itemValue == -2, "A living attacking dog must recoil without entering death.");
        f.Set(CollisionType::AnacondaHead, 60);
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().field_24 == 10, "A living anaconda head must install its hit reaction.");
        f.Set(CollisionType::Dragon, 10);
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().behavior == BehaviorType::Behavior_61 && f.Target().health == 1000 &&
            f.Target().modelAnim == 2 && f.Target().animFrame == 1 && f.Target().modelFrame2 == 2 &&
            f.Target().field_24 == 250 && f.Target().field_26 == 0 && f.Target().itemValue == 4 &&
            f.Target().blendWeightOrSpriteOpacity == 0 && f.Target().doorLockOrGroupBitmask == 0,
            "Dragon death must install behavior 61 only once.");
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().behavior == BehaviorType::Behavior_61 && f.Target().velocityY == 100,
            "An already-dying dragon still follows the native non-initialization reaction.");
        for (std::uint32_t seed = 0; seed < 100; ++seed)
        {
            f.Set(CollisionType::Dragon, 2000);
            f.context.randomSeed = seed;
            auto expectedSeed = seed;
            const bool roar = Next(expectedSeed, 3) == 0;
            f.context.DamageObject(f.Target(), 1);
            Check(f.Target().itemValue == (roar ? -130 : 2) && f.context.randomSeed == expectedSeed &&
                f.Target().velocityY == 100, "Dragon threshold crossing must draw Random(3) only on a changed thousand bucket.");
        }
    }

    void TestKnockback()
    {
        Fixture f;
        for (const auto amount : { 1,2,20,30,31,100 })
        {
            f.Set(CollisionType::PushingEnemy, 200);
            f.context.DamageObject(f.Target(), static_cast<std::int16_t>(amount));
            Check(f.Target().posX == (amount <= 1 ? 1500 : 1500 + std::min(amount, 30)) &&
                f.Target().posZ == 2000 && f.Target().itemValue == (amount <= 1 ? 2 : 0),
                "Living pushing enemies must move away from the player with the native 30-unit cap.");
        }
        f.Set(CollisionType::PushingEnemy, 200);
        f.level.collisionMap->data[84 * 200 + 57] = 0x38;
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().posX == 1500, "Blocked knockback must not move the target.");
        f.Set(CollisionType::PushingEnemy, 200);
        std::fill(std::begin(f.level.collisionMap->data), std::end(f.level.collisionMap->data), std::uint8_t{0x38});
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().posX == 1520, "Native knockback tests corrections, not the center pixel; symmetric obstacles cancel.");
        f.Set(CollisionType::PushingEnemy, 200);
        f.context.soundListenerX = 1500;
        bool rejected = false;
        try { f.context.DamageObject(f.Target(), 20); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "The native zero-distance division fault must produce an explicit diagnostic.");
        f.Set(CollisionType::PushingEnemy, 200);
        f.context.soundListenerX = Word(1500 - 32768);
        f.context.soundListenerZ = Word(2000 + 32767);
        rejected = false;
        try { f.context.DamageObject(f.Target(), 3); }
        catch (const std::overflow_error&) { rejected = true; }
        Check(rejected, "Native signed-word division overflow must also produce an explicit diagnostic.");
    }

    void TestGeometryAndGroups()
    {
        Fixture f;
        f.Set(CollisionType::WallDestruction);
        f.level.wallTextures[20][14] = 123;
        std::fill(std::begin(f.level.collisionMap->data), std::end(f.level.collisionMap->data), std::uint8_t{0x3f});
        auto expected = *f.level.collisionMap;
        for (int x = 54; x <= 57; ++x)
        {
            for (int z = 83; z <= 86; ++z) { expected.data[z * 200 + x] = 0; }
        }
        f.context.DamageObject(f.Target(), 0);
        Check(f.Target().health == 0 && f.Target().posX == 15 && f.Target().posZ == 21 &&
            f.level.wallTextures[20][14] == 0 && f.level.geometryRevision == 1 &&
            std::memcmp(&expected, f.level.collisionMap.get(), sizeof(expected)) == 0,
            "Wall destruction must erase only the masked 4x4 rectangle and exact one-based wall cell.");
        f.Set(CollisionType::WaterRecharge);
        f.Target().itemValue = 513;
        const auto heights = f.level.floorHeights[20][14];
        f.context.DamageObject(f.Target(), 0);
        Check(f.Target().collisionType == CollisionType::None && f.level.floorObjects[20][14] == 1 &&
            f.level.floorHeights[20][14] == heights && f.level.geometryRevision == 2,
            "Water-recharge activation writes the floor-object byte, not terrain height.");
        std::vector<std::uint8_t> geometry(4 + 7 * 50 * 40, 0);
        geometry[0] = 49; geometry[2] = 39;
        geometry[4 + 2000 + 1014] = 93;
        f.level.LoadGeometry(geometry.data(), geometry.size());
        Check(f.level.floorObjects[20][14] == 93, "FELD plane 1 must now load into the game's floor-object array.");
        f.context.worldObjects = {};
        f.Set(CollisionType::Switch);
        auto& button = f.Target();
        button.itemValue = 10; button.blendWeightOrSpriteOpacity = 0;
        for (const std::size_t slot : { 2,140,141 })
        {
            auto& target = f.context.worldObjects[slot];
            target.health = 1010; target.collisionType = CollisionType::Ghost;
            target.groupIndexOrLock = 60;
        }
        f.context.DamageObject(button, 20);
        Check(button.field_26 == 10 && button.groupIndexOrLock == 1 &&
            f.context.worldObjects[2].health == 1000 && f.context.worldObjects[140].health == 1000 &&
            f.context.worldObjects[141].health == 1010,
            "Damage-triggered switches must activate all native target types, only in slots 2..140.");
        f.context.worldObjects[2].health = 1010;
        button.field_26 = 0;
        button.field_24 = 6;
        Check(HandleSwitchInteraction(button, f.context).status == InteractionStatus::Applied &&
            button.field_26 == 6 && f.context.worldObjects[2].health == 1000,
            "Explicit switch interaction must use the same general activation dispatcher.");
    }

    struct Audio final : AudioHandler
    {
        std::array<bool, SoundChannelCount> playing{};
        int stops = 0;
        std::vector<std::uint8_t> samples;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
        void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t) override
        {
            playing.at(channel) = true;
            samples.push_back(sample);
        }
        void StopAllSamples() override { ++stops; playing.fill(false); }
    };

    void TestVaseAndAudio()
    {
        Fixture f;
        Audio audio;
        f.context.audioHandler = &audio;
        f.context.soundEffectsLoaded = true;
        for (std::size_t i = 0; i < SoundEffectCount; ++i)
        {
            f.context.soundEffects[i] = { 1, { static_cast<std::uint8_t>(i + 1),0,0,0 } };
        }
        for (std::uint8_t variant = 0; variant < 2; ++variant)
        {
            f.Set(CollisionType::SpiderLarge, 60);
            f.Target().soundEffectId = variant;
            audio.playing.fill(false);
            f.context.DamageObject(f.Target(), 20);
            Check(audio.samples.back() == (variant == 0 ? 13 : 26), "Spider hit sound must use the native DS:0483 pair byte.");
            f.context.DamageObject(f.Target(), 60);
            Check(audio.samples.back() == (variant == 0 ? 16 : 26) && audio.stops == variant + 1,
                "Spider death must stop all four native channels before playing the DS:0482 sound.");
        }
        f.Set(CollisionType::VaseToGrab);
        f.Target().field_28 = 7;
        f.Target().soundEffectId = 5;
        auto expected = f.Target();
        f.context.DamageObject(f.Target(), 199);
        Check(Equal(f.Target(), expected), "Vases must reject sub-200 damage and restore strength.");
        f.context.DamageObject(f.Target(), 200);
        Check(audio.stops == 3 && audio.samples.back() == 5 && f.Target().collisionType == CollisionType::Key &&
            f.Target().modelId == 39 && f.Target().modelAnim == 1 && f.Target().animFrame == 1 &&
            f.Target().modelFrame2 == 3 && f.Target().itemValue == 7 && f.Target().field_07 == 3 &&
            f.Target().field_32 == 4 && f.Target().displayType == DisplayType::VectorUnlit &&
            f.Target().renderPlacementMode == 5,
            "A broken vase must retain its copied broken shell and initialize the original record as a key.");
        Check(f.context.worldObjects[2].collisionType == CollisionType::None &&
            f.context.worldObjects[2].behavior == BehaviorType::None &&
            f.context.worldObjects[2].animFrame == 3 && f.context.worldObjects[2].health == 1000,
            "The vase shell copy must preserve the native post-break record.");
        f.context.StopAllSoundEffects();
        Check(audio.stops == 4 && f.context.soundChannelSources == std::array<std::int16_t, SoundChannelCount>{},
            "Stopping audio must also clear channel-source admission state.");
    }

    void TestSecondaryEffects()
    {
        Fixture f;
        f.Set(CollisionType::Shark);
        f.context.DamageObject(f.Target(), 20);
        Check(f.Target().modelAnim == 3 && f.Target().velocityY == 3 && f.Target().health == 2000 &&
            f.Target().animFrame == 1 && f.Target().modelFrame2 == 2 && f.Target().itemValue == 200 &&
            f.context.worldObjects[2].behavior == BehaviorType::WaterImpact &&
            static_cast<int>(f.context.worldObjects[2].displayType) == 4,
            "Shark damage must enter its death phase and always emit the native water impact.");
        f.context.worldObjects = {};
        f.Set(CollisionType::ActivateSpiders);
        f.context.randomSeed = 123;
        auto seed = f.context.randomSeed;
        f.context.DamageObject(f.Target(), 199);
        Check(f.Target().groupIndexOrLock == 10 && f.Target().health == 1000 &&
            f.context.worldObjects[2].health == 0, "Spider activators must reject sub-200 damage.");
        f.context.DamageObject(f.Target(), 200);
        for (int i = 0; i < 180; ++i) { Next(seed, 1); }
        Check(f.context.randomSeed == seed, "Twenty subtype-8 factory calls must consume exactly nine draws apiece.");
        int particles = 0;
        for (const auto& object : f.context.worldObjects)
        {
            if (object.behavior == BehaviorType::FlyingArrow && object.health > 0)
            {
                ++particles;
                Check(object.modelId == 1 && object.modelAnim == 5 &&
                    object.collisionType == CollisionType::FlyingArrow && object.itemValue == 6 &&
                    object.health >= 25 && object.health <= 34 && object.behaviorParameter == 70 &&
                    object.spriteScale >= 40 && object.spriteScale <= 59 &&
                    object.collisionRadiusXZ == 55 && object.collisionRadiusY == 25 &&
                    object.renderPlacementMode == 3 && object.blendWeightOrSpriteOpacity == 20,
                    "Activation's subtype-8 projectiles must retain the exact factory record shape.");
            }
        }
        Check(f.Target().health == 35 && particles == 20, "Spider activators emit exactly twenty projectiles.");
        f.context.worldObjects = {};
        f.Set(CollisionType::DamageObj);
        seed = f.context.randomSeed;
        f.context.DamageObject(f.Target(), -1);
        for (int i = 0; i < 30; ++i) { Next(seed, 1); }
        Check(f.context.randomSeed == seed, "Five smoke particles must preserve the native six-draw allocation/initialization order.");
        Check(f.Target().behavior == BehaviorType::WalkableDrawBridge && f.Target().animFrame == 3 &&
            f.Target().modelFrame2 == 2 && f.Target().blendWeightOrSpriteOpacity == 100 &&
            f.Target().velocityZ == 3 && static_cast<int>(f.Target().displayType) == 80 && f.Target().field_07 == 0,
            "The special damage-object transition must preserve the native behavior value 49 and frame swap.");
        int smoke = 0;
        for (const auto& object : f.context.worldObjects)
        {
            if (object.behavior == BehaviorType::SmokeFading && object.health > 0) { ++smoke; }
        }
        Check(smoke == 5, "Special damage-object destruction must create five smoke particles.");
        f.context.worldObjects = {};
        f.Set(CollisionType::Type_22);
        f.Target().velocityX = 2;
        f.Target().velocityZ = -3;
        seed = f.context.randomSeed;
        f.context.DamageObject(f.Target(), 0);
        for (int i = 0; i < 81; ++i) { Next(seed, 1); }
        Check(f.context.randomSeed == seed, "Nine zero-selector fragments must consume exactly nine draws apiece.");
        int fragments = 0;
        for (const auto& object : f.context.worldObjects)
        {
            if (object.behavior == BehaviorType::Behavior_1 && object.health > 0)
            {
                ++fragments;
                Check(object.modelId == 1 && object.modelAnim == 5 && object.field_24 == 1 &&
                    object.health >= 50 && object.health <= 99 && object.velocityY >= 2 && object.velocityY <= 7,
                    "Collision 22 zero-selector fragments must use native sprite-fragment fields.");
            }
        }
        Check(fragments == 9 && f.Target().health == 0 && f.Target().posX == 1496 && f.Target().posZ == 2006,
            "Collision 22 must back up two velocities and emit exactly nine breakup fragments.");
        f.context.worldObjects = {};
        f.Set(CollisionType::Type_22);
        f.Target().field_24 = 30;
        auto& linked = f.context.worldObjects[30];
        linked.health = 1000; linked.posX = 1500; linked.posY = 25; linked.posZ = 2000;
        linked.collisionRadiusXZ = 20; linked.collisionRadiusY = 10; linked.renderPlacementMode = 5;
        seed = f.context.randomSeed;
        f.context.DamageObject(f.Target(), 20);
        for (int i = 0; i < 90; ++i) { Next(seed, 1); }
        Check(f.context.randomSeed == seed, "Linked breakup smoke must retain the initial three draws before smoke initialization.");
        smoke = 0;
        for (const auto& object : f.context.worldObjects)
        {
            if (object.behavior == BehaviorType::SmokeFading && object.health > 0)
            {
                ++smoke;
                Check(object.renderPlacementMode == 5 && object.collisionRadiusXZ == 20,
                    "Nonzero breakup selectors must seed smoke from the linked object's bounds and placement.");
            }
        }
        Check(smoke == 9, "Nonzero collision-22 selectors must emit nine linked-object smoke particles.");
    }

    void TestFallbackAllocation()
    {
        Fixture f;
        for (auto& object : f.context.worldObjects) { object.health = 1000; }
        f.Set(CollisionType::Type_22);
        auto seed = f.context.randomSeed;
        f.context.DamageObject(f.Target(), 0);
        for (int i = 0; i < 81; ++i) { Next(seed, 1); }
        Check(f.Target().health == 0 && f.context.worldObjects[165].behavior == BehaviorType::Behavior_1 &&
            f.context.randomSeed == seed, "A full world must repeatedly reuse fallback 165 without truncating the nine-fragment loop.");
        for (auto& object : f.context.worldObjects) { object = {}; object.health = 1000; }
        f.Set(CollisionType::ActivateSpiders);
        f.context.DamageObject(f.Target(), 200);
        Check(f.Target().health == 35 && f.context.worldObjects[165].behavior == BehaviorType::FlyingArrow,
            "The twenty activation particles must use the native reclaiming allocator and its fallback slot.");
        for (auto& object : f.context.worldObjects) { object = {}; object.health = 1000; }
        f.Set(CollisionType::VaseToGrab);
        f.Target().health = 0;
        f.Target().field_28 = 7;
        f.context.DamageObject(f.Target(), 200);
        Check(f.Target().collisionType == CollisionType::Key && f.Target().health == 0 &&
            f.Target().itemValue == 0 && f.Target().posX == 0,
            "Vase allocation must read the source after clearing it, including native self-aliasing semantics.");
    }
}

int main()
{
    try
    {
        TestCollider();
        TestDefaultAndActivation();
        TestArithmeticAndSpecialTargets();
        TestDeaths();
        TestKnockback();
        TestGeometryAndGroups();
        TestVaseAndAudio();
        TestSecondaryEffects();
        TestFallbackAllocation();
        std::cout << "Weapon collider, shared damage/activation and secondary effects passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
