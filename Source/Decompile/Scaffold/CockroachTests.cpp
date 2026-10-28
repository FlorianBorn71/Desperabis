#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
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

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * 134775813u + 1);
        return range == 0 ? 0 : (seed >> 16) % range;
    }

    int Signed(std::uint32_t& seed, int radius)
    {
        const int value = static_cast<int>(Next(seed, 2 * radius + 1)) - radius;
        return value == 0 ? radius : value;
    }

    int Step(int current, int target, int speed)
    {
        const int delta = Word(target - current);
        return Word(std::abs(delta)) <= speed ? delta : ((delta > 0) - (delta < 0)) * speed;
    }

    void Direction(RawGameObject& object)
    {
        const auto high = [](std::int64_t value)
        {
            return value >= 0 ? value / 65536 : -((-value + 65535) / 65536);
        };
        object.velocityX = Word(high(3LL * ClampedSineTable[static_cast<std::uint16_t>(object.winY) % 512]));
        object.velocityZ = Word(-high(3LL *
            ClampedSineTable[(static_cast<std::uint16_t>(object.winY) + 128) % 512]));
    }

    RawGameObject Fixture(int phase)
    {
        RawGameObject object;
        object.behavior = BehaviorType::Cockroach;
        object.collisionType = CollisionType::Cockroach;
        object.health = 1000;
        object.posX = 1500;
        object.posZ = 2000;
        object.posY = 17;
        object.velocityX = 1;
        object.velocityZ = -2;
        object.velocityY = -1;
        object.itemValue = Word(phase);
        object.groupIndexOrLock = 40;
        object.generalPurpose1 = 15;
        object.collisionMask = 3;
        object.displayType = DisplayType::VectorNormal;
        object.winZ = 150;
        object.blendWeightOrSpriteOpacity = -100;
        return object;
    }

    RawGameObject Reference(RawGameObject object, std::uint32_t& seed, int ticks)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            if (object.health != 1000) { continue; }
            object.soundEffectId = 3;
            object.posX = Word(object.posX + object.velocityX);
            object.posZ = Word(object.posZ + object.velocityZ);
            object.posY = Word(object.posY + object.velocityY);
            switch (object.itemValue)
            {
            case -1:
                if (std::abs(object.posX - 1500) + std::abs(object.posZ - 2000) < 280)
                {
                    object.itemValue = 15;
                }
                break;
            case 0:
                if (std::abs(object.posX - 1500) + std::abs(object.posZ - 2000) < 150)
                {
                    object.itemValue = 1;
                }
                break;
            case 1:
                if (Next(seed, 30) == 0) { object.itemValue = 2; }
                break;
            case 2:
                object.animFrame = 3;
                object.modelFrame2 = 4;
                object.blendWeightOrSpriteOpacity = -100;
                if (object.doorLockOrGroupBitmask == 0) { object.doorLockOrGroupBitmask = 1; }
                if (object.velocityY > -5) { --object.velocityY; }
                if (object.posY < 0) { object.posY = 0; object.velocityY = 0; object.itemValue = 3; }
                object.winZ = Word(object.winZ + Step(object.winZ, 150, 5));
                break;
            case 3:
                object.collisionType = CollisionType::Cockroach;
                if (object.generalPurpose2 == 0 && Next(seed, 20) == 0 && object.blendWeightOrSpriteOpacity == -100)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(Signed(seed, 15));
                }
                else
                {
                    const int value = Word(object.generalPurpose2);
                    object.generalPurpose2 = static_cast<std::uint16_t>(value - ((value > 0) - (value < 0)));
                }
                object.winY = Word(object.winY + object.generalPurpose2);
                if (Next(seed, 40) == 0) { object.itemValue = 4; }
                if (Next(seed, 15) == 0) { object.blendWeightOrSpriteOpacity = -100; }
                if (Next(seed, 15) == 0) { object.blendWeightOrSpriteOpacity = Word(Next(seed, 100)); }
                break;
            case 4:
                object.blendWeightOrSpriteOpacity = -100;
                object.animFrame = 1;
                object.modelFrame2 = 2;
                object.winZ = Word(object.winZ + Step(object.winZ, 0, 9));
                if (object.winZ == 0) { object.itemValue = 5; }
                break;
            case 5:
            case 15:
                if (Next(seed, 20) == 0)
                {
                    object.blendWeightOrSpriteOpacity = 0;
                    object.velocityX = 0;
                    object.velocityZ = 0;
                }
                object.posY = 0;
                object.winZ = 0;
                if (Next(seed, 15) == 0)
                {
                    object.winY = Word((object.winY + Signed(seed, 20)) & 511);
                    Direction(object);
                    object.blendWeightOrSpriteOpacity = -100;
                }
                if (Word(object.generalPurpose1) > 0 && Next(seed, object.generalPurpose1) == 0)
                {
                    const int target = GetVectorAngle(Word(1500 - object.posX), Word(2000 - object.posZ));
                    int current = (static_cast<std::uint16_t>(object.winY) % 512) + 512;
                    while (std::abs(current - target) > 256) { current -= 512; }
                    object.winY = Word(current + Step(current, target, 14));
                    Direction(object);
                }
                Signed(seed, 1);
                if (object.velocityX != 0 || object.velocityZ != 0) { object.winX = 0; }
                break;
            case 7:
                if (object.collisionMask < 131) { object.collisionMask = 0; }
                object.velocityX = 0;
                object.velocityZ = 0;
                object.blendWeightOrSpriteOpacity = 0;
                object.itemValue = 8;
                object.animFrame = 1;
                object.modelFrame2 = 2;
                object.modelAnim = 2;
                break;
            case 8:
                if (object.blendWeightOrSpriteOpacity < 100 && Next(seed, 7) == 0)
                {
                    ++object.blendWeightOrSpriteOpacity;
                }
                break;
            case 10:
                if (Next(seed, 900) == 0)
                {
                    object.displayType = DisplayType::VectorNormal;
                    object.winY = Word(Next(seed, 512));
                    object.itemValue = 11;
                    object.collisionMask = 3;
                }
                break;
            case 11:
                if (object.posY < 0) { ++object.posY; }
                else { object.itemValue = 15; }
                break;
            case 16:
                object.collisionType = CollisionType::None;
                object.collisionMask = 0;
                object.animFrame = 4;
                object.modelFrame2 = 3;
                object.velocityX = 0;
                object.velocityY = 0;
                object.velocityZ = 0;
                object.winZ = Word(object.winZ + Step(object.winZ, 256, 17));
                if (object.blendWeightOrSpriteOpacity < 0) { ++object.blendWeightOrSpriteOpacity; }
                else if (object.winZ == 256) { object.behavior = BehaviorType::None; }
                break;
            default:
                break;
            }
        }
        return object;
    }

    void TestOracle()
    {
        auto level = CreateDummyLevel();
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        context.soundListenerX = 1500;
        context.soundListenerZ = 2000;
        for (int sample = 0; sample < 32768; ++sample)
        {
            auto object = Fixture(sample % 22 - 2);
            object.winY = Word(sample * 13);
            object.generalPurpose2 = static_cast<std::uint16_t>(sample % 31 - 15);
            object.generalPurpose1 = static_cast<std::uint16_t>(sample % 7 == 0 ? 0x8000 : sample % 17);
            object.winZ = Word(sample * 79);
            object.collisionMask = static_cast<std::uint8_t>(sample);
            object.blendWeightOrSpriteOpacity = Word(sample % 401 - 200);
            context.tickCount = static_cast<std::int16_t>(sample % 9);
            context.objectCollisionsEnabled = (sample & 1) != 0;
            context.randomSeed = static_cast<std::uint32_t>(sample);
            auto seed = context.randomSeed;
            const auto expected = Reference(object, seed, context.tickCount);
            const auto result = Function_Cockroach(object, context);
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1c17 &&
                std::memcmp(&object, &expected, sizeof(object)) == 0 && context.randomSeed == seed,
                "Cockroach phases/animation/steering/native RNG order differ from oracle.");
        }
        context.level = nullptr;
        context.collisionMap = nullptr;
        for (int bits = 0; bits < 65536; ++bits)
        {
            auto object = Fixture(99);
            object.health = Word(bits);
            auto expected = object;
            if (object.health == 1000)
            {
                expected.soundEffectId = 3;
                ++expected.posX;
                expected.posZ -= 2;
                --expected.posY;
            }
            context.tickCount = 1;
            const auto seed = context.randomSeed;
            Function_Cockroach(object, context);
            Check(std::memcmp(&object, &expected, sizeof(object)) == 0 && context.randomSeed == seed,
                "Only health exactly 1000 may execute a cockroach substep.");
        }
        auto object = Fixture(16);
        context.tickCount = 32767;
        auto seed = context.randomSeed;
        const auto expected = Reference(object, seed, context.tickCount);
        Function_Cockroach(object, context);
        Check(std::memcmp(&object, &expected, sizeof(object)) == 0 && context.randomSeed == seed &&
            object.behavior == BehaviorType::None, "Death completion must finish all batched native substeps.");
    }

    void TestDamageAndWalls()
    {
        auto level = CreateDummyLevel();
        GameContext context;
        context.tickCount = 1;
        context.soundEnabled = false;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        for (const int phase : { 5, 15 })
        {
            auto& object = context.worldObjects[2];
            object = Fixture(phase);
            object.groupIndexOrLock = 0;
            Function_Cockroach(object, context);
            Check(object.itemValue == (phase == 5 ? 7 : 16) &&
                object.collisionType == (phase == 5 ? CollisionType::None : CollisionType::Cockroach),
                "Walking depleted cockroaches must enter their native death phase.");
            object = Fixture(phase);
            object.displayType = DisplayType::Hidden;
            object.groupIndexOrLock = 0;
            Function_Cockroach(object, context);
            Check(object.health == 0, "Hidden depleted cockroach must be removed through DamageObject.");
        }
        std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), std::uint8_t{ 0 });
        for (int row = 0; row < 200; ++row) { level.collisionMap->data[row * 200 + 2] = 0x38; }
        context.discardBlockedObjects = true;
        auto& object = context.worldObjects[2];
        object = Fixture(5);
        object.posX = 150;
        object.posZ = 4050;
        object.velocityX = 0;
        object.velocityZ = 0;
        Function_Cockroach(object, context);
        Check(object.health == 0, "Sixteen failed obstacle probes must invoke the blocked-object policy.");
    }

    void TestDistanceAndSlope()
    {
        GameContext context;
        context.tickCount = 1;
        context.soundListenerX = 1500;
        context.soundListenerZ = 2000;
        for (const int phase : { -1, 0 })
        {
            const int threshold = phase == -1 ? 280 : 150;
            for (const int offset : { threshold - 1, threshold })
            {
                auto object = Fixture(phase);
                object.posX = Word(1500 + offset);
                object.velocityX = 0;
                object.velocityY = 0;
                object.velocityZ = 0;
                Function_Cockroach(object, context);
                Check(object.itemValue == (offset < threshold ? (phase == -1 ? 15 : 1) : phase),
                    "Cockroach wake-up must use the strict native 280/150 distance threshold.");
            }
        }
        auto level = CreateDummyLevel();
        for (auto& row : level.floorHeights)
        {
            for (std::size_t column = 0; column < Level::Width; ++column)
            {
                row[column] = static_cast<std::uint8_t>(column);
            }
        }
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        int moving = 0;
        for (unsigned int seed = 0; seed < 256; ++seed)
        {
            auto object = Fixture(5);
            object.velocityX = 3;
            object.velocityZ = 0;
            object.velocityY = 0;
            context.randomSeed = seed;
            Function_Cockroach(object, context);
            if (object.velocityX == 0 && object.velocityZ == 0) { continue; }
            ++moving;
            const auto floor = context.GetTerrainFloorHeight(Word(object.posX + object.velocityX * 8),
                object.posY, Word(object.posZ + object.velocityZ * 8));
            Check(object.winX == GetVectorAngle(Word(object.posY - floor), -24),
                "Cockroach terrain pitch must sample eight velocity steps ahead against speed -24.");
        }
        Check(moving > 200, "Slope fixture did not exercise enough moving cockroaches.");
    }

    class Input final : public InputHandler
    {
    public:
        bool forward = false;
        bool backward = false;
        bool IsKeyPressed(InputKey key) override
        {
            return (key == InputKey::ForwardKey && forward) || (key == InputKey::BackwardKey && backward);
        }
    };

    void TestPlayerContact()
    {
        GameContext context;
        Input input;
        auto& player = context.worldObjects[1];
        player = Fixture(0);
        player.collisionRadiusXZ = 15;
        player.collisionRadiusY = 23;
        for (int factor = 0; factor < 256; ++factor)
        {
            for (const bool enabled : { false, true })
            {
                for (const int suppressed : { 0, 1 })
                {
                    auto& object = context.worldObjects[2];
                    object = Fixture(5);
                    object.collisionRadiusXZ = 20;
                    object.collisionRadiusY = 20;
                    object.doorLockOrGroupBitmask = 1;
                    object.behaviorParameter = static_cast<std::uint8_t>(factor);
                    context.behaviorDispatchEnabled = enabled;
                    context.playerLifeDeltaSuppressed = static_cast<std::int16_t>(suppressed);
                    context.playerLife = 360 * 65536;
                    context.playerHitEffectTicks = 0;
                    const auto before = object;
                    const auto result = UpdatePlayerInteraction(context, input);
                    const bool applied = enabled && factor > 0;
                    Check(result.status == (applied ? InteractionStatus::Applied : InteractionStatus::NoEffect) &&
                        context.playerLife == 360 * 65536 - (applied && suppressed == 0 ? 29491 * factor : 0) &&
                        context.playerHitEffectTicks == (applied ? 8 : 0) &&
                        object.velocityX == (applied ? 0 : before.velocityX) &&
                        object.velocityZ == (applied ? 0 : before.velocityZ) && object.velocityY == before.velocityY &&
                        player.health == 1000, "Cockroach contact gates/factor/stopping or player-life storage differ.");
                }
            }
        }
        context.playerLifeDeltaSuppressed = 0;
        context.playerLife = 10000;
        context.worldObjects[2].behaviorParameter = 1;
        UpdatePlayerInteraction(context, input);
        Check(context.playerLife == 0 && context.playerPostureDelta == -1 &&
            context.positionalSoundsSuppressed && context.playerHitEffectTicks == 8,
            "Cockroach depletion must use the native player death path and then set contact hit ticks.");
        class Host final : public GameContext
        {
        protected:
            void LoadLevel(const LevelLoadRequest&) override {}
        };
        for (const bool backward : { false, true })
        {
            for (const int threshold : { 2, 3 })
            {
                Host host;
                host.soundEnabled = false;
                const auto request = host.StartLevel(11);
                LevelLoadData data;
                data.level = std::make_unique<Level>(CreateDummyLevel());
                data.playerSpawn = PlayerCamera{ {1500,45,2000}, {0,0,100} };
                data.objects[2] = Fixture(5);
                data.objects[2].collisionRadiusXZ = 100;
                data.objects[2].collisionRadiusY = 100;
                data.objects[2].doorLockOrGroupBitmask = static_cast<std::int16_t>(threshold);
                data.objects[2].behaviorParameter = 2;
                host.CompleteLevelLoad(request.id, std::move(data));
                input.forward = !backward;
                input.backward = backward;
                const auto result = host.Tick({ 0,1 }, input);
                Check(result.status == GameTickStatus::Advanced && host.playerCollisionSlot == 2 &&
                    host.worldObjects[2].groupIndexOrLock == (threshold == 2 ? 39 : 40) &&
                    host.playerLife == 360 * 65536 - (threshold == 3 ? 2 * 29491 : 0),
                    "Absolute forward speed must damage the cockroach strictly above its threshold, otherwise bite.");
            }
        }
    }

    void TestAssets(const std::filesystem::path& directory)
    {
        auto level = CreateDummyLevel();
        GameContext context;
        context.tickCount = 1;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        int count = 0;
        for (const auto& file : std::filesystem::directory_iterator(directory))
        {
            const auto name = file.path().filename().string();
            if (name.rfind("OBJPOS", 0) != 0 || file.path().extension() != ".DAT") { continue; }
            std::ifstream input(file.path(), std::ios::binary);
            Check(static_cast<bool>(input), "Cannot read original level objects.");
            RawGameObject object;
            while (input.read(reinterpret_cast<char*>(&object), sizeof(object)))
            {
                if (object.behavior != BehaviorType::Cockroach) { continue; }
                ++count;
                const auto health = object.health;
                for (int step = 0; step < 20; ++step)
                {
                    const auto result = UpdateObject(object, context);
                    Check(result.status == DispatchStatus::ExecutedCase && object.health == health,
                        "Original cockroach fixture must update without guessed health normalization.");
                }
            }
            Check(input.eof(), "Original object table read failed.");
        }
        Check(count == 70, "Unexpected number of original cockroach/brown-spider records.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "Expected original LEVEL directory and executable.");
        std::ifstream executable(argv[2], std::ios::binary);
        executable.seekg(0x319f4);
        char speed[2]{};
        executable.read(speed, sizeof(speed));
        Check(static_cast<bool>(executable) && speed[0] == 3 && speed[1] == 0,
            "The original DS:02D4 cockroach speed word must be 3.");
        TestOracle();
        TestDamageAndWalls();
        TestDistanceAndSlope();
        TestPlayerContact();
        TestAssets(argv[1]);
        std::cout << "Cockroach tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
