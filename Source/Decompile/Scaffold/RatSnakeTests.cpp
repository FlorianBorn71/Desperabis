#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
        seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1) % 4294967296ULL);
        return range == 0 ? 0 : (seed >> 16) % range;
    }

    int Signed(std::uint32_t& seed, int radius)
    {
        const int result = static_cast<int>(Next(seed, static_cast<unsigned int>(2 * radius + 1))) - radius;
        return result == 0 ? radius : result;
    }

    bool Equal(const RawGameObject& first, const RawGameObject& second)
    {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    }

    void Direction(RawGameObject& object, bool clamped)
    {
        const auto& table = clamped ? ClampedSineTable : SineTable;
        const auto high = [](std::int64_t value)
        {
            return value >= 0 ? value / 65536 : -((-value + 65535) / 65536);
        };
        object.velocityX = Word(high(static_cast<std::int64_t>(table[static_cast<std::uint16_t>(object.winY) % 512]) *
            object.itemValue));
        object.velocityZ = Word(-high(static_cast<std::int64_t>(
            table[(static_cast<std::uint16_t>(object.winY) + 128) % 512]) * object.itemValue));
    }

    RawGameObject Reference(RawGameObject object, std::uint32_t& seed, int ticks, bool collisions,
        std::uint8_t mapValue, std::int16_t listenerX = 0, std::int16_t listenerZ = 0)
    {
        for (int tick = 0; tick < ticks; ++tick)
        {
            if (object.health == 1000)
            {
                if (collisions && (mapValue & 7) != 1)
                {
                    object.posX = Word(object.posX + object.velocityX);
                    object.posZ = Word(object.posZ + object.velocityZ);
                }
                else { object.winY = Word((object.winY + 16) & 511); }
                if (Word(object.field_24) > 0 && Next(seed, object.field_24) == 0)
                {
                    auto target = GetVectorAngle(Word(listenerX - object.posX), Word(listenerZ - object.posZ));
                    int current = (static_cast<std::uint16_t>(object.winY) % 512) + 512;
                    while (std::abs(current - target) > 256) { current -= 512; }
                    const int delta = target - current;
                    object.winY = Word(current + std::clamp(delta, -13, 13));
                    Direction(object, true);
                }
                if (Next(seed, 20) == 0) { object.velocityX = 0; object.velocityZ = 0; }
                object.posY = 0;
                if (Next(seed, object.field_28) == 0)
                {
                    object.winY = Word((object.winY + Signed(seed, 20)) & 511);
                    Direction(object, true);
                    object.field_28 = 15;
                }
                if (object.velocityX != 0 || object.velocityZ != 0)
                {
                    const auto phase = Word(2LL * object.itemValue * object.itemValue + object.blendWeightOrSpriteOpacity);
                    object.blendWeightOrSpriteOpacity = Word(phase % 200);
                }
                Signed(seed, 1);
                if (object.modelAnim == 1) { Next(seed, 300); }
            }
            else if (object.health > 1100)
            {
                object.velocityY = Word(object.velocityY - 1);
                object.posY = Word(object.posY + object.velocityY);
                if (object.posY < 0) { object.posY = 0; object.velocityY = 0; }
                object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + 1);
                if (object.blendWeightOrSpriteOpacity > 0) { object.behavior = BehaviorType::None; }
            }
        }
        return object;
    }

    RawGameObject Fixture()
    {
        RawGameObject object;
        object.behavior = BehaviorType::RatAndSnake;
        object.health = 1000;
        object.posX = 1500;
        object.posZ = 2000;
        object.posY = 17;
        object.itemValue = 3;
        object.velocityX = 3;
        object.field_28 = 0;
        return object;
    }

    void TestMath()
    {
        Check(GetVectorAngle(0, 0) == 256 && GetVectorAngle(0, 1) == 256 &&
            GetVectorAngle(0, -1) == 0 && GetVectorAngle(1, 0) == 128 &&
            GetVectorAngle(-1, 0) == 384 && GetVectorAngle(1, 1) == 192 &&
            GetVectorAngle(1, -1) == 64 && GetVectorAngle(-1, 1) == 320 &&
            GetVectorAngle(-1, -1) == 448 && GetVectorAngle(32767, -32768) == 448,
            "Vector angle axes/quadrants or signed ABS overflow differ.");
        for (int bits = 0; bits < 65536; ++bits)
        {
            std::int16_t vectorX = 0;
            std::int16_t vectorZ = 0;
            GetDirectionDisplacement(vectorX, vectorZ, Word(bits), 10000);
            Check(GetVectorAngle(vectorX, vectorZ) == bits % 512,
                "Vector-angle conversion failed to recover an ordinary-table direction.");
            for (int targetBits : { 0, 1, 128, 255, 256, 511, 32767, 65535 })
            {
                auto current = Word(bits);
                auto target = Word(targetBits);
                NormalizeAnglePair(current, target);
                int expected = (bits % 512) + 512;
                while (std::abs(expected - targetBits % 512) > 256) { expected -= 512; }
                Check(current == expected && target == targetBits % 512, "Angle pair normalization differs.");
            }
            for (int speed : { -32768, -1, 0, 1, 3, 32767 })
            {
                RawGameObject expected;
                expected.winY = Word(bits);
                expected.itemValue = Word(speed);
                Direction(expected, true);
                std::int16_t x = 0;
                std::int16_t z = 0;
                GetClampedDirectionDisplacement(x, z, Word(bits), Word(speed));
                Check(x == expected.velocityX && z == expected.velocityZ, "Clamped direction multiplication differs.");
            }
            GameContext context;
            context.randomSeed = static_cast<std::uint32_t>(bits);
            auto seed = context.randomSeed;
            Next(seed, 0);
            Check(context.Random(0) == 0 && context.randomSeed == seed, "Native zero-bound RNG behavior differs.");
        }
    }

    void TestMovement()
    {
        auto level = CreateDummyLevel();
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        const std::array<std::int16_t, 9> health{ 999, 1000, 1001, 1100, 1101, 32767, -1, 0, 1000 };
        const std::array<std::uint16_t, 5> steer{ 0, 1, 70, 32768, 65535 };
        const std::array<std::uint16_t, 4> wander{ 0, 1, 15, 65535 };
        for (unsigned int sample = 0; sample < 65536; ++sample)
        {
            auto object = Fixture();
            object.health = health[sample % health.size()];
            object.field_24 = steer[sample % steer.size()];
            object.field_28 = wander[(sample / steer.size()) % wander.size()];
            object.winY = Word(sample);
            object.blendWeightOrSpriteOpacity = Word(sample * 7);
            object.modelAnim = static_cast<std::uint8_t>(sample % 3);
            object.velocityY = Word(sample);
            object.renderPlacementMode = 0xa5;
            object.field_26 = 0xdead;
            object.behaviorParameter = 1;
            object.spriteScale = 3;
            context.tickCount = static_cast<std::int16_t>(sample % 5);
            context.randomSeed = sample * 65537u;
            context.objectCollisionsEnabled = sample % 2 != 0;
            const std::uint8_t value = (sample & 2) != 0 ? 1 : 0;
            // Both patterns have no masked obstacle bits; the 3x3 query returns no correction.
            if (sample % 4 == 0 || sample % 4 == 2)
            {
                std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), value);
            }
            context.soundListenerX = Word(sample * 3);
            context.soundListenerZ = Word(sample * 11);
            auto seed = context.randomSeed;
            const auto expected = Reference(object, seed, context.tickCount, context.objectCollisionsEnabled,
                value, context.soundListenerX, context.soundListenerZ);
            const auto result = Function_RatAndSnake(object, context);
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x15ef &&
                result.behavior == BehaviorType::RatAndSnake && Equal(object, expected) && context.randomSeed == seed,
                "Rat/snake movement, health gate, fields or RNG sequence differ from instruction oracle.");
        }
        auto object = Fixture();
        context.tickCount = 32767;
        object.health = 1101;
        context.randomSeed = 123;
        auto expectedSeed = context.randomSeed;
        const auto maximumExpected = Reference(object, expectedSeed, 32767, true, 0);
        Function_RatAndSnake(object, context);
        Check(Equal(object, maximumExpected) && context.randomSeed == expectedSeed,
            "Maximum-tick alternate-health path must continue after behavior changes.");
        object = Fixture();
        context.tickCount = 0;
        context.level = nullptr;
        context.collisionMap = nullptr;
        const auto before = object;
        const auto seed = context.randomSeed;
        Function_RatAndSnake(object, context);
        Check(Equal(before, object) && seed == context.randomSeed, "Zero-tick rat/snake must do no local work.");
    }

    void TestNeighborhoodAndFallback()
    {
        class MessageContext final : public GameContext
        {
        public:
            std::string userMessage;
            std::int16_t userMessageTicks = 0;
            void PromptUserMessage(std::string_view text, std::int16_t duration) override
            {
                userMessage = text;
                userMessageTicks = duration;
            }
        };
        auto level = CreateDummyLevel();
        MessageContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        for (int pattern = 0; pattern < 512; ++pattern)
        {
            std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), std::uint8_t{ 0 });
            int sumX = 5;
            int sumZ = -7;
            int bit = 0;
            for (int x = -1; x <= 1; ++x)
            {
                for (int z = -1; z <= 1; ++z, ++bit)
                {
                    if ((pattern & (1 << bit)) == 0) { continue; }
                    level.collisionMap->data[(162 - (80 + z)) * 200 + 55 + x] = 0x38;
                    sumX -= x * 4;
                    sumZ -= z * 4;
                }
            }
            std::int16_t x = 5;
            std::int16_t z = -7;
            std::uint16_t center = 123;
            context.QueryCollisionMapNeighborhood(1487, 2038, 4, x, z, center, 0x38);
            Check(x == ((sumX > 0) - (sumX < 0)) * 4 && z == ((sumZ > 0) - (sumZ < 0)) * 4 &&
                center == ((pattern & 16) != 0 ? 0x38 : 0), "Neighborhood scan/mask/in-out corrections differ.");
        }
        std::fill(std::begin(level.collisionMap->data), std::end(level.collisionMap->data), std::uint8_t{ 0 });
        for (int row = 0; row < 200; ++row) { level.collisionMap->data[row * 200 + 2] = 0x38; }
        for (bool discard : { false, true })
        {
            context.discardBlockedObjects = discard;
            context.tickCount = 1;
            context.randomSeed = 1234;
            auto& object = context.worldObjects[2];
            object = Fixture();
            object.posX = 150;
            object.posZ = 4050;
            object.velocityX = 0;
            object.itemValue = 1;
            context.userMessage.clear();
            Function_RatAndSnake(object, context);
            Check(discard ? object.health == 0 :
                object.behavior == BehaviorType::None && object.blendWeightOrSpriteOpacity == -100 &&
                object.posY == 50 && context.userMessage == "WC-Error Obj 2" && context.userMessageTicks == 50,
                "Sixteen failed obstacle retries must take the recovered discard/diagnostic path.");
        }
    }

    struct Audio : AudioHandler
    {
        unsigned int calls = 0;
        std::uint8_t volume = 0;
        std::uint16_t GetSampleCount() const override { return 10; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t sample, std::uint8_t value) override
        {
            Check(sample == 4, "Wrong rat/snake sound sample.");
            ++calls;
            volume = value;
        }
    };

    void TestSoundAndAssets(const std::filesystem::path& directory)
    {
        auto level = CreateDummyLevel();
        GameContext context;
        context.level = &level;
        context.collisionMap = level.collisionMap.get();
        context.tickCount = 1;
        context.soundEffectsLoaded = true;
        context.soundEffects[16].sampleCount = 1;
        context.soundEffects[16].samples[0] = 4;
        Audio audio;
        context.audioHandler = &audio;
        context.soundListenerX = 1500;
        context.soundListenerZ = 2000;
        for (unsigned int seed = 0; seed < 1024; ++seed)
        {
            auto& object = context.worldObjects[2];
            object = Fixture();
            object.modelAnim = 1;
            context.randomSeed = seed;
            Function_RatAndSnake(object, context);
        }
        Check(audio.calls > 0 && audio.volume == 255 && context.soundChannelSources[3] == 2,
            "Animation-1 rat/snake sound must use effect 17 and the object source slot.");
        context.audioHandler = nullptr;
        context.soundEffectsLoaded = false;
        int count = 0;
        for (const auto& file : std::filesystem::directory_iterator(directory))
        {
            const auto name = file.path().filename().string();
            if (name.rfind("OBJPOS", 0) != 0 || file.path().extension() != ".DAT") { continue; }
            std::ifstream input(file.path(), std::ios::binary);
            Check(static_cast<bool>(input), "Cannot open original object table.");
            RawGameObject object;
            while (input.read(reinterpret_cast<char*>(&object), sizeof(object)))
            {
                if (object.behavior != BehaviorType::RatAndSnake) { continue; }
                ++count;
                for (int ticks : { 0, 1, 3 })
                {
                    auto actual = object;
                    context.tickCount = static_cast<std::int16_t>(ticks);
                    context.randomSeed = static_cast<std::uint32_t>(count);
                    auto seed = context.randomSeed;
                    const auto expected = Reference(actual, seed, ticks, true, 0, 1500, 2000);
                    Function_RatAndSnake(actual, context);
                    Check(Equal(actual, expected) && context.randomSeed == seed, "Original rat/snake record differs.");
                }
            }
            Check(input.eof() && input.gcount() == 0, "Partial original object record.");
        }
        Check(count == 40, "Expected all 40 original rat/snake records.");
        std::cout << "PASS: all 40 original rat/snake records with explicit dummy geometry/map.\n";
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original level directory.");
        TestMath();
        TestMovement();
        TestNeighborhoodAndFallback();
        TestSoundAndAssets(argv[1]);
        std::cout << "PASS: rat/snake gates, RNG, heading, movement, obstacle retries, sound and health transition.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
