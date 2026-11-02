#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
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
        const auto bits = static_cast<unsigned int>(value) & 65535u;
        return bits >= 32768 ? static_cast<int>(bits) - 65536 : static_cast<int>(bits);
    }

    int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = seed * 134775813u + 1;
        return range == 0 ? 0 : static_cast<int>((seed >> 16) % range);
    }

    struct Context : GameContext
    {
        int creations = 0;
        void OnObjectCreated(int) override { ++creations; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        Fixture()
        {
            for (auto& row : level.floorHeights) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 0 }); }
            for (auto& row : level.floorObjects) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 0 }); }
            for (auto& row : level.ceilingObjects) { std::fill(std::begin(row), std::end(row), std::uint8_t{ 0 }); }
            level.heightSurfaces[0] = Level::HeightSurface{ 0, 0 };
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            Fill(0);
            auto& object = Object();
            object.health = 1000;
            object.posX = 1800;
            object.posZ = 2000;
            object.animFrame = 1;
            object.modelFrame2 = 2;
        }
        void Fill(std::uint8_t pixel)
        {
            std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), pixel);
        }
        RawGameObject& Object() { return context.worldObjects[2]; }
        void Run(BehaviorType behavior, std::int16_t ticks = 1)
        {
            Object().behavior = behavior;
            context.tickCount = ticks;
            Check(DispatchObjectCase(Object(), context).status == DispatchStatus::ExecutedCase,
                "Target behavior must execute.");
        }
    };

    void TestLavaSmoke()
    {
        for (auto ticks : { 0, 1, 7 })
        {
            Fixture f;
            auto& object = f.Object();
            object.posY = 32765;
            object.velocityY = 8;
            object.itemValue = 0x105;
            const auto seed = f.context.randomSeed;
            f.Run(BehaviorType::LavaSmoke, static_cast<std::int16_t>(ticks));
            Check(object.posY == -32763 && object.displayType == DisplayType::VectorUnlit &&
                f.context.randomSeed == seed && f.context.creations == 0,
                "Lava smoke rises exactly once per dispatch and restores the low-byte display mode without particles/RNG.");
            object.health = 999;
            const auto y = object.posY;
            f.Run(BehaviorType::LavaSmoke, 7);
            Check(object.posY == y && object.displayType == DisplayType::Hidden,
                "Non-1000 lava smoke must hide without movement.");
        }
    }

    void TestFrog()
    {
        for (std::uint32_t initial : { 0u, 1u, 17u, 984u, 0xffffffffu })
        {
            Fixture f;
            auto& object = f.Object();
            object.velocityX = 6;
            object.velocityZ = -2;
            object.posY = 0;
            object.blendWeightOrSpriteOpacity = 7;
            auto seed = initial;
            f.context.randomSeed = seed;
            const auto opacity = std::max(7 - Next(seed, 20) - 5, 0);
            const auto jump = Next(seed, 50);
            const auto velocity = jump == 0 && opacity == 0 ? Next(seed, 4) + 3 : 0;
            Next(seed, 250);
            f.Run(BehaviorType::Frog);
            Check(object.posX == 1800 && object.posZ == 2000 && object.posY == 0 &&
                object.velocityY == velocity && object.generalPurpose2 == 21 &&
                object.blendWeightOrSpriteOpacity == opacity && f.context.randomSeed == seed,
                "Grounded frog must preserve native RNG order, relaxation and jump state.");
        }
        Fixture airborne;
        auto& frog = airborne.Object();
        frog.posX = 4094;
        frog.posZ = 2000;
        frog.posY = 100;
        frog.velocityX = 6;
        frog.velocityZ = -3;
        frog.velocityY = 4;
        frog.generalPurpose2 = 21;
        frog.blendWeightOrSpriteOpacity = 95;
        auto seed = airborne.context.randomSeed;
        Next(seed, 250);
        airborne.Run(BehaviorType::Frog);
        Check(frog.posX == 4 && frog.posZ == 1997 && frog.posY == 104 && frog.velocityY == 3 &&
            frog.blendWeightOrSpriteOpacity == 116 && frog.generalPurpose2 == 22 &&
            airborne.context.randomSeed == seed,
            "Airborne frog must wrap X/Z at 4096 and permit one opacity overshoot.");
        frog.posX = 1800;
        airborne.Run(BehaviorType::Frog);
        Check(frog.blendWeightOrSpriteOpacity == 100, "Airborne opacity clamps on the following tick.");
        Fixture landing;
        landing.Object().posY = 1;
        landing.Object().velocityY = -3;
        landing.Object().velocityX = 6;
        landing.Object().velocityZ = 1;
        seed = landing.context.randomSeed;
        Next(seed, 300);
        Next(seed, 20);
        if (Next(seed, 50) == 0) { Next(seed, 4); }
        Next(seed, 250);
        landing.Run(BehaviorType::Frog);
        Check(landing.Object().posY == 0 && landing.context.randomSeed == seed,
            "Landing must consume its sound-distance random draw before grounded animation/jump draws.");
        Fixture water;
        water.Fill(1);
        water.Object().velocityX = 6;
        water.Object().velocityZ = -1;
        water.Run(BehaviorType::Frog);
        Check(water.Object().behavior == BehaviorType::Behavior_11, "Frog must enter the native swimming dependency.");
        water.Run(BehaviorType::Behavior_11, 3);
        Check(water.Object().posX == 1803 && water.Object().posZ == 1997 &&
            water.Object().animFrame == 3 && water.Object().modelFrame2 == 0 &&
            water.Object().behavior == BehaviorType::Behavior_11,
            "Swimming frog uses tick-scaled velocity signs and its separate frame pair.");
        water.Fill(0);
        water.Run(BehaviorType::Behavior_11);
        Check(water.Object().behavior == BehaviorType::Frog && water.Object().animFrame == 1 &&
            water.Object().modelFrame2 == 2, "Leaving water restores jumping animation.");
        water.Fill(3);
        water.Object().modelId = 40;
        water.Run(BehaviorType::Behavior_11);
        Check(water.Object().behavior == BehaviorType::None && water.Object().health >= 6 &&
            water.Object().health <= 13 && static_cast<int>(water.Object().displayType) == 70 &&
            water.context.creations == 1 && water.context.worldObjects[3].behavior == BehaviorType::SmokeFading,
            "Lava contact must finish the swimming dependency with one native smoke record.");
        Fixture zero;
        const auto original = zero.Object();
        zero.Run(BehaviorType::Frog, 0);
        auto expected = original;
        expected.behavior = BehaviorType::Frog;
        Check(std::memcmp(&expected, &zero.Object(), sizeof(expected)) == 0,
            "Zero frog ticks must preserve every payload field.");
    }

    void TestBat()
    {
        Fixture f;
        auto& object = f.Object();
        for (int phase = 0; phase < 5120; ++phase)
        {
            object = {};
            object.health = 1000;
            object.posX = 1800;
            object.posZ = 2000;
            object.posY = 120;
            object.generalPurpose2 = static_cast<std::uint16_t>(phase);
            object.itemValue = static_cast<std::int16_t>(4 + phase % 4);
            f.Run(BehaviorType::BatOutdoor, 3);
            const long double exact = phase * 0.0012271846484375000000614352319017186232486L;
            int exponent = 0;
            const auto mantissa = std::frexp(exact, &exponent);
            const auto a = std::ldexp(std::floor(std::ldexp(mantissa, 40) + 0.5L), exponent - 40);
            const auto x = static_cast<int>(std::nearbyint(object.itemValue * std::sin(a * 3) + std::cos(a * 8)));
            const auto y = static_cast<int>(std::nearbyint(object.itemValue * std::cos(a * 5)));
            const auto z = static_cast<int>(std::nearbyint(object.itemValue * std::sin(a * 7)));
            Check(object.velocityX == x && object.velocityY == y && object.velocityZ == z &&
                object.posX == 1800 + 2 * x && object.posZ == 2000 + 2 * z &&
                object.winX == -6 * y && object.blendWeightOrSpriteOpacity == 41 &&
                f.context.randomSeed == 0,
                "All 5120 bat phases must match stored-angle Real48 sine/cosine motion and once-per-dispatch blending.");
        }
        object.health = 999;
        const auto original = object;
        f.Run(BehaviorType::BatOutdoor, 4);
        Check(std::memcmp(&original, &object, sizeof(object)) == 0, "Dormant/dead outdoor bats must not simulate.");
        object.health = 1000;
        object.posY = -100;
        object.generalPurpose1 = 9;
        object.generalPurpose2 = 5118;
        f.context.field_D500 = false;
        const auto x = object.posX;
        f.Run(BehaviorType::BatOutdoor, 2);
        Check(object.posX == x && object.posY == 90 && object.generalPurpose2 == 16,
            "Body mode disables bat movement, not phase advancement or terrain height limits.");
        object.doorLockOrGroupBitmask = 1;
        object.posY = 500;
        f.context.worldObjects[1].collisionRadiusY = 20;
        f.Run(BehaviorType::BatOutdoor);
        Check(object.posY == 200 && f.context.worldObjects[1].posX == object.posX - 100 &&
            f.context.worldObjects[1].posY == 178 && f.context.worldObjects[1].posZ == object.posZ - 100,
            "Native outdoor-bat camera mode must synchronize the host-owned player eye.");
    }

    void TestRock()
    {
        Fixture f;
        for (auto timer : { -1, 0, 20, 21, 65536 })
        {
            f.context.levelElapsedTicks = timer;
            f.Run(BehaviorType::RollingRockSlope, 0);
            Check((f.Object().behavior == BehaviorType::RollingRock) == (timer > 20),
                "Slope release is a signed 32-bit >20 gate independent of behavior tick count.");
        }
        f.Fill(0x38);
        f.Run(BehaviorType::RollingRockSlope, 10);
        Check(f.Object().behavior == BehaviorType::RollingRockSlope, "Blocking map pixels must retain the delayed slope.");
        f.Fill(0);
        auto& object = f.Object();
        object.posY = 100;
        object.velocityY = -3;
        object.itemValue = 300;
        object.generalPurpose1 = static_cast<std::uint16_t>(-257);
        object.velocityX = 250;
        object.velocityZ = 1;
        f.Run(BehaviorType::RollingRock);
        Check(object.posX == 1802 && object.velocityX == 38 && object.posZ == 1999 && object.velocityZ == 0 &&
            object.posY == 99 && object.velocityY == -4 && object.winX == 510 && object.winZ == 3,
            "Rolling rock must use signed 8.8 carry, SAR rotation and truncating vertical division.");
        object.posY = -1;
        object.velocityY = -11;
        object.collisionRadiusY = 16;
        f.Run(BehaviorType::RollingRock);
        Check(object.posY == 16 && object.velocityY == 6, "Rolling rock must rebound from terrain plus its Y radius.");
        f.Fill(8);
        f.Run(BehaviorType::RollingRock);
        Check(object.health == 2000 && object.collisionMask == 0 && object.collisionType == CollisionType::None,
            "Rock contact with a wall enters native settling state.");
        object.posY = 0;
        object.renderPlacementMode = 3;
        f.Run(BehaviorType::RollingRock);
        Check(object.behavior == BehaviorType::Pillar && object.posY == 16 && object.velocityY == 0 &&
            object.renderPlacementMode == 0, "Settled rock must enter the recovered pillar collision-map path.");
        f.Run(BehaviorType::Pillar);
        Check(f.context.GetCollisionMapPixelAtWorldXZ(object.posX, object.posZ) == 0x38,
            "Settled rock must become a blocking map pillar.");
    }

    void TestShark()
    {
        for (const auto water : { false, true })
        {
            for (std::uint32_t initial : { 0u, 1u, 17u, 733u, 0xffffffffu })
            {
                Fixture f;
                f.Fill(water ? 1 : 0);
                auto& object = f.Object();
                object.winY = 128;
                object.itemValue = 256;
                object.generalPurpose1 = 256;
                object.velocityX = 255;
                object.velocityZ = 255;
                object.blendWeightOrSpriteOpacity = 99;
                object.modelAnim = 1;
                auto seed = initial;
                f.context.randomSeed = seed;
                int turn = 0;
                if (Next(seed, 30) == 0) { turn = Next(seed, 5) - 2; }
                if (Next(seed, 15) == 0) { turn = 0; }
                int target = 256;
                if (Next(seed, 100) == 0) { target = Next(seed, 600) + 60; }
                const int speed = 256 + std::clamp(target - 256, -3, 3);
                const int yaw = (128 + turn) & 511;
                std::int16_t dx;
                std::int16_t dz;
                GetClampedDirectionDisplacement(dx, dz, static_cast<std::int16_t>(yaw), static_cast<std::int16_t>(speed));
                Next(seed, 30);
                Next(seed, 30);
                if (!water) { turn = Next(seed, 3) + 2; }
                Next(seed, 15);
                f.Run(BehaviorType::Shark);
                Check(object.velocityY == turn && object.itemValue == speed && object.winY == yaw &&
                    object.collisionMask == 0 && object.animFrame == 2 && object.modelFrame2 == 3 &&
                    f.context.randomSeed == seed && object.posY == 0,
                    "Shark wandering/probing/animation must preserve native RNG and frame ordering.");
                Check(object.posX == 1800 + (water ? Word(255 + dx) >> 8 : 0) &&
                    object.posZ == 2000 + (water ? Word(255 + dz) >> 8 : 0),
                    "Shark movement must consume 8.8 fractions only when both water probes pass.");
            }
        }
        Fixture dead;
        auto& object = dead.Object();
        object.health = 2000;
        object.itemValue = 25;
        object.posY = -11;
        object.collisionMask = 3;
        object.collisionType = CollisionType::Shark;
        dead.Run(BehaviorType::Shark);
        Check(object.itemValue == 15 && object.velocityY == -1 && object.health == 2000 &&
            object.behavior == BehaviorType::None && object.collisionMask == 0 &&
            object.collisionType == CollisionType::None, "Dead shark must sink and become an inert corpse, not be deleted.");
    }

    void TestTimerAndHeightBoundaries()
    {
        Fixture f;
        f.level.floorHeights[27][49] = 10;
        f.level.floorHeights[28][0] = 20;
        f.level.floorHeights[28][49] = 30;
        f.level.floorHeights[29][0] = 40;
        const auto interpolate = [](int a, int b, int fraction)
        {
            return (a * (65535 - fraction) + b * fraction) >> 16;
        };
        const auto height = interpolate(interpolate(30, 40, 97 * 661),
            interpolate(10, 20, 97 * 661), 59 * 661);
        Check(f.context.GetTerrainFloorHeight(97, 0, 1259) == -2 * height,
            "Authored X<100 frog must use native row-linear corner aliases, not clamp or reject its coordinates.");
        struct Host final : Context
        {
            void LoadLevel(const LevelLoadRequest&) override {}
        } host;
        struct Input final : InputHandler
        {
            bool IsKeyPressed(InputKey) override { return false; }
        } input;
        const auto request = host.StartLevel(17);
        LevelLoadData data;
        data.level = std::make_unique<Level>(std::move(f.level));
        data.objects[1].health = 1000;
        data.objects[1].posX = 1800;
        data.objects[1].posZ = 2000;
        data.objects[2] = f.Object();
        data.objects[2].behavior = BehaviorType::RollingRockSlope;
        host.levelElapsedTicks = 99;
        host.CompleteLevelLoad(request.id, std::move(data));
        Check(host.levelElapsedTicks == 0, "Level installation must reset the native elapsed timer.");
        host.basicPlayerMovementEnabled = false;
        host.SetMenuPage(MenuPage::Gameplay);
        Check(host.Tick({ 0, 20 }, input).status == GameTickStatus::Advanced &&
            host.levelElapsedTicks == 20 && host.worldObjects[2].behavior == BehaviorType::RollingRockSlope,
            "Timer increments after active objects, not before delayed-rock dispatch.");
        host.Tick({ 0, 1 }, input);
        Check(host.levelElapsedTicks == 21 && host.worldObjects[2].behavior == BehaviorType::RollingRockSlope,
            "Exactly twenty elapsed ticks still retains the delayed rock.");
        host.Tick({ 0, 0 }, input);
        Check(host.worldObjects[2].behavior == BehaviorType::RollingRock,
            "The following dispatch releases the rock, including zero behavior/frame ticks.");
        host.SetPaused(true);
        host.Tick({ 0, 10 }, input);
        Check(host.levelElapsedTicks == 21, "Paused gameplay must not advance elapsed level ticks.");
        host.SetPaused(false);
        host.levelElapsedTicks = std::numeric_limits<std::int32_t>::max();
        host.Tick({ 0, 1 }, input);
        Check(host.levelElapsedTicks == std::numeric_limits<std::int32_t>::min(),
            "Native elapsed ticks must wrap as a signed 32-bit counter without overflow UB.");
        auto& destination = host.worldObjects[3];
        destination.health = 1000;
        destination.collisionType = CollisionType::Teleporter;
        destination.itemValue = 7;
        destination.posX = 1900;
        destination.posZ = 2000;
        destination.velocityY = 1;
        host.TeleportPlayer(7);
        Check(host.levelElapsedTicks == 0, "Native teleportation restarts the slope delay.");
    }

    void TestHeightPlaneBoundaries()
    {
        Fixture f;
        std::array<std::uint8_t, 65536> dosMemory{};
        for (std::size_t row = 0; row < Level::Height; ++row)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                f.level.ceilingObjects[row][column] = 1;
                f.level.floorHeights[row][column] = static_cast<std::uint8_t>(10 + (row * 7 + column * 13) % 80);
                f.level.roomHeights[row][column] = static_cast<std::uint8_t>(100 + (row * 11 + column * 3) % 90);
                f.level.surfaceFlags[row][column] = 0;
                f.level.collisionMaterials[row][column] = 0;
            }
        }
        // The surface query at X<100, Z>=4000 aliases this byte; keep it an ordinary floor.
        f.level.roomHeights[39][49] = 0;
        std::memcpy(dosMemory.data() + 0x32d0, f.level.wallTextures, sizeof(f.level.wallTextures));
        std::memcpy(dosMemory.data() + 0x3aa0, f.level.floorObjects, sizeof(f.level.floorObjects));
        std::memcpy(dosMemory.data() + 0x4270, f.level.ceilingObjects, sizeof(f.level.ceilingObjects));
        std::memcpy(dosMemory.data() + 0x4a40, f.level.floorHeights, sizeof(f.level.floorHeights));
        std::memcpy(dosMemory.data() + 0x5210, f.level.roomHeights, sizeof(f.level.roomHeights));
        std::memcpy(dosMemory.data() + 0x59e0, f.level.surfaceFlags, sizeof(f.level.surfaceFlags));
        std::memcpy(dosMemory.data() + 0x61b0, f.level.collisionMaterials, sizeof(f.level.collisionMaterials));
        const auto interpolate = [](int a, int b, int fraction)
        {
            return (a * (65535 - fraction) + b * fraction) >> 16;
        };
        const auto check = [&](std::int16_t x, std::int16_t z)
        {
            // Use the original DS operands and one-based Pascal row calculation independently of the host layout.
            const auto displacement = (41 - z / 100) * 50 + x / 100;
            const auto height = [&](int operand)
            {
                const auto address = operand + displacement;
                return interpolate(
                    interpolate(dosMemory[address + 50], dosMemory[address + 51], x % 100 * 661),
                    interpolate(dosMemory[address], dosMemory[address + 1], x % 100 * 661),
                    z % 100 * 661);
            };
            const auto floor = -2 * height(0x4a0d);
            Check(f.context.GetTerrainFloorHeight(x, 0, z) == floor &&
                f.context.GetFloorHeight(x, 0, z) == floor,
                "Boundary floor samples must match native DS reads, including neighboring geometry planes.");
            const bool ceiling = dosMemory[0x423d + displacement] != 0;
            Check(f.context.GetRoomHeight(x, z) == (ceiling ? 2 * height(0x51dd) : 20000),
                "Boundary ceiling and room samples must match their distinct native neighboring planes.");
            f.context.worldObjects[1].posX = x;
            f.context.worldObjects[1].posZ = z;
            Check(f.context.HasCeilingAtPlayer() == ceiling,
                "Player ceiling tests must use the same boundary addressing as room-height queries.");
        };
        for (std::int16_t coordinate = 0; coordinate < 4096; ++coordinate)
        {
            for (const auto edge : std::array<std::int16_t, 6>{ 0, 99, 100, 199, 200, 4095 })
            {
                check(coordinate, edge);
                check(edge, coordinate);
            }
        }
        bool rejected = false;
        try { f.context.GetTerrainFloorHeight(100, 0, -12000); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "Unmodeled DOS memory must still fail explicitly rather than wrap or clamp.");
    }

    void TestOriginalFixtures(const std::filesystem::path& root)
    {
        struct Host final : Context
        {
            std::filesystem::path directory;
            using GameContext::LoadLevelGeometry;
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                std::ifstream file(directory / std::string(filename.substr(6)), std::ios::binary);
                Check(static_cast<bool>(file), "Cannot load original forest/cave fixture geometry.");
                return { std::istreambuf_iterator<char>(file), {} };
            }
        } host;
        host.directory = root;
        int counts[5]{};
        for (int number : { 16, 17 })
        {
            LevelLoadData data;
            host.LoadLevelGeometry(static_cast<std::int16_t>(number), data);
            host.level = data.level.get();
            host.collisionMap = data.level->collisionMap.get();
            host.floorRepeatHeight = data.floorRepeatHeight;
            std::ifstream file(root / ("OBJPOS" + std::to_string(number) + ".DAT"), std::ios::binary);
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
            Check(!bytes.empty() && bytes.size() % 54 == 0, "Original object records must be complete.");
            for (std::size_t offset = 0; offset < bytes.size(); offset += 54)
            {
                const auto original = DecodeGameObject(bytes.data() + offset, 54);
                const auto behavior = original.behavior;
                const int index = behavior == BehaviorType::Frog ? 0 : behavior == BehaviorType::BatOutdoor ? 1 :
                    behavior == BehaviorType::LavaSmoke ? 2 : behavior == BehaviorType::RollingRockSlope ? 3 :
                    behavior == BehaviorType::Shark ? 4 : -1;
                if (index < 0 || original.health <= 0) { continue; }
                ++counts[index];
                host.worldObjects = {};
                host.ReplaceWorldObject(2, original);
                host.tickCount = 1;
                host.field_D500 = true;
                host.levelElapsedTicks = 21;
                host.soundListenerX = WrapSignedWord(original.posX + 300);
                host.soundListenerZ = original.posZ;
                for (int step = 0; step < 10000; ++step)
                {
                    auto& object = host.worldObjects[2];
                    if (object.health <= 0 || object.behavior == BehaviorType::None) { break; }
                    try
                    {
                        Check(UpdateObject(object, host).status == DispatchStatus::ExecutedCase,
                            "Authored forest/cave records must run through every needed dependency on their original terrain.");
                    }
                    catch (const std::exception& error)
                    {
                        throw std::runtime_error("Level " + std::to_string(number) + ", original slot " +
                            std::to_string(offset / 54 + 1) + ", update " + std::to_string(step) +
                            ", X=" + std::to_string(object.posX) + ", Z=" + std::to_string(object.posZ) +
                            ": " + error.what());
                    }
                }
            }
        }
        Check(counts[0] == 10 && counts[1] == 7 && counts[2] == 23 && counts[3] == 5 && counts[4] == 7,
            "Original forest/cave behavior fixture counts changed.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original LEVEL fixture directory.");
        TestLavaSmoke();
        std::cout << "Lava smoke passed.\n";
        TestFrog();
        std::cout << "Frog passed.\n";
        TestBat();
        std::cout << "Outdoor bat passed.\n";
        TestRock();
        std::cout << "Rolling rock passed.\n";
        TestShark();
        std::cout << "Shark passed.\n";
        TestTimerAndHeightBoundaries();
        TestHeightPlaneBoundaries();
        TestOriginalFixtures(argv[1]);
        std::cout << "Level 16/17 frog, outdoor bat, lava smoke, slope rock and shark passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
