#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

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

    int Sign(int value) { return (value > 0) - (value < 0); }

    int Step(int current, int target, int amount)
    {
        const auto delta = Word(target - current);
        return Word(std::abs(static_cast<int>(delta))) <= amount ? delta : Word(Sign(delta) * amount);
    }

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        Check(range != 0, "Oracle received zero RNG range.");
        seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1) % 4294967296ULL);
        return (seed / 65536) % range;
    }

    int Signed(std::uint32_t& seed, unsigned int radius)
    {
        const int result = static_cast<int>(Next(seed, radius * 2 + 1)) - static_cast<int>(radius);
        return result == 0 ? static_cast<int>(radius) : result;
    }

    int HighWord(std::int64_t product)
    {
        return static_cast<int>(product >= 0 ? product / 65536 : -((-product + 65535) / 65536));
    }

    void Accumulate(std::int16_t& position, std::int16_t& fraction, int increment)
    {
        const int sum = Word(fraction + increment);
        const int carry = sum >= 0 ? sum / 256 : -((-sum + 255) / 256);
        position = Word(position + carry);
        fraction = static_cast<std::int16_t>((sum % 256 + 256) % 256);
    }

    bool Equal(const RawGameObject& first, const RawGameObject& second)
    {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    }

    RawGameObject Reference(RawGameObject object, std::uint32_t& seed, int ticks,
        bool collision, int floor, int surface, bool xOpen, bool zOpen)
    {
        const int originalPitch = object.winX;
        if (collision)
        {
            object.field_28 = static_cast<std::uint16_t>(Next(seed, 300) + 100);
            object.velocityY = Word(std::abs(static_cast<int>(object.velocityY)));
        }
        const auto chance = object.behaviorParameter;
        if (chance != 0 && Next(seed, chance) == 0) { object.field_28 = 0; }
        for (int tick = 0; tick < ticks; ++tick)
        {
            if (Word(object.field_28) > 0)
            {
                object.field_28 = static_cast<std::uint16_t>(object.field_28 - Next(seed, 2));
                if (Next(seed, 10) == 0)
                {
                    object.field_24 = static_cast<std::uint16_t>((object.field_24 + Signed(seed, 20)) & 511);
                }
                object.winX = Word((object.winX + Next(seed, object.field_28 / 16 + 1)) & 511);
                object.winY = Word((object.winY + Next(seed, object.field_28 / 32 + 1)) & 511);
                if (Next(seed, 50) == 0) { object.velocityY = Word(static_cast<int>(Next(seed, 30)) - 12); }
                const int threshold = static_cast<int>(Next(seed, 10));
                if (Word(std::abs(static_cast<int>(object.velocityY))) > threshold)
                {
                    object.posY = Word(object.posY + Sign(object.velocityY));
                }
            }
            else
            {
                object.winX = Word(object.winX + Step(object.winX, 256, 1));
                if (Next(seed, 3) == 0) { object.posY = Word(object.posY - 1); }
                if (Next(seed, 300) == 0) { object.field_28 = static_cast<std::uint16_t>(Next(seed, 100)); }
            }
        }
        if (Next(seed, 3) == 0) { object.posY = Word(object.posY - 1); }
        if (static_cast<unsigned int>(object.collisionType) != 20) { floor = Word(floor + object.collisionRadiusY); }
        object.posY = static_cast<std::int16_t>(std::max<int>(object.posY, floor));
        if (object.posY > surface)
        {
            object.posY = Word(surface);
            const int target = 256 + Signed(seed, 8);
            object.winX = Word(originalPitch + Step(originalPitch, target, Word(6 * ticks)));
        }
        const auto heading = object.field_24 % 512;
        const int speed = Word(object.field_28);
        int dx = Word(HighWord(static_cast<std::int64_t>(SineTable[heading]) * speed));
        int dz = Word(-HighWord(static_cast<std::int64_t>(SineTable[(heading + 128) % 512]) * speed));
        if (!xOpen) { object.field_24 ^= 256; dx = Word(-dx); }
        if (!zOpen) { object.field_24 ^= 256; dz = Word(-dz); }
        Accumulate(object.posX, object.velocityX, dx);
        Accumulate(object.posZ, object.velocityZ, dz);
        return object;
    }

    void TestDirection()
    {
        for (int angle = 0; angle < 65536; ++angle)
        {
            for (int speed : { -32768, -1025, -1, 0, 1, 255, 256, 1025, 32767 })
            {
                std::int16_t x = 0;
                std::int16_t z = 0;
                GetDirectionDisplacement(x, z, Word(angle), Word(speed));
                Check(x == Word(HighWord(static_cast<std::int64_t>(SineTable[angle % 512]) * speed)) &&
                    z == Word(-HighWord(static_cast<std::int64_t>(SineTable[(angle + 128) % 512]) * speed)),
                    "Direction differs from native signed high-word multiplication.");
            }
        }
    }

    void TestReference()
    {
        auto level = CreateDummyLevel();
        std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), std::uint8_t{ 1 });
        level.heightSurfaces[0] = Level::HeightSurface{ -50, 9 };
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        const std::array<std::uint16_t, 10> states = { 0, 1, 15, 16, 31, 100, 399, 32767, 32768, 65535 };
        const std::array<int, 8> velocities = { -32768, -30, -1, 0, 1, 9, 17, 32767 };
        for (unsigned int sample = 0; sample < 65536; ++sample)
        {
            context.worldObjects = {};
            auto& object = context.worldObjects[2];
            std::array<unsigned char, sizeof(object)> bytes{};
            bytes.fill(0xa5);
            std::memcpy(&object, bytes.data(), bytes.size());
            object.health = 1000;
            object.behavior = BehaviorType::FloatingDebris;
            object.displayType = DisplayType::SpriteNormal;
            object.posX = 150;
            object.posY = Word(sample % 90 - 10);
            object.posZ = 4050;
            object.field_07 = 1;
            object.collisionRadiusXZ = 2;
            object.collisionRadiusY = 3;
            object.collisionType = static_cast<CollisionType>(sample % 2 == 0 ? 20 : 117);
            object.field_28 = states[sample % states.size()];
            object.field_24 = static_cast<std::uint16_t>(sample);
            object.winX = Word(sample * 7);
            object.winY = Word(sample * 13);
            object.velocityY = Word(velocities[sample % velocities.size()]);
            object.behaviorParameter = static_cast<std::uint8_t>(sample);
            object.spriteScale = static_cast<std::uint8_t>(sample >> 8);
            context.tickCount = static_cast<std::int16_t>(sample % 16);
            context.randomSeed = sample * 65537u;
            if (sample % 3 == 1) { context.worldObjects[3] = object; }
            if (sample % 3 == 2)
            {
                context.worldObjects[1] = object;
                context.worldObjects[1].health = 0;
            }
            auto seed = context.randomSeed;
            const auto expected = Reference(object, seed, context.tickCount, sample % 3 != 0, 0, 50, true, true);
            const auto result = Function_FloatingDebris(object, context);
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x372c &&
                result.behavior == BehaviorType::FloatingDebris, "Debris result identity differs.");
            Check(Equal(object, expected) && seed == context.randomSeed && context.floorSurfaceValue == 9,
                "Full debris record, RNG order, or surface side effect differs from instruction oracle.");
        }
    }

    RawGameObject Fixture()
    {
        RawGameObject object;
        object.behavior = BehaviorType::FloatingDebris;
        object.health = 1000;
        object.posX = 150;
        object.posY = 30;
        object.posZ = 4050;
        object.field_24 = 64;
        object.field_28 = 256;
        object.velocityX = 255;
        object.velocityZ = 255;
        return object;
    }

    void TestStateWordsAndLifetime()
    {
        auto level = CreateDummyLevel();
        level.heightSurfaces[0] = Level::HeightSurface{ -50, 7 };
        std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), std::uint8_t{ 1 });
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        context.tickCount = 1;
        for (int state = 0; state < 65536; ++state)
        {
            auto object = Fixture();
            object.field_28 = static_cast<std::uint16_t>(state);
            context.randomSeed = static_cast<std::uint32_t>(state);
            auto seed = context.randomSeed;
            const auto expected = Reference(object, seed, 1, false, 0, 50, true, true);
            Function_FloatingDebris(object, context);
            Check(Equal(object, expected) && seed == context.randomSeed, "Signed drift-state boundary differs.");
        }
        auto object = Fixture();
        context.tickCount = 32767;
        context.randomSeed = 4321;
        auto seed = context.randomSeed;
        auto expected = Reference(object, seed, 32767, false, 0, 50, true, true);
        Function_FloatingDebris(object, context);
        Check(Equal(object, expected) && seed == context.randomSeed, "Maximum tick update differs.");

        object = Fixture();
        object.health = 1;
        context.tickCount = 1;
        seed = context.randomSeed;
        expected = object;
        expected.health = 0;
        expected = Reference(expected, seed, 1, false, 0, 50, true, true);
        const auto result = UpdateObject(object, context);
        Check(result.status == DispatchStatus::ExecutedCase && Equal(object, expected) && seed == context.randomSeed,
            "Debris must finish dispatch after the shared prefix expires it.");
        const auto before = object;
        const auto after = UpdateObject(object, context);
        Check(after.status == DispatchStatus::InactiveObject && Equal(object, before), "Expired debris must skip later updates.");
    }

    void TestProbesAndGates()
    {
        auto level = CreateDummyLevel();
        level.heightSurfaces[0] = Level::HeightSurface{ -50, 7 };
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        context.tickCount = 0;
        for (unsigned int mapValue = 0; mapValue < 256; ++mapValue)
        {
            std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data),
                static_cast<std::uint8_t>(mapValue));
            auto object = Fixture();
            context.randomSeed = mapValue;
            auto seed = context.randomSeed;
            const auto expected = Reference(object, seed, 0, false, 0, 50,
                (mapValue & 39) == 1, (mapValue & 39) == 1);
            Function_FloatingDebris(object, context);
            Check(Equal(object, expected) && seed == context.randomSeed, "Map mask or zero-tick update differs.");
        }
        // Distinct probe pixels, one axis blocked at a time; probing is before either position changes.
        for (bool xOpen : { false, true })
        {
            for (bool zOpen : { false, true })
            {
                auto object = Fixture();
                object.posX = 160;
                object.posZ = 4063;
                context.SetCollisionMapPixelAtWorldXZ(163, 4063, xOpen ? 1 : 0);
                context.SetCollisionMapPixelAtWorldXZ(160, 4060, zOpen ? 1 : 0);
                context.randomSeed = 0;
                auto seed = context.randomSeed;
                const auto expected = Reference(object, seed, 0, false, 0, 50, xOpen, zOpen);
                Function_FloatingDebris(object, context);
                Check(Equal(object, expected), "Independent axis reflection differs.");
            }
        }
        auto object = Fixture();
        const auto before = object;
        context.objectCollisionsEnabled = false;
        context.level = nullptr;
        context.collisionMap = nullptr;
        context.randomSeed = 1234;
        Function_FloatingDebris(object, context);
        Check(Equal(object, before) && context.randomSeed == 1234, "Disabled collision gate must skip all work.");
        context.objectCollisionsEnabled = true;
        bool failed = false;
        try { Function_FloatingDebris(object, context); }
        catch (const std::runtime_error&) { failed = true; }
        Check(failed, "Missing geometry must fail explicitly.");
        context.level = &level;
        level.heightSurfaces[0].reset();
        failed = false;
        try { Function_FloatingDebris(object, context); }
        catch (const std::runtime_error&) { failed = true; }
        Check(failed, "Missing water-surface metadata must fail explicitly.");
        level.heightSurfaces[0] = Level::HeightSurface{ -50, 7 };
        failed = false;
        try { Function_FloatingDebris(object, context); }
        catch (const std::runtime_error&) { failed = true; }
        Check(failed, "Missing map memory must fail explicitly.");
    }

    void TestTerrainAndPlayer()
    {
        auto level = CreateDummyLevel();
        level.heightSurfaces[0] = Level::HeightSurface{ 73, 9 };
        for (auto& row : level.floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 100 }); }
        for (auto& row : level.surfaceFlags) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 1 }); }
        GameContext context;
        context.level = &level;
        context.floorSurfaceValue = 123;
        Check(context.GetTerrainFloorHeight(150, 0, 4050) == -196 && context.floorSurfaceValue == 123 &&
            context.GetFloorHeight(150, 0, 4050) == -73, "Raw terrain lookup must bypass flat surface and preserve metadata.");
        context.collisionMap = level.collisionMap.get();
        context.tickCount = 0;
        auto object = Fixture();
        object.posY = -300;
        object.collisionRadiusY = 10;
        context.randomSeed = 0;
        auto seed = context.randomSeed;
        auto expected = Reference(object, seed, 0, false, -196, -73, false, false);
        Function_FloatingDebris(object, context);
        Check(Equal(object, expected), "Terrain radius lower bound differs.");
        object = Fixture();
        object.posY = -300;
        object.collisionType = static_cast<CollisionType>(20);
        object.collisionRadiusY = 10;
        seed = context.randomSeed;
        expected = Reference(object, seed, 0, false, -196, -73, false, false);
        Function_FloatingDebris(object, context);
        Check(Equal(object, expected), "Collision type 20 must omit terrain radius.");
        RawGameObject source = Fixture();
        source.field_07 = 1;
        source.collisionRadiusXZ = 5;
        source.collisionRadiusY = 7;
        context.worldObjects[1] = source;
        context.worldObjects[1].health = 0;
        Check(context.OverlapsPlayer(source), "Player overlap must ignore player health.");
        context.worldObjects[1].posX += 10;
        Check(!context.OverlapsPlayer(source), "Touching edges are not overlapping.");
        context.worldObjects[1] = source;
        context.worldObjects[1].field_07 = 2;
        Check(!context.OverlapsPlayer(source), "Disjoint collision masks must not overlap.");
    }

    void TestAssets(const std::filesystem::path& directory)
    {
        auto level = CreateDummyLevel();
        level.heightSurfaces[0] = Level::HeightSurface{ 0, 0 };
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            const auto name = entry.path().filename().string();
            if (name.rfind("OBJPOS", 0) != 0 || entry.path().extension() != ".DAT") { continue; }
            std::ifstream input(entry.path(), std::ios::binary);
            Check(static_cast<bool>(input), "Cannot open original object table.");
            RawGameObject object;
            while (input.read(reinterpret_cast<char*>(&object), sizeof(object)))
            {
                if (object.behavior != BehaviorType::FloatingDebris) { continue; }
                ++count;
                for (int ticks : { 0, 1, 7 })
                {
                    auto actual = object;
                    context.tickCount = static_cast<std::int16_t>(ticks);
                    context.randomSeed = static_cast<std::uint32_t>(count);
                    auto seed = context.randomSeed;
                    const auto expected = Reference(actual, seed, ticks, false, 0, 0, false, false);
                    Function_FloatingDebris(actual, context);
                    Check(Equal(actual, expected) && seed == context.randomSeed, "Original debris record differs.");
                }
            }
            Check(input.eof() && input.gcount() == 0, "Original object table is incomplete.");
        }
        Check(count == 24, "Expected all 24 original floating debris records.");
        std::cout << "PASS: " << count << " original debris records with explicit dummy terrain/surface/map.\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original level directory.");
        TestDirection();
        TestReference();
        TestStateWordsAndLifetime();
        TestProbesAndGates();
        TestTerrainAndPlayer();
        TestAssets(argv[1]);
        std::cout << "PASS: floating debris full-record/RNG oracle, fixed-point direction, gates, bounds and map reflections.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
