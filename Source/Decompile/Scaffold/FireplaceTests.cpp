#include "BehaviorDispatch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
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

    int ShiftDown(int value, int divisor)
    {
        return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
    }

    unsigned int Next(std::uint32_t& seed, unsigned int range)
    {
        seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1u) % 4294967296ULL);
        return (seed >> 16) % range;
    }

    bool Equal(const RawGameObject& first, const RawGameObject& second)
    {
        return std::memcmp(&first, &second, sizeof(first)) == 0;
    }

    RawGameObject ReferenceParticle(const RawGameObject& source, std::uint32_t& seed)
    {
        RawGameObject particle;
        particle.modelId = 39;
        particle.modelAnim = 3;
        particle.animFrame = 5;
        particle.modelFrame2 = 2;
        particle.blendWeightOrSpriteOpacity = 100;
        particle.renderPlacementMode = 130;
        particle.velocityY = static_cast<std::int16_t>(Next(seed, 500) + 600);
        particle.behavior = BehaviorType::FireParticle;
        particle.generalPurpose1 = static_cast<std::uint16_t>(Next(seed, 4));
        particle.health = static_cast<std::int16_t>(Next(seed, 60) + 50);
        particle.displayType = DisplayType::VectorUnlit;
        int noise = static_cast<int>(Next(seed, 61)) - 30;
        if (noise == 0) { noise = 30; }
        particle.doorLockOrGroupBitmask = static_cast<std::int16_t>(std::clamp(
            -ShiftDown(noise + particle.velocityY / particle.health, 2), -30, 1));
        particle.posX = source.posX;
        particle.posY = Word(source.posY + source.collisionRadiusY);
        particle.posZ = source.posZ;
        particle.collisionRadiusXZ = 10;
        particle.collisionRadiusY = 10;
        particle.winX = static_cast<std::int16_t>(Next(seed, 512));
        particle.winY = static_cast<std::int16_t>(Next(seed, 512));
        return particle;
    }

    RawGameObject ReferenceUpdate(RawGameObject object, int ticks, std::uint32_t& seed)
    {
        const auto fraction = Word(object.itemValue + Word(static_cast<std::int64_t>(object.velocityY) * ticks));
        object.posY = Word(object.posY + ShiftDown(fraction, 256));
        object.itemValue = static_cast<std::int16_t>(static_cast<std::uint16_t>(fraction) % 256);
        const auto step = Word(ticks * Next(seed, 3));
        const auto difference = Word(-object.blendWeightOrSpriteOpacity);
        const auto magnitude = Word(difference < 0 ? -static_cast<int>(difference) : difference);
        const auto delta = magnitude <= step ? difference :
            Word((difference < 0 ? -1 : difference > 0 ? 1 : 0) * step);
        object.blendWeightOrSpriteOpacity = Word(object.blendWeightOrSpriteOpacity + delta);
        object.velocityY = Word(object.velocityY +
            static_cast<std::int64_t>(object.doorLockOrGroupBitmask) * ticks);
        object.winY = Word(object.winY + static_cast<std::int64_t>(object.generalPurpose1) * ticks);
        object.displayType = static_cast<DisplayType>(std::clamp(37 - ShiftDown(object.health, 4), 30, 36));
        return object;
    }

    struct Audio final : AudioHandler
    {
        std::array<bool, SoundChannelCount> playing{};
        unsigned int calls = 0;
        std::uint8_t sample = 0;
        std::uint8_t volume = 0;
        std::size_t channel = 0;
        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t index) const override { return playing.at(index); }
        void PlaySample(std::size_t index, std::uint8_t value, std::uint8_t loudness) override
        {
            ++calls;
            channel = index;
            sample = value;
            volume = loudness;
        }
    };

    bool ReferenceEmitter(const RawGameObject& source, int ticks, std::uint32_t& seed,
        LightingCache& cache, bool tableLoaded, std::uint8_t& sample)
    {
        sample = 0;
        if (source.doorLockOrGroupBitmask >= static_cast<int>(Next(seed, 20)) && tableLoaded)
        {
            sample = static_cast<std::uint8_t>(Next(seed, 2) + 7);
        }
        for (int x = -2; x <= 2; ++x)
        {
            for (int z = -2; z <= 2; ++z)
            {
                const int weight = 5 - (x < 0 ? -x : x) - (z < 0 ? -z : z);
                const auto amount = static_cast<int>(Next(seed, 30)) * weight * weight / 8;
                const int dx = Word(Word(source.posX + x * 40) + 50) / 100 - cache.centerX / 100;
                const int dz = Word(Word(source.posZ + z * 40) + 50) / 100 - cache.centerZ / 100;
                if (dx >= -cache.activeRadius && dx <= cache.activeRadius &&
                    dz >= -cache.activeRadius && dz <= cache.activeRadius)
                {
                    auto& cell = cache.contributions[static_cast<std::size_t>(dx + 10)]
                        [static_cast<std::size_t>(dz + 10)];
                    cell = Word(cell + amount);
                }
            }
        }
        const auto chance = Next(seed, 100);
        return Word(static_cast<std::int64_t>(source.doorLockOrGroupBitmask) * ticks) >
            static_cast<int>(chance);
    }

    RawGameObject MakeEmitter()
    {
        RawGameObject object;
        object.behavior = BehaviorType::BurningFireplace;
        object.collisionType = CollisionType::DamageObj;
        object.health = 1000;
        object.posX = 1550;
        object.posY = -200;
        object.posZ = 2550;
        object.collisionRadiusY = 23;
        object.doorLockOrGroupBitmask = 7;
        return object;
    }

    void TestEmitterAndFactory()
    {
        GameContext context;
        Audio audio;
        context.soundEffectsLoaded = true;
        context.soundEffects[61].sampleCount = 2;
        context.soundEffects[61].samples = { 7, 8, 0, 0 };
        const std::array<std::int16_t, 8> intensities{ -32768, -1, 0, 1, 7, 20, 100, 32767 };
        const std::array<std::int16_t, 4> ticks{ 0, 1, 2, 32767 };
        for (unsigned int value = 0; value <= 65535; ++value)
        {
            auto emitter = MakeEmitter();
            emitter.doorLockOrGroupBitmask = intensities[(value / 4) % intensities.size()];
            context.worldObjects[10] = emitter;
            context.worldObjects[2] = {};
            context.tickCount = ticks[value % ticks.size()];
            context.randomSeed = value;
            context.audioHandler = (value & 256) != 0 ? nullptr : &audio;
            context.soundEnabled = (value & 128) == 0;
            context.positionalSoundsSuppressed = (value & 64) != 0;
            context.soundListenerX = emitter.posX;
            context.soundListenerZ = emitter.posZ;
            audio = {};
            if ((value & 512) != 0) { audio.playing = { true, true, true, false }; }
            context.lightingCache.Reset(emitter.posX, emitter.posZ, static_cast<std::uint8_t>(value % 3));
            for (auto& row : context.lightingCache.contributions) { row.fill(32760); }
            auto expectedCache = context.lightingCache;
            auto seed = context.randomSeed;
            std::uint8_t sample = 0;
            const auto spawn = ReferenceEmitter(emitter, context.tickCount, seed, expectedCache, true, sample);
            const auto particle = spawn ? ReferenceParticle(emitter, seed) : RawGameObject{};
            const auto result = DispatchObjectCase(context.worldObjects[10], context);
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x18c6 &&
                Equal(context.worldObjects[10], emitter), "The emitter changed its own object or reported a stub.");
            Check(context.randomSeed == seed && Equal(context.worldObjects[2], particle),
                "Particle initialization or random-number ordering differs from the executable.");
            Check(context.lightingCache.contributions == expectedCache.contributions,
                "The exact 25-sample weighted lighting footprint differs.");
            const bool played = sample != 0 && context.audioHandler && context.soundEnabled &&
                !context.positionalSoundsSuppressed && (value & 512) == 0;
            Check(audio.calls == (played ? 1u : 0u) &&
                (!played || (audio.sample == sample && audio.volume == 255 && audio.channel == 3 &&
                    context.soundChannelSources[3] == 10)),
                "Fire sound attenuation, reserved-channel policy, or sample selection differs.");
        }
        context = GameContext{};
        for (auto& object : context.worldObjects) { object.health = 1000; }
        auto source = MakeEmitter();
        source.posY = 32767;
        source.collisionRadiusY = 1;
        auto seed = context.randomSeed;
        const auto expected = ReferenceParticle(source, seed);
        Check(context.SpawnFireParticle(source) == 165 && Equal(context.worldObjects[165], expected) &&
            context.randomSeed == seed, "A full world must use cleared fallback slot 165.");
        UpdateObjects(context);
        Check(Equal(context.worldObjects[165], expected) && context.randomSeed == seed,
            "The normal world loop must leave fallback slot 165 untouched.");
        context = GameContext{};
        context.worldObjects[2] = MakeEmitter();
        context.worldObjects[2].health = 0;
        Check(context.SpawnFireParticle(context.worldObjects[2]) == 0 &&
            Equal(context.worldObjects[2], RawGameObject{}) && context.randomSeed == 0,
            "Allocation must reread a source that it has just cleared.");
        context = GameContext{};
        source.collisionType = CollisionType::Dragon;
        bool rejected = false;
        try { context.SpawnFireParticle(source); }
        catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "Unrecovered fragment types must not silently create fire particles.");
        context = GameContext{};
        source = MakeEmitter();
        context.worldObjects[2].health = -1;
        seed = context.randomSeed;
        const auto nextParticle = ReferenceParticle(source, seed);
        Check(context.SpawnFireParticle(source) == 3 && context.worldObjects[2].health == -1 &&
            Equal(context.worldObjects[3], nextParticle) && context.randomSeed == seed,
            "The allocator must skip negative-health slots rather than treating them as free.");
    }

    void TestParticleMotion()
    {
        GameContext context;
        RawGameObject fixture;
        std::array<std::uint8_t, sizeof(fixture)> bytes{};
        bytes.fill(0xa5);
        std::memcpy(&fixture, bytes.data(), bytes.size());
        fixture.behavior = BehaviorType::FireParticle;
        for (const auto ticks : { 0, 1, 2, 32767 })
        {
            context.tickCount = static_cast<std::int16_t>(ticks);
            for (unsigned int value = 0; value <= 65535; ++value)
            {
                auto object = fixture;
                object.health = Word(value);
                object.velocityY = Word(value);
                object.itemValue = Word(value * 3);
                object.blendWeightOrSpriteOpacity = Word(value * 17);
                object.doorLockOrGroupBitmask = Word(value * 5);
                object.generalPurpose1 = static_cast<std::uint16_t>(value);
                object.winY = Word(value * 7);
                context.randomSeed = value;
                auto seed = context.randomSeed;
                const auto expected = ReferenceUpdate(object, ticks, seed);
                const auto result = Function_FireParticle(object, context);
                Check(Equal(object, expected) && context.randomSeed == seed &&
                    result.status == DispatchStatus::ExecutedCase,
                    "Particle motion, fading, acceleration, angle, or lifetime shading differs.");
            }
        }
        for (unsigned int value = 0; value <= 65535; ++value)
        {
            context.randomSeed = value;
            auto seed = context.randomSeed;
            int expected = static_cast<int>(Next(seed, 61)) - 30;
            if (expected == 0) { expected = 30; }
            Check(context.RandomSigned(30) == expected && context.randomSeed == seed,
                "Signed random must replace zero rather than rerolling it.");
        }
        for (const std::uint16_t radius : { std::uint16_t{0}, std::uint16_t{32767} })
        {
            auto seed = context.randomSeed;
            int expected = static_cast<int>(Next(seed, radius * 2u + 1u)) - radius;
            if (expected == 0) { expected = radius; }
            Check(context.RandomSigned(radius) == expected && context.randomSeed == seed,
                "Signed random radius boundaries differ.");
        }
        const auto seed = context.randomSeed;
        bool rejected = false;
        try { context.RandomSigned(32768); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected && context.randomSeed == seed, "An invalid signed radius must fail without advancing RNG.");
    }

    void TestWorldUpdateAndAssets(const char* directory)
    {
        GameContext context;
        context.tickCount = 15;
        context.worldObjects[2] = MakeEmitter();
        auto seed = context.randomSeed;
        auto cache = context.lightingCache;
        std::uint8_t sample = 0;
        Check(ReferenceEmitter(context.worldObjects[2], context.tickCount, seed, cache, false, sample),
            "The integration fixture must emit a particle.");
        auto particle = ReferenceParticle(context.worldObjects[2], seed);
        particle.health -= context.tickCount;
        particle = ReferenceUpdate(particle, context.tickCount, seed);
        const auto results = UpdateObjects(context);
        Check(results[1].behavior == BehaviorType::FireParticle &&
            Equal(context.worldObjects[3], particle) && context.randomSeed == seed,
            "Particles spawned into later slots must update during the same world pass.");

        context.objectCollisionsEnabled = false;
        context.worldObjects[3].health = 1;
        seed = context.randomSeed;
        particle = ReferenceUpdate(context.worldObjects[3], context.tickCount, seed);
        UpdateObject(context.worldObjects[3], context);
        Check(Equal(context.worldObjects[3], particle) && context.randomSeed == seed,
            "Disabling the shared lifetime countdown must preserve particle health.");
        context.objectCollisionsEnabled = true;
        particle.health = 0;
        particle = ReferenceUpdate(particle, context.tickCount, seed);
        UpdateObject(context.worldObjects[3], context);
        Check(Equal(context.worldObjects[3], particle) && context.randomSeed == seed,
            "The last particle update must still run after shared lifetime reaches zero.");
        Check(UpdateObject(context.worldObjects[3], context).status == DispatchStatus::InactiveObject &&
            context.randomSeed == seed, "Expired particles must stop consuming randomness.");

        context = GameContext{};
        context.worldObjects[2] = MakeEmitter();
        context.worldObjects[2].health = 1;
        context.worldObjects[2].doorLockOrGroupBitmask = 100;
        seed = context.randomSeed;
        cache = context.lightingCache;
        Check(ReferenceEmitter(context.worldObjects[2], 1, seed, cache, false, sample),
            "The source-alias fixture must attempt emission.");
        Check(UpdateObject(context.worldObjects[2], context).status == DispatchStatus::ExecutedCase &&
            Equal(context.worldObjects[2], RawGameObject{}) && context.randomSeed == seed,
            "A source expiring during the shared prefix can be cleared by its own particle allocation.");

        unsigned int count = 0;
        for (const auto name : { "OBJPOS23.DAT", "OBJPOS24.DAT" })
        {
            const auto path = std::filesystem::path(directory) / name;
            std::ifstream file(path, std::ios::binary);
            Check(static_cast<bool>(file), "Cannot open the original fireplace asset fixture.");
            const std::vector<char> data{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
            Check(!file.bad() && data.size() % sizeof(RawGameObject) == 0, "Invalid object-table fixture.");
            for (std::size_t at = 0; at < data.size(); at += sizeof(RawGameObject))
            {
                RawGameObject object;
                std::memcpy(&object, data.data() + at, sizeof(object));
                if (object.behavior != BehaviorType::BurningFireplace) { continue; }
                ++count;
                Check(object.collisionType == CollisionType::DamageObj &&
                    object.doorLockOrGroupBitmask == (name[7] == '3' ? 0 : 7),
                    "The extracted fireplace configuration changed.");
                context = GameContext{};
                context.tickCount = 15;
                context.worldObjects[2] = object;
                const auto update = UpdateObjects(context);
                Check(update[0].status == DispatchStatus::ExecutedCase &&
                    Equal(context.worldObjects[2], object), "A real fireplace fixture failed to update.");
                Check((context.worldObjects[3].behavior == BehaviorType::FireParticle) ==
                    (object.doorLockOrGroupBitmask == 7), "Real intensity values must control particle emission.");
            }
        }
        Check(count == 6, "Expected five level-23 fireplaces and one level-24 fireplace.");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected the original level-asset directory.");
        TestEmitterAndFactory();
        TestParticleMotion();
        TestWorldUpdateAndAssets(argv[1]);
        std::cout << "PASS: fireplace lighting/audio/spawning, particle arithmetic/lifecycle, "
            "allocation edges, and all six original fireplace fixtures verified.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
