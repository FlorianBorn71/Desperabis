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

    int Signed(std::uint32_t& seed, unsigned int radius)
    {
        const int result = static_cast<int>(Next(seed, (2 * radius + 1) & 65535)) - static_cast<int>(radius);
        return result == 0 ? static_cast<int>(radius) : result;
    }

    int Step(int current, int target, int speed)
    {
        const int delta = Word(target - current);
        return Word(std::abs(delta)) <= speed ? delta : ((delta > 0) - (delta < 0)) * speed;
    }

    RawGameObject Bat()
    {
        RawGameObject object;
        object.behavior = BehaviorType::BatIndoor;
        object.health = 1000;
        object.posX = 1500;
        object.posY = 50;
        object.posZ = 2000;
        object.itemValue = 4;
        object.generalPurpose1 = 47;
        object.generalPurpose3 = 0xcafe;
        object.groupIndexOrLock = 20;
        object.soundEffectId = 1;
        object.modelFrame2 = 2;
        object.animFrame = 1;
        object.collisionMask = 3;
        object.collisionRadiusXZ = 20;
        object.collisionRadiusY = 20;
        object.displayType = DisplayType::VectorNormal;
        return object;
    }

    RawGameObject Reference(RawGameObject object, std::uint32_t& seed, int ticks)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            const int distance = Word(Word(std::abs(Word(2000 - object.posX))) +
                Word(std::abs(Word(2000 - object.posZ))));
            if (object.health == 1000)
            {
                object.collisionType = CollisionType::BatIndoor;
                if (object.displayType == DisplayType::Hidden) { object.displayType = DisplayType::VectorNormal; }
                Next(seed, static_cast<std::uint16_t>(Word(2 * (distance + 1))));
                object.winX = Word(object.winX + Step(object.winX, 0, 23));
                object.posX = Word(object.posX + object.velocityX);
                object.posZ = Word(object.posZ + object.velocityZ);
                const int fraction = Word(object.doorLockOrGroupBitmask + object.velocityY);
                const int high = static_cast<unsigned int>(static_cast<std::uint16_t>(fraction)) >> 8;
                object.posY = Word(object.posY + (high >= 128 ? high - 256 : high));
                object.doorLockOrGroupBitmask = static_cast<std::int16_t>(fraction & 255);
                if (Next(seed, 20) == 0)
                {
                    if (object.behaviorParameter == 0 || Next(seed, 3) > 0)
                    {
                        object.velocityY = Word(Signed(seed, static_cast<std::uint16_t>(Word(object.itemValue * 100))) +
                            object.itemValue * 15);
                    }
                    else { object.velocityY = Word(Step(Word(object.posY * 8), 45 * 8 + 45 * 4, 600)); }
                }
                if (Next(seed, 10) == 1)
                {
                    object.generalPurpose2 = static_cast<std::uint16_t>(
                        Word(object.generalPurpose2 + Signed(seed, 10)) & 511);
                }
                Next(seed, 2);
                const auto highWord = [](std::int64_t product)
                {
                    return product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
                };
                const auto heading = object.generalPurpose2 & 511u;
                object.velocityX = Word(highWord(ClampedSineTable[heading] * static_cast<std::int64_t>(object.itemValue)));
                object.velocityZ = Word(-highWord(
                    ClampedSineTable[(heading + 128) & 511u] * static_cast<std::int64_t>(object.itemValue)));
                if (object.posY < 15) { object.velocityY = Word((15 - object.posY) * 150); }
                if (tick == 0)
                {
                    object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(
                        Word(object.blendWeightOrSpriteOpacity + object.generalPurpose1) % 200);
                }
                if (object.posY > 10000) { object.posY = 10000; object.velocityY = 0; }
                // Two truncating interpolation stages turn room height 50 into 96 world units.
                if (object.posY > 76)
                {
                    if (distance > 350)
                    {
                        object.posY = 96;
                        object.blendWeightOrSpriteOpacity = 0;
                        object.health = 3000;
                    }
                    else { object.velocityY = -400; }
                }
                const int target = Word(std::abs(Word(Word(object.generalPurpose2) - object.winY))) < 256 ?
                    Word(object.generalPurpose2) : Word(object.generalPurpose2 - 512);
                object.winY = Word(object.winY + Step(object.winY, target, 18));
            }
            else if (object.health == 2000)
            {
                object.velocityY = Word(object.velocityY - 1);
                object.posY = Word(object.posY + object.velocityY);
                object.winX = Word(object.winX + Step(object.winX, 256, 23));
                if (object.posY < 0)
                {
                    object.posY = 0;
                    object.velocityY = 0;
                    if (object.blendWeightOrSpriteOpacity >= 0) { object.behavior = BehaviorType::None; }
                }
                if (object.blendWeightOrSpriteOpacity < 0 && object.winX == 256)
                {
                    ++object.blendWeightOrSpriteOpacity;
                }
            }
            else if (object.health == 3000)
            {
                object.modelFrame2 = 3;
                object.winX = Word(object.winX + Step(object.winX, 128, 23));
                if (object.blendWeightOrSpriteOpacity < 100)
                {
                    object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + 9);
                }
                if (distance < 300) { object.health = 1000; object.modelFrame2 = 2; }
            }
        }
        return object;
    }

    class Context final : public GameContext
    {
    public:
        std::string message;
        int destroyed = 0;
        void PromptUserMessage(std::string_view text, std::int16_t) override { message = text; }
        void OnObjectDestroyed(int) override { ++destroyed; }
    };

    struct Fixture
    {
        Level level = CreateDummyLevel();
        Context context;
        Fixture()
        {
            context.level = &level;
            context.collisionMap = level.collisionMap.get();
            context.soundListenerX = 2000;
            context.soundListenerZ = 2000;
            context.worldObjects[1].posY = 45;
            context.worldObjects[2] = Bat();
        }
        RawGameObject& Object() { return context.worldObjects[2]; }
    };

    void TestOracle()
    {
        Fixture fixture;
        for (int sample = 0; sample < 4096; ++sample)
        {
            auto object = Bat();
            object.health = sample % 4 == 0 ? 2000 : sample % 4 == 1 ? 3000 : 1000;
            object.displayType = sample % 7 == 0 ? DisplayType::Hidden : DisplayType::VectorNormal;
            object.posY = Word(sample % 120 - 10);
            object.velocityX = Word(sample % 7 - 3);
            object.velocityZ = Word(sample % 9 - 4);
            object.velocityY = Word(sample * 37);
            object.doorLockOrGroupBitmask = Word(sample * 131);
            object.winX = Word(sample * 113);
            object.winY = Word(sample % 1024 - 256);
            object.generalPurpose2 = static_cast<std::uint16_t>(sample & 511);
            object.generalPurpose1 = sample % 9 == 0 ? 32760 : 47;
            object.blendWeightOrSpriteOpacity = Word(sample * 71);
            object.behaviorParameter = static_cast<std::uint8_t>(sample & 1);
            fixture.Object() = object;
            fixture.context.tickCount = static_cast<std::int16_t>(sample % 5);
            fixture.context.randomSeed = static_cast<std::uint32_t>(sample) * 65537u;
            auto seed = fixture.context.randomSeed;
            const auto expected = Reference(object, seed, fixture.context.tickCount);
            const auto result = DispatchObjectCase(fixture.Object(), fixture.context);
            if (std::memcmp(&fixture.Object(), &expected, sizeof(expected)) != 0 || fixture.context.randomSeed != seed)
            {
                std::cerr << "Oracle sample " << sample << ", seeds " << fixture.context.randomSeed << "/" << seed << '\n';
                const auto* actualBytes = reinterpret_cast<const std::uint8_t*>(&fixture.Object());
                const auto* expectedBytes = reinterpret_cast<const std::uint8_t*>(&expected);
                for (std::size_t offset = 0; offset < sizeof(expected); ++offset)
                {
                    if (actualBytes[offset] != expectedBytes[offset])
                    {
                        std::cerr << "Byte " << offset << ": " << int(actualBytes[offset]) << "/" << int(expectedBytes[offset]) << '\n';
                    }
                }
            }
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x161d &&
                std::memcmp(&fixture.Object(), &expected, sizeof(expected)) == 0 && fixture.context.randomSeed == seed,
                "Indoor-bat full-record, fixed-point, angle, animation or RNG oracle differs.");
        }
    }

    void TestCeilingAndDeath()
    {
        Fixture fixture;
        auto& object = fixture.Object();
        object.posY = 81;
        fixture.context.soundListenerX = 1850;
        DispatchObjectCase(object, fixture.context);
        Check(object.health == 1000 && object.velocityY == -400, "A nearby bat must descend at the ceiling.");
        object = Bat();
        object.posY = 81;
        fixture.context.soundListenerX = 1851;
        DispatchObjectCase(object, fixture.context);
        Check(object.health == 3000 && object.posY == 96 && object.blendWeightOrSpriteOpacity == 0,
            "Roosting requires distance strictly above 350.");
        fixture.context.soundListenerX = static_cast<std::int16_t>(object.posX + 300);
        fixture.context.soundListenerZ = object.posZ;
        DispatchObjectCase(object, fixture.context);
        Check(object.health == 3000 && object.modelFrame2 == 3, "Distance 300 must not wake a roosting bat.");
        fixture.context.soundListenerX = static_cast<std::int16_t>(object.posX + 299);
        DispatchObjectCase(object, fixture.context);
        Check(object.health == 1000 && object.modelFrame2 == 2, "A nearby player must wake the roosting bat.");

        object = Bat();
        object.collisionType = CollisionType::BatIndoor;
        fixture.context.DamageObject(object, 21);
        Check(object.health == 2000 && object.collisionType == CollisionType::None &&
            object.velocityY == 2 && object.blendWeightOrSpriteOpacity == -100,
            "A lethal weapon hit must enter the recovered bat falling state.");
        fixture.context.tickCount = 250;
        DispatchObjectCase(object, fixture.context);
        Check(object.behavior == BehaviorType::None && object.health == 2000 &&
            object.posY == 0 && object.winX == 256 && object.blendWeightOrSpriteOpacity == 0,
            "A dead bat must land, finish its blend and retain its corpse.");
    }

    void TestWalls()
    {
        Fixture fixture;
        fixture.context.SetCollisionMapPixelAtWorldXZ(1500, 1940, 0x38);
        DispatchObjectCase(fixture.Object(), fixture.context);
        const auto& object = fixture.Object();
        Check(object.generalPurpose2 != 0, "The fifth lookahead point must turn the bat before it reaches a wall.");
        for (int step = 1; step <= 5; ++step)
        {
            Check((fixture.context.GetCollisionMapPixelAtWorldXZ(
                Word(object.posX + 3 * step * object.velocityX),
                Word(object.posZ + 3 * step * object.velocityZ)) & 0x38) == 0,
                "The chosen bat heading must have five unblocked lookahead points.");
        }
        std::fill(std::begin(fixture.level.collisionMap->data), std::end(fixture.level.collisionMap->data), std::uint8_t{0x38});
        fixture.context.discardBlockedObjects = true;
        fixture.Object() = Bat();
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(fixture.Object().health == 0 && fixture.context.destroyed == 1,
            "A fully trapped bat must use the lifecycle-aware stuck-object policy, not hang.");
        fixture.context.discardBlockedObjects = false;
        fixture.context.ReplaceWorldObject(2, Bat());
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(fixture.Object().behavior == BehaviorType::None && fixture.context.message == "WC-Error Obj 2",
            "The normal trapped-bat policy must explicitly report its diagnostic.");
    }

    void TestAudioAndGates()
    {
        class Audio final : public AudioHandler
        {
        public:
            int calls = 0;
            std::uint8_t lastVolume = 0;
            std::uint16_t GetSampleCount() const override { return 1; }
            bool IsChannelPlaying(std::size_t) const override { return false; }
            void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(sample == 1, "Bat used the wrong sound effect.");
                ++calls; lastVolume = volume;
            }
            void StopAllSamples() override {}
        } audio;
        Fixture fixture;
        fixture.context.audioHandler = &audio;
        fixture.context.soundEffectsLoaded = true;
        fixture.context.soundEffects[0] = { 1, {1,0,0,0} };
        fixture.context.soundListenerX = 1750;
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(audio.calls == 1 && audio.lastVolume == 105 && fixture.context.soundChannelSources[3] == 2,
            "Bat sound must use doubled-distance attenuation and the world-slot source.");
        const auto before = fixture.Object();
        fixture.context.tickCount = 0;
        const auto seed = fixture.context.randomSeed;
        DispatchObjectCase(fixture.Object(), fixture.context);
        Check(std::memcmp(&before, &fixture.Object(), sizeof(before)) == 0 && fixture.context.randomSeed == seed,
            "Zero movement ticks must not animate, draw RNG or request bat sound.");
        fixture.context.tickCount = 1;
        fixture.context.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(fixture.Object(), fixture.context).status == DispatchStatus::DispatchDisabled,
            "Disabled bat dispatch must be honored.");
    }

    void TestAssets(const std::filesystem::path& directory)
    {
        class Host final : public GameContext
        {
        public:
            std::filesystem::path directory;
            using GameContext::LoadLevelGeometry;
        protected:
            std::vector<std::uint8_t> LoadFile(std::string_view filename) override
            {
                Check(filename.substr(0, 6) == "LEVEL\\", "Unexpected bat fixture asset request.");
                std::ifstream stream(directory / std::string(filename.substr(6)), std::ios::binary);
                Check(static_cast<bool>(stream), "Cannot open original bat level geometry.");
                return { std::istreambuf_iterator<char>(stream), {} };
            }
        } host;
        host.directory = directory;
        int count = 0;
        for (const auto& file : std::filesystem::directory_iterator(directory))
        {
            const auto name = file.path().filename().string();
            if (name.rfind("OBJPOS", 0) != 0 || file.path().extension() != ".DAT") { continue; }
            std::ifstream stream(file.path(), std::ios::binary);
            Check(static_cast<bool>(stream), "Cannot read original bat object table.");
            LevelLoadData data;
            RawGameObject object;
            while (stream.read(reinterpret_cast<char*>(&object), sizeof(object)))
            {
                if (object.behavior != BehaviorType::BatIndoor) { continue; }
                if (!data.level)
                {
                    host.LoadLevelGeometry(static_cast<std::int16_t>(std::stoi(name.substr(6))), data);
                    host.level = data.level.get();
                    host.collisionMap = data.level->collisionMap.get();
                    host.floorRepeatHeight = data.floorRepeatHeight;
                }
                ++count;
                host.worldObjects[2] = object;
                host.soundListenerX = object.posX;
                host.soundListenerZ = object.posZ;
                for (int step = 0; step < 60; ++step)
                {
                    Check(UpdateObject(host.worldObjects[2], host).status == DispatchStatus::ExecutedCase &&
                        host.worldObjects[2].behavior == BehaviorType::BatIndoor,
                        "An original indoor bat must simulate on its real collision map without becoming stuck.");
                }
            }
            Check(stream.eof(), "Original bat object-table read failed.");
        }
        Check(count == 50, "Unexpected number of original indoor-bat fixtures.");
    }

    void TestRuntime()
    {
        class Host final : public GameContext
        {
        protected:
            void LoadLevel(const LevelLoadRequest&) override {}
        } host;
        class Input final : public InputHandler
        {
            bool IsKeyPressed(InputKey) override { return false; }
        } input;
        const auto request = host.StartLevel(13);
        LevelLoadData data;
        data.level = std::make_unique<Level>(CreateDummyLevel());
        data.objects[2] = Bat();
        data.playerSpawn = PlayerCamera{ {1500,45,2000}, {0,0,100} };
        host.CompleteLevelLoad(request.id, std::move(data));
        host.basicPlayerMovementEnabled = false;
        Check(host.Tick({3,9}, input).status == GameTickStatus::Advanced &&
            host.worldObjects[2].blendWeightOrSpriteOpacity == 47 && host.worldObjects[2].posZ < 2000,
            "Runtime must move the bat per behavior tick but flap only once per frame.");
        const auto before = host.worldObjects[2];
        host.Tick({0,100}, input);
        Check(std::memcmp(&before, &host.worldObjects[2], sizeof(before)) == 0,
            "Frame/burning ticks alone must not advance indoor-bat movement.");

        host.ReplaceWorldObject(2, Bat());
        host.worldObjects[2].behaviorParameter = 1;
        host.tickCount = 1;
        bool tested = false;
        for (std::uint32_t seed = 0; seed < 4096; ++seed)
        {
            auto next = seed;
            Next(next, 2);
            if (Next(next, 20) != 0 || Next(next, 3) != 0) { continue; }
            host.randomSeed = seed;
            DispatchObjectCase(host.worldObjects[2], host);
            Check(host.GetPlayerCamera().position[1] == 45 && host.worldObjects[2].velocityY == 140,
                "Vertical tracking must use the player's eye Y plus half body height, not object-center Y.");
            tested = true;
            break;
        }
        Check(tested, "No deterministic player-height tracking seed was tested.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original LEVEL directory.");
        TestOracle();
        TestCeilingAndDeath();
        TestWalls();
        TestAudioAndGates();
        TestAssets(argv[1]);
        TestRuntime();
        std::cout << "Indoor-bat tests passed (4096 oracle cases and 50 original objects).\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
