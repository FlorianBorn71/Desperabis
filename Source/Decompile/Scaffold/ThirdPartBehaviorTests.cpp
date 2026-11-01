#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
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

    int Word(int value)
    {
        const auto bits = static_cast<unsigned int>(value) & 65535;
        return bits < 32768 ? static_cast<int>(bits) : static_cast<int>(bits) - 65536;
    }

    int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = seed * 134775813u + 1;
        return static_cast<int>((seed >> 16) % range);
    }

    struct Input final : InputHandler
    {
        bool backward = false;
        bool IsKeyPressed(InputKey key) override { return backward && key == InputKey::BackwardKey; }
    };

    struct Host final : GameContext
    {
        int destroyed = 0, healthChanges = 0;
        void OnObjectDestroyed(int) override { ++destroyed; }
        void OnPlayerHealthChanged(std::int32_t, std::int32_t) override { ++healthChanges; }
        void Load()
        {
            const auto request = StartLevel(10);
            LevelLoadData data;
            data.level = std::make_unique<Level>(CreateDummyLevel());
            data.playerSpawn = PlayerCamera{{1500, 45, 2500}, {0, 0, 100}};
            CompleteLevelLoad(request.id, std::move(data));
            lightingCache.Reset(1500, 2500);
        }
    protected:
        void LoadLevel(const LevelLoadRequest&) override {}
    };

    RawGameObject Object(BehaviorType behavior, CollisionType collision)
    {
        RawGameObject object;
        object.behavior = behavior;
        object.collisionType = collision;
        object.health = 1000;
        object.posX = 1500;
        object.posZ = 2500;
        object.modelId = 31;
        object.displayType = DisplayType::VectorNormal;
        object.collisionMask = 255;
        object.collisionRadiusXZ = 30;
        object.collisionRadiusY = 10;
        return object;
    }

    void Step(Host& host)
    {
        Input input;
        Check(host.Tick({0, 1}, input).status == GameTickStatus::Advanced, "Player tick did not advance.");
    }

    void TestPush()
    {
        for (const auto collision : {CollisionType::PushPlayer, CollisionType::TriggerDebris})
        {
            Host host;
            host.Load();
            auto pusher = Object(BehaviorType::None, collision);
            pusher.posX -= 10;
            pusher.posY = host.worldObjects[1].posY;
            pusher.collisionRadiusY = 45;
            host.ReplaceWorldObject(2, pusher);
            const int radius = 3 * (host.worldObjects[1].collisionRadiusXZ + pusher.collisionRadiusXZ) / 2 + 5;
            Step(host);
            Check(host.worldObjects[1].posX == pusher.posX + radius &&
                host.worldObjects[1].posZ == pusher.posZ && host.playerLife == 360 * 65536,
                "Push contacts must separate radially without inflicting default damage.");
            const auto camera = host.GetPlayerCamera();
            Check(camera.position[0] == host.worldObjects[1].posX, "Push must reach the authoritative camera.");
            Step(host);
            Check(host.worldObjects[1].posX == pusher.posX + radius, "Separated player must not drift every tick.");
        }
        Host host;
        host.Load();
        auto pusher = Object(BehaviorType::None, CollisionType::PushPlayer);
        pusher.posX -= 10;
        pusher.posZ -= 10;
        pusher.posY = host.worldObjects[1].posY;
        pusher.collisionRadiusY = 45;
        host.ReplaceWorldObject(2, pusher);
        const int radius = 3 * (host.worldObjects[1].collisionRadiusXZ + pusher.collisionRadiusXZ) / 2 + 5;
        const int offset = static_cast<int>(std::nearbyint(radius / std::sqrt(2.0)));
        Step(host);
        Check(host.worldObjects[1].posX == pusher.posX + offset &&
            host.worldObjects[1].posZ == pusher.posZ + offset, "Diagonal separation must use rounded radial offsets.");

        host.Load();
        pusher.posX = 1490;
        pusher.posZ = 2500;
        host.ReplaceWorldObject(2, pusher);
        host.SetCollisionMapPixelAtWorldXZ(static_cast<std::int16_t>(1490 + radius), 2500, 0x18);
        Step(host);
        Check(host.worldObjects[1].posX == 1500, "Push must not place the player inside a blocking wall.");

        host.Load();
        pusher.posX = 1500;
        host.ReplaceWorldObject(2, pusher);
        Step(host);
        Check(host.worldObjects[1].posX == 1500 && host.worldObjects[1].posZ == 2500,
            "Coincident centers must remain finite without an invented push direction.");
        pusher.posX = 1490;
        pusher.behaviorParameter = 2;
        host.ReplaceWorldObject(2, pusher);
        const auto life = host.playerLife;
        Step(host);
        Check(host.playerLife == life - 2 * 29491, "Push factor above one must apply pre-separation life damage.");

        host.Load();
        pusher.behaviorParameter = 0;
        host.ReplaceWorldObject(2, pusher);
        host.DEBUG_noPlayerCollision = true;
        Step(host);
        Check(host.worldObjects[1].posX == 1500, "Player collision cheat must bypass radial pushing.");
        host.DEBUG_noPlayerCollision = false;
        host.field_D500 = false;
        Step(host);
        Check(host.worldObjects[1].posX == 1500, "DS:D500 must gate expanded player contact collection.");
        host.field_D500 = true;
        host.worldObjects[2].behaviorParameter = 1;
        Input input;
        Check(UpdatePlayerInteraction(host, input).status == InteractionStatus::Applied &&
            host.playerLife == 0 && host.positionalSoundsSuppressed &&
            host.worldObjects[1].health == 1000, "Fatal residual push contact must use player-life depletion.");
    }

    void ReferenceCrab(RawGameObject& object, Host& host, std::uint32_t& seed)
    {
        if (object.health != 1000) { return; }
        object.posY = host.GetTerrainFloorHeight(object.posX, object.posY, object.posZ);
        if (Next(seed, 30) == 0) { object.itemValue = static_cast<std::int16_t>(Next(seed, 200) + 60); }
        if (Next(seed, 50) == 0) { object.itemValue = 0; }
        if (Next(seed, 20) == 0)
        {
            int turn = Next(seed, 41) - 20;
            if (turn == 0) { turn = 20; }
            object.winY = static_cast<std::int16_t>(Word(object.winY + turn) & 511);
        }
        std::int16_t dx, dz;
        GetClampedDirectionDisplacement(dx, dz, object.winY, object.itemValue);
        const auto sign = [](int value) { return (value > 0) - (value < 0); };
        const int material = object.generalPurpose1 & 255;
        const bool blocked =
            (host.GetCollisionMapPixelAtWorldXZ(static_cast<std::int16_t>(Word(object.posX + 30 * sign(dx))),
                object.posZ) & 63) != material ||
            (host.GetCollisionMapPixelAtWorldXZ(object.posX,
                static_cast<std::int16_t>(Word(object.posZ + 30 * sign(dz)))) & 63) != material;
        if (blocked) { object.winY = static_cast<std::int16_t>(Word(object.winY + 11 * sign(object.itemValue)) & 511); }
        else
        {
            const int x = Word(object.velocityX + dx), z = Word(object.velocityZ + dz);
            object.posX = static_cast<std::int16_t>(Word(object.posX + static_cast<int>(std::floor(x / 256.0))));
            object.posZ = static_cast<std::int16_t>(Word(object.posZ + static_cast<int>(std::floor(z / 256.0))));
            object.velocityX = static_cast<std::int16_t>(x & 255);
            object.velocityZ = static_cast<std::int16_t>(z & 255);
        }
        object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
            Word(object.blendWeightOrSpriteOpacity + object.itemValue / 20) % 200);
    }

    void TestCrab()
    {
        Host host;
        host.Load();
        auto crab = Object(BehaviorType::Crab, CollisionType::Crab);
        crab.modelId = 37;
        crab.generalPurpose1 = 1;
        crab.itemValue = 60;
        crab.velocityX = 250;
        crab.velocityZ = 128;
        crab.blendWeightOrSpriteOpacity = 198;
        for (int sample = 0; sample < 80; ++sample)
        {
            std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data),
                static_cast<std::uint8_t>(sample % 2));
            auto actual = crab;
            actual.winY = static_cast<std::int16_t>((sample * 73) & 511);
            actual.itemValue = sample % 3 == 0 ? -199 : 259;
            auto expected = actual;
            host.randomSeed = static_cast<std::uint32_t>(sample * 54321 + 1);
            auto seed = host.randomSeed;
            host.tickCount = 20;
            for (int tick = 0; tick < host.tickCount; ++tick) { ReferenceCrab(expected, host, seed); }
            Check(DispatchObjectCase(actual, host).status == DispatchStatus::ExecutedCase &&
                std::memcmp(&actual, &expected, sizeof(actual)) == 0 && seed == host.randomSeed,
                "Crab state and RNG must match native per-tick movement, turns and animation.");
        }
        crab.health = 1001;
        const auto before = crab;
        const auto seed = host.randomSeed;
        DispatchObjectCase(crab, host);
        Check(std::memcmp(&crab, &before, sizeof(crab)) == 0 && seed == host.randomSeed,
            "Dormant crab must not move, animate or consume RNG.");
        crab.health = 1000;
        crab.collisionMask = 1;
        Input input;
        input.backward = true;
        host.Tick({0, 1}, input);
        input.backward = false;
        Step(host);
        Step(host);
        crab.posZ = host.worldObjects[1].posZ;
        crab.posY = host.worldObjects[1].posY;
        crab.behaviorParameter = 3;
        host.ReplaceWorldObject(2, crab);
        const auto life = host.playerLife;
        UpdatePlayerInteraction(host, input);
        Check(host.playerLife == life - 3 * 29491 && host.playerHitEffectTicks == 8,
            "Slow contact with an enabled crab must apply its configured damage factor.");
        input.backward = true;
        host.Tick({0, 1}, input);
        Check(host.worldObjects[2].behavior == BehaviorType::None &&
            host.worldObjects[2].collisionMask == 0 && host.worldObjects[2].modelAnim == 2 &&
            host.worldObjects[2].animFrame == 1, "Walking over a crab must reuse the native death reaction.");
    }

    void TestOil()
    {
        Host host;
        host.Load();
        auto oil = Object(BehaviorType::BurnableOilPuddle, CollisionType::BurnableOilPuddle);
        host.ReplaceWorldObject(2, oil);
        auto& burning = host.worldObjects[2];
        host.tickCount = 1;
        const auto seed = host.randomSeed;
        DispatchObjectCase(burning, host);
        Check(host.randomSeed == seed && burning.behaviorParameter == 0 && burning.health == 1000,
            "Unlit oil must not damage, emit light or consume RNG.");
        burning.displayType = static_cast<DisplayType>(115);
        DispatchObjectCase(burning, host);
        Check(host.randomSeed == seed, "Oil helper must wait for display strictly below 115.");
        burning.displayType = static_cast<DisplayType>(114);
        auto expectedSeed = host.randomSeed;
        const int light = Next(expectedSeed, 40);
        const int age = Next(expectedSeed, 2);
        const int warm = Next(expectedSeed, 5) < 2;
        DispatchObjectCase(burning, host);
        Check(host.randomSeed == expectedSeed && burning.blendWeightOrSpriteOpacity == age &&
            burning.behaviorParameter == 5 && burning.collisionRadiusXZ == (110 - age) / 4 &&
            static_cast<int>(burning.displayType) == 114 + warm &&
            host.lightingCache.contributions[10][10] == light,
            "Burning oil must follow the native light, age, damage, warming and shrinking formulas.");
        auto neighbor = oil;
        neighbor.posX += 45;
        host.ReplaceWorldObject(3, neighbor);
        neighbor.collisionType = CollisionType::Weapon;
        neighbor.posX = 1500;
        host.ReplaceWorldObject(4, neighbor);
        neighbor.displayType = DisplayType::VectorBrightness1;
        neighbor.collisionType = CollisionType::BurnableOilPuddle;
        host.ReplaceWorldObject(5, neighbor);
        neighbor = oil;
        neighbor.collisionMask = 0;
        host.ReplaceWorldObject(6, neighbor);
        neighbor = oil;
        host.ReplaceWorldObject(165, neighbor);
        burning.collisionRadiusXZ = 30;
        burning.blendWeightOrSpriteOpacity = 99;
        burning.displayType = static_cast<DisplayType>(100);
        host.objectCollisionsEnabled = false;
        DispatchObjectCase(burning, host);
        Check(host.worldObjects[3].displayType == DisplayType::LastVectorBurning &&
            host.worldObjects[165].displayType == DisplayType::LastVectorBurning &&
            host.worldObjects[4].displayType == DisplayType::VectorNormal &&
            host.worldObjects[5].displayType == DisplayType::VectorBrightness1 &&
            host.worldObjects[6].displayType == DisplayType::VectorNormal,
            "Oil must collect pre-shrink contacts including slot 165, while excluding weapons and masked/bright objects.");
        host.objectCollisionsEnabled = true;
        burning.collisionRadiusXZ = 40;
        burning.blendWeightOrSpriteOpacity = 100;
        burning.behaviorParameter = 6;
        burning.displayType = static_cast<DisplayType>(100);
        DispatchObjectCase(burning, host);
        Check(burning.behaviorParameter == 0 && burning.collisionRadiusXZ == 40 &&
            burning.blendWeightOrSpriteOpacity == 100, "Spent oil must stop damage; radius >=31 must remain authored.");
        burning.behaviorParameter = 7;
        DispatchObjectCase(burning, host);
        Check(burning.behaviorParameter == 7, "Authored oil damage factors >=7 must not be overwritten.");
        host.field_D500 = false;
        host.worldObjects[3].displayType = DisplayType::VectorNormal;
        DispatchObjectCase(burning, host);
        Check(host.worldObjects[3].displayType == DisplayType::VectorNormal, "DS:D500 must gate oil spreading.");
        host.field_D500 = true;
        burning.displayType = static_cast<DisplayType>(50);
        burning.behaviorParameter = 5;
        burning.posY = host.worldObjects[1].posY;
        Input input;
        const auto life = host.playerLife;
        UpdatePlayerInteraction(host, input);
        Check(host.playerLife == life - 5 * 29491, "Burning oil contact must damage player life.");
        burning.displayType = DisplayType::VectorNormal;
        UpdatePlayerInteraction(host, input);
        Check(host.playerLife == life - 5 * 29491, "Unlit oil must not retain a contact hazard.");
        burning.displayType = DisplayType::VectorBrightness2;
        host.tickCount = 8;
        DispatchObjectCase(burning, host);
        Check(burning.health == 0 && host.destroyed == 1, "Burnout must notify destruction exactly once.");
    }

    void TestOriginalLevels(const std::filesystem::path& root)
    {
        for (const int level : {10, 12, 13})
        {
            std::ifstream input(root / ("OBJPOS" + std::to_string(level) + ".DAT"), std::ios::binary);
            Check(static_cast<bool>(input), "Cannot read original third-part fixture.");
            const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
            Check(bytes.size() % 54 == 0, "Original OBJPOS must contain whole native records.");
            int count = 0;
            Host host;
            host.Load();
            for (std::size_t offset = 0; offset < bytes.size(); offset += 54)
            {
                auto object = DecodeGameObject(bytes.data() + offset, 54);
                if (object.health <= 0) { continue; }
                if (level == 10 && object.collisionType == CollisionType::PushPlayer)
                {
                    Check(object.behaviorParameter == 0 && object.collisionMask == 1,
                        "Level-10 push fixtures must remain non-damaging colliders.");
                    object.posX = 1490;
                    object.posZ = 2500;
                    object.posY = host.worldObjects[1].posY;
                    host.ReplaceWorldObject(2, object);
                    Step(host);
                    Check(host.worldObjects[1].posX > 1500, "Original level-10 push collider must separate player.");
                    host.Load();
                    ++count;
                }
                if (level == 12 && object.behavior == BehaviorType::Crab)
                {
                    Check(object.collisionMask == 0 && object.generalPurpose1 == 1,
                        "Original crabs must retain their passive mask and allowed terrain material.");
                    std::fill(std::begin(host.collisionMap->data), std::end(host.collisionMap->data), std::uint8_t{1});
                    host.tickCount = 100;
                    auto expected = object;
                    auto seed = host.randomSeed;
                    for (int tick = 0; tick < host.tickCount; ++tick) { ReferenceCrab(expected, host, seed); }
                    DispatchObjectCase(object, host);
                    Check(std::memcmp(&object, &expected, sizeof(object)) == 0 &&
                        host.randomSeed == seed && object.collisionMask == 0, "Original crab fixture must run recovered AI.");
                    ++count;
                }
                if (level == 13 && object.behavior == BehaviorType::BurnableOilPuddle)
                {
                    const auto slot = static_cast<int>(offset / 54 + 2);
                    host.ReplaceWorldObject(slot, object);
                    auto& puddle = host.worldObjects[slot];
                    host.tickCount = host.frameTickCount = 1;
                    auto activated = object;
                    activated.health = 1001;
                    host.DamageObject(activated, 0);
                    Check(activated.displayType == DisplayType::LastVectorBurning, "Existing activation must ignite authored oil.");
                    auto fireball = Object(BehaviorType::FireballProjectile, CollisionType::None);
                    fireball.modelId = 1;
                    fireball.health = 100;
                    fireball.groupIndexOrLock = 12;
                    fireball.posX = puddle.posX;
                    fireball.posY = puddle.posY;
                    fireball.posZ = puddle.posZ;
                    host.ReplaceWorldObject(3, fireball);
                    DispatchObjectCase(host.worldObjects[3], host);
                    Check(puddle.health == object.health && puddle.displayType == DisplayType::LastVectorBurning,
                        "A fired fireball must ignite the unmodified original oil fixture.");
                    host.SetObjectHealth(host.worldObjects[3], 0);
                    int frames = 0;
                    while (puddle.health > 0 && frames++ < 5000) { UpdateObject(puddle, host); }
                    Check(puddle.health == 0 && frames < 5000 && puddle.blendWeightOrSpriteOpacity == 100 &&
                        puddle.behaviorParameter == 0 && host.destroyed > 0,
                        "Original oil must ignite, age, stop damaging and reach renderer-visible burnout.");
                    ++count;
                }
            }
            Check(count == (level == 10 ? 2 : level == 12 ? 3 : 10), "Unexpected original push/crab/oil fixture count.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: third-part-behavior-tests ORIGINAL_LEVEL_DIRECTORY");
        TestPush();
        TestCrab();
        TestOil();
        TestOriginalLevels(argv[1]);
        std::cout << "Push-player, crab and burnable-oil tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
