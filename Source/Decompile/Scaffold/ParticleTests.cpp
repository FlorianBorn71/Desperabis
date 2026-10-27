#include "BehaviorDispatch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    std::int16_t Word(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    int Shift(int value, int divisor)
    {
        return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
    }

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1u) % 4294967296ULL);
        return range == 0 ? 0 : (seed >> 16) % range;
    }

    bool Equal(const RawGameObject& first, const RawGameObject& second)
    {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    }

    RawGameObject DirtyRecord()
    {
        RawGameObject object;
        std::array<std::uint8_t, sizeof(object)> data{};
        data.fill(0xa5);
        std::memcpy(&object, data.data(), data.size());
        return object;
    }

    RawGameObject DriftUpdate(RawGameObject object, int ticks)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            object.posX = Word(object.posX + object.velocityX);
            object.posY = Word(object.posY + object.velocityY);
            object.posZ = Word(object.posZ + object.velocityZ);
            if (object.itemValue % 2 != 0) { object.velocityY = Word(object.velocityY - 1); }
            object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity - 5);
            if (object.blendWeightOrSpriteOpacity < 0) { object.health = 0; }
            object.itemValue = Word(object.itemValue + (object.itemValue % 2 != 0 ? -1 : 1));
            object.displayType = static_cast<DisplayType>(
                std::clamp(37 - Shift(object.blendWeightOrSpriteOpacity, 8), 30, 36));
        }
        return object;
    }

    RawGameObject SmokeInit(RawGameObject object, std::int16_t x, std::int16_t y, std::int16_t z,
        std::int16_t radius, std::int16_t factor, std::uint8_t placement, std::int16_t floor,
        std::uint32_t& seed)
    {
        object.modelId = 1;
        object.modelAnim = 2;
        object.animFrame = 3;
        object.behavior = BehaviorType::SmokeFading;
        object.displayType = DisplayType::SpriteTransparent;
        object.blendWeightOrSpriteOpacity = 10;
        object.winY = static_cast<std::int16_t>(Next(seed, 2));
        object.collisionRadiusXZ = radius;
        object.collisionRadiusY = 5;
        object.velocityZ = 2;
        object.health = Word(factor * 50 + static_cast<int>(Next(seed, 50)));
        const auto size = (Next(seed, static_cast<std::uint16_t>(radius)) + 10) % 256;
        object.spriteScale = static_cast<std::uint8_t>(size);
        object.velocityY = Word(factor * 200 + static_cast<int>(Next(seed, 130)));
        object.posX = x;
        object.posY = std::max(y, floor);
        object.posZ = z;
        object.renderPlacementMode = placement;
        return object;
    }

    RawGameObject SmokeUpdate(RawGameObject object, int ticks)
    {
        object.health = static_cast<std::int16_t>(std::max(0, object.health - ticks));
        const auto fraction = Word(object.itemValue + Word(object.velocityY * ticks));
        object.posY = Word(object.posY + Shift(fraction, 256));
        object.itemValue = static_cast<std::int16_t>(static_cast<std::uint16_t>(fraction) % 256);
        object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(std::min(5, object.health / 4));
        const auto high = object.spriteScale;
        if (high < 200)
        {
            object.spriteScale = static_cast<std::uint8_t>((high + object.velocityZ * ticks) % 256);
        }
        object.velocityY = static_cast<std::int16_t>(std::max(0, object.velocityY - ticks * 2));
        return object;
    }

    void TestDriftingParticle()
    {
        GameContext context;
        for (const auto ticks : { 0, 1, 2, 7, 31 })
        {
            context.tickCount = static_cast<std::int16_t>(ticks);
            for (unsigned int value = 0; value <= 65535; ++value)
            {
                auto object = DirtyRecord();
                object.behavior = BehaviorType::DriftingParticle;
                object.health = 50;
                object.blendWeightOrSpriteOpacity = Word(value);
                object.itemValue = Word(value * 3);
                object.velocityX = Word(value * 5);
                object.velocityY = Word(value * 7);
                object.velocityZ = Word(value * 11);
                const auto expected = DriftUpdate(object, ticks);
                const auto result = Function_DriftingParticle(object, context);
                Check(Equal(object, expected) && result.status == DispatchStatus::ExecutedCase &&
                    result.behavior == BehaviorType::DriftingParticle && result.moduleOffset == 0x16e5 &&
                    context.randomSeed == 0, "Drifting-particle tick arithmetic or untouched fields differ.");
            }
        }
        context.tickCount = 32767;
        auto object = DirtyRecord();
        object.behavior = BehaviorType::DriftingParticle;
        auto expected = DriftUpdate(object, context.tickCount);
        Function_DriftingParticle(object, context);
        Check(Equal(object, expected), "The maximum tick count must not truncate or stop early after death.");

        context.tickCount = 20;
        object = {};
        object.behavior = BehaviorType::DriftingParticle;
        object.health = 50;
        object.blendWeightOrSpriteOpacity = 100;
        expected = DriftUpdate(object, 20);
        Function_DriftingParticle(object, context);
        Check(Equal(object, expected) && object.health == 50 && object.blendWeightOrSpriteOpacity == 0,
            "Exactly zero fade must not kill the particle.");
        context.tickCount = 2;
        expected = DriftUpdate(object, 2);
        Function_DriftingParticle(object, context);
        Check(Equal(object, expected) && object.health == 0 && object.blendWeightOrSpriteOpacity == -10,
            "Fade death must be strict and must not interrupt the remaining ticks.");
    }

    void TestDriftInitializer()
    {
        GameContext context;
        for (unsigned int value = 0; value <= 65535; ++value)
        {
            auto object = DirtyRecord();
            object.posX = Word(value);
            object.posZ = Word(value * 7);
            const auto x = Word(value * 3);
            const auto z = Word(value * 11);
            const auto frame = static_cast<std::uint8_t>(value);
            context.randomSeed = value;
            auto seed = context.randomSeed;
            auto expected = object;
            expected.health = 50;
            expected.modelId = 39;
            expected.modelAnim = 3;
            expected.animFrame = frame;
            expected.modelFrame2 = 1;
            expected.blendWeightOrSpriteOpacity = 100;
            expected.displayType = DisplayType::Type30;
            expected.behavior = BehaviorType::DriftingParticle;
            expected.collisionType = CollisionType::None;
            expected.velocityY = 1;
            const auto dx = Word(object.posX - x);
            const auto dz = Word(object.posZ - z);
            expected.velocityX = static_cast<std::int16_t>((dx > 0) - (dx < 0));
            expected.velocityZ = static_cast<std::int16_t>((dz > 0) - (dz < 0));
            expected.posX = x;
            expected.posZ = z;
            expected.winX = static_cast<std::int16_t>(Next(seed, 512));
            expected.winY = static_cast<std::int16_t>(Next(seed, 512));
            context.InitializeDriftingParticle(object, frame, x, z);
            Check(Equal(object, expected) && context.randomSeed == seed,
                "The supplied-object initializer changed extra bytes or used the wrong drift/RNG order.");
        }
        auto object = DirtyRecord();
        context.InitializeDriftingParticle(object, 3, 150, 4050);
        context.tickCount = 3;
        context.objectCollisionsEnabled = false;
        const auto expected = DriftUpdate(object, 3);
        Check(UpdateObject(object, context).status == DispatchStatus::ExecutedCase &&
            Equal(object, expected), "Initialized drifting particles must enter the normal update path.");
        object.health = 0;
        const auto before = object;
        Check(UpdateObject(object, context).status == DispatchStatus::InactiveObject && Equal(object, before),
            "Inactive particles must not execute the handler.");
    }

    void TestSmokeInitializer()
    {
        GameContext context;
        auto level = CreateDummyLevel();
        context.level = &level;
        constexpr std::array<std::int16_t, 7> radii{ 1, 5, 255, 256, 32767, -1, -32768 };
        constexpr std::array<std::int16_t, 7> factors{ 0, 1, 2, -1, 164, 32767, -32768 };
        for (unsigned int value = 0; value <= 65535; ++value)
        {
            const bool special = (value & 1) != 0;
            level.surfaceFlags[0][0] = special ? 1 : 0;
            level.heightSurfaces[0] = Level::HeightSurface{ 73, 9 };
            const auto floor = static_cast<std::int16_t>(special ? -73 : 0);
            const auto y = Word(value * 31);
            const auto radius = radii[value % radii.size()];
            const auto factor = factors[(value / radii.size()) % factors.size()];
            const auto placement = static_cast<std::uint8_t>(value);
            auto object = DirtyRecord();
            context.randomSeed = value;
            auto seed = context.randomSeed;
            const auto expected = SmokeInit(object, 150, y, 4050, radius, factor, placement, floor, seed);
            context.InitializeSmokeParticle(object, 150, y, 4050, radius, factor, placement);
            Check(Equal(object, expected) && context.randomSeed == seed &&
                context.floorSurfaceValue == (special ? 9 : -1),
                "Smoke initialization, signed-word/byte wrapping, floor correction, or retained fields differ.");
        }
        auto object = DirtyRecord();
        auto seed = context.randomSeed;
        const auto expected = SmokeInit(object, 150, 0, 4050, 0, 1, 1, -73, seed);
        context.InitializeSmokeParticle(object, 150, 0, 4050, 0, 1, 1);
        Check(Equal(object, expected) && context.randomSeed == seed,
            "Zero-radius Random must advance the seed and return zero like the native RNG.");
        context.level = nullptr;
        bool rejected = false;
        try { context.InitializeSmokeParticle(object, 150, 0, 4050, 5, 1, 1); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Missing floor geometry must not produce success-shaped smoke initialization.");
    }

    RawGameObject Emitter()
    {
        RawGameObject object;
        object.behavior = BehaviorType::SpawnInCrater;
        object.health = 1000;
        object.posX = 150;
        object.posY = -20;
        object.posZ = 4050;
        object.collisionRadiusY = 17;
        object.doorLockOrGroupBitmask = 16;
        return object;
    }

    void TestEmitter()
    {
        GameContext context;
        auto level = CreateDummyLevel();
        context.level = &level;
        constexpr std::array<std::int16_t, 8> rates{ -32768, -1, 0, 1, 16, 999, 1000, 32767 };
        for (unsigned int value = 0; value <= 65535; ++value)
        {
            auto source = Emitter();
            source.doorLockOrGroupBitmask = rates[value % rates.size()];
            context.worldObjects[10] = source;
            context.worldObjects[2] = {};
            context.tickCount = static_cast<std::int16_t>(value % 32768);
            context.randomSeed = value;
            auto seed = context.randomSeed;
            const bool spawn = source.doorLockOrGroupBitmask > static_cast<int>(Next(seed, 1000));
            const auto expected = spawn ? SmokeInit({}, source.posX,
                Word(source.posY + source.collisionRadiusY), source.posZ, 5, 1, 1, 0, seed) : RawGameObject{};
            const auto result = Function_SpawnInCrater(context.worldObjects[10], context);
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x3f8a &&
                Equal(context.worldObjects[10], source) && Equal(context.worldObjects[2], expected) &&
                context.randomSeed == seed, "Crater emission threshold, RNG, initializer arguments, or source mutation differ.");
        }
        context = GameContext{};
        context.level = &level;
        context.tickCount = 3;
        context.worldObjects[2] = Emitter();
        auto seed = context.randomSeed;
        Check(Next(seed, 1000) == 0, "The positive integration fixture must emit.");
        auto expected = SmokeInit({}, 150, -3, 4050, 5, 1, 1, 0, seed);
        expected = SmokeUpdate(expected, 3);
        const auto results = UpdateObjects(context);
        Check(results[1].behavior == BehaviorType::SmokeFading &&
            Equal(context.worldObjects[3], expected) && context.randomSeed == seed,
            "A newly emitted later-slot smoke particle must update during the same world pass.");

        context = GameContext{};
        context.level = &level;
        context.tickCount = 0;
        context.worldObjects[2] = Emitter();
        context.worldObjects[2].doorLockOrGroupBitmask = 1000;
        seed = context.randomSeed;
        Next(seed, 1000);
        expected = SmokeUpdate(SmokeInit({}, 150, -3, 4050, 5, 1, 1, 0, seed), 0);
        UpdateObjects(context);
        Check(Equal(context.worldObjects[3], expected) && context.randomSeed == seed,
            "Crater emission is per update and must still run with zero elapsed ticks.");

        context = GameContext{};
        context.level = &level;
        context.worldObjects[10] = Emitter();
        seed = context.randomSeed;
        Next(seed, 1000);
        expected = SmokeInit({}, 150, -3, 4050, 5, 1, 1, 0, seed);
        UpdateObjects(context);
        Check(Equal(context.worldObjects[2], expected) && context.randomSeed == seed,
            "A newly emitted earlier-slot particle must wait until the next world pass.");

        context = GameContext{};
        context.level = &level;
        for (auto& object : context.worldObjects) { object.health = 1000; }
        context.worldObjects[10] = Emitter();
        seed = context.randomSeed;
        Next(seed, 1000);
        expected = SmokeInit({}, 150, -3, 4050, 5, 1, 1, 0, seed);
        UpdateObjects(context);
        Check(Equal(context.worldObjects[165], expected) && context.randomSeed == seed,
            "A full pool must initialize fallback slot 165 without updating it.");

        context = GameContext{};
        context.level = &level;
        context.worldObjects[2] = Emitter();
        context.worldObjects[2].health = 1;
        bool rejected = false;
        try { UpdateObject(context.worldObjects[2], context); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected && context.worldObjects[2].behavior == BehaviorType::SmokeFading &&
            context.worldObjects[2].posX == 0 && context.worldObjects[2].posY == 0 &&
            context.worldObjects[2].posZ == 0,
            "Allocation aliasing must reread the cleared source, not silently reuse its old coordinates.");
    }

    void TestAssets(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        Check(static_cast<bool>(file), "Cannot open original level-25 object fixtures.");
        const std::vector<char> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        Check(!file.bad() && bytes.size() % sizeof(RawGameObject) == 0, "Invalid object-table fixture.");
        auto level = CreateDummyLevel();
        GameContext context;
        context.level = &level;
        unsigned int count = 0;
        for (std::size_t at = 0; at < bytes.size(); at += sizeof(RawGameObject))
        {
            RawGameObject source;
            std::memcpy(&source, bytes.data() + at, sizeof(source));
            if (source.behavior != BehaviorType::SpawnInCrater) { continue; }
            ++count;
            Check(source.collisionRadiusY == 17 &&
                (source.doorLockOrGroupBitmask == 16 || source.doorLockOrGroupBitmask == 36 ||
                    source.doorLockOrGroupBitmask == 46), "The original crater-emitter configuration changed.");
            context.worldObjects = {};
            context.worldObjects[10] = source;
            context.randomSeed = 0;
            auto seed = context.randomSeed;
            Next(seed, 1000);
            const auto expected = SmokeInit({}, source.posX, Word(source.posY + 17), source.posZ,
                5, 1, 1, 0, seed);
            Check(UpdateObject(context.worldObjects[10], context).status == DispatchStatus::ExecutedCase &&
                Equal(context.worldObjects[10], source) && Equal(context.worldObjects[2], expected) &&
                context.randomSeed == seed, "An original crater-emitter fixture failed its exact-record check.");
        }
        Check(count == 28, "Expected all 28 crater emitters in OBJPOS25.DAT.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected the original level-25 object-table path.");
        TestDriftingParticle();
        TestDriftInitializer();
        TestSmokeInitializer();
        TestEmitter();
        TestAssets(argv[1]);
        std::cout << "PASS: drifting particles, supplied-object initializers, crater emission, "
            "RNG/arithmetic/allocation edges, and 28 original emitter fixtures.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
