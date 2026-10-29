#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace Desperabis;

namespace
{
    class MessageContext final : public GameContext
    {
    public:
        std::string userMessage;
        std::int16_t userMessageTicks = 0;
        int promptCalls = 0;

        void PromptUserMessage(std::string_view text, std::int16_t duration) override
        {
            userMessage = text;
            userMessageTicks = duration;
            ++promptCalls;
        }

        bool IsUserMessageActive() const override { return userMessageTicks > 0; }
    };

    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    std::array<std::uint8_t, sizeof(RawGameObject)> ObjectBytes(const RawGameObject& object)
    {
        std::array<std::uint8_t, sizeof(RawGameObject)> bytes;
        std::memcpy(bytes.data(), &object, sizeof(object));
        return bytes;
    }

    std::array<std::uint16_t, 27> ObjectWords(const RawGameObject& object)
    {
        std::array<std::uint16_t, 27> words;
        std::memcpy(words.data(), &object, sizeof(object));
        return words;
    }

    void TestObjectLayout()
    {
        RawGameObject object;
        for (const auto value : ObjectBytes(object))
        {
            Check(value == 0, "Default object must remain zero-initialized.");
        }
        std::array<std::uint16_t, 27> fixture;
        for (std::size_t i = 0; i < fixture.size(); ++i)
        {
            fixture[i] = static_cast<std::uint16_t>(0x8080u + i * 257u);
        }
        std::memcpy(&object, fixture.data(), sizeof(object));
        const auto bytes = ObjectBytes(object);
        std::array<std::int16_t, 27> signedWords;
        std::memcpy(signedWords.data(), fixture.data(), sizeof(object));
        const auto unsignedWords = ObjectWords(object);
        for (std::size_t i = 0; i < unsignedWords.size(); ++i)
        {
            Check(unsignedWords[i] == fixture[i] &&
                bytes[i * 2] == (fixture[i] & 0xff) && bytes[i * 2 + 1] == (fixture[i] >> 8),
                "The struct must preserve the original little-endian fixture.");
        }
        Check(object.modelId == bytes[0] && object.modelAnim == bytes[1] &&
            object.modelFrame2 == bytes[2] && object.animFrame == bytes[3],
            "Animation byte fields differ from the renderer accessor layout.");
        Check(object.renderPlacementMode == bytes[4] && object.soundEffectId == bytes[5] &&
            object.renderSortKey == signedWords[24],
            "Placement mode and configured sound must occupy separate original bytes.");
        Check(static_cast<std::uint16_t>(object.behavior) == unsignedWords[4] && object.health == signedWords[5] &&
            object.posX == signedWords[6] && object.posY == signedWords[7] && object.posZ == signedWords[8],
            "Behavior, health, or position field layout changed.");
        Check(object.blendWeightOrSpriteOpacity == signedWords[21] &&
            object.groupIndexOrLock == signedWords[22] &&
            object.doorLockOrGroupBitmask == signedWords[23] &&
            static_cast<std::uint8_t>(object.displayType) == bytes[51] &&
            object.behaviorParameter == bytes[52] && object.spriteScale == bytes[53],
            "Behavior-dependent or display field layout changed.");

        auto split = object;
        auto expectedBytes = bytes;
        for (unsigned int value = 0; value < 256; ++value)
        {
            split.behaviorParameter = static_cast<std::uint8_t>(value);
            expectedBytes[52] = static_cast<std::uint8_t>(value);
            Check(ObjectBytes(split) == expectedBytes,
                "Behavior parameter writes must change only byte 52, preserving sprite scale.");
            split.spriteScale = static_cast<std::uint8_t>(255 - value);
            expectedBytes[53] = static_cast<std::uint8_t>(255 - value);
            Check(ObjectBytes(split) == expectedBytes,
                "Sprite scale writes must change only byte 53, preserving the behavior parameter.");
        }
        for (unsigned int value = 0; value < 256; ++value)
        {
            split.pickupType = static_cast<PickupType>(value);
            expectedBytes[50] = static_cast<std::uint8_t>(value);
            Check(ObjectBytes(split) == expectedBytes,
                "Pickup type must occupy only its original byte, including unnamed values.");
            const auto decoded = DecodeGameObject(expectedBytes.data(), expectedBytes.size());
            Check(static_cast<std::uint8_t>(decoded.pickupType) == value,
                "Decoding must preserve every pickup type byte without normalization.");
            std::array<std::uint8_t, sizeof(RawGameObject)> encoded{};
            EncodeGameObject(decoded, encoded.data(), encoded.size());
            Check(encoded == expectedBytes, "Named and unnamed pickup types must round-trip exactly.");
        }

        RawGameObject copy = object;
        Check(ObjectBytes(copy) == bytes, "Struct copies must preserve all 54 raw bytes.");
        copy.behavior = BehaviorType::Behavior_300;
        copy.collisionType = CollisionType::Gold;
        copy.displayType = DisplayType::SpriteTransparent;
        copy.health = -123;
        copy.posX = -32768;
        copy.posY = 1234;
        copy.posZ = 32767;
        copy.behaviorParameter = 255;
        copy.spriteScale = 255;
        copy.renderSortKey = -1234;
        const auto copyWords = ObjectWords(copy);
        Check(copyWords[4] == 300 && copyWords[5] == static_cast<std::uint16_t>(-123),
            "Named fields must preserve full-width behavior and signed health.");
        Check(ObjectBytes(copy)[6] == 3 && ObjectBytes(copy)[51] == 1,
            "Typed collision/display members must preserve their original byte values.");
        Check(copyWords[6] == 0x8000 && copyWords[7] == 1234 &&
            copyWords[8] == 32767 && copyWords[26] == 0xffff &&
            copyWords[24] == static_cast<std::uint16_t>(-1234),
            "Named field writes did not reach the original offsets.");
        RawGameObject& direct = copy;
        direct.posX = 99;
        Check(copy.posX == 99, "Passing the object by reference must not create a snapshot.");
        for (int mode = 0; mode <= 255; ++mode)
        {
            copy.renderPlacementMode = static_cast<std::uint8_t>(mode);
            for (int effect = 0; effect <= 255; ++effect)
            {
                copy.soundEffectId = static_cast<std::uint8_t>(effect);
                Check(copy.renderPlacementMode == mode && ObjectWords(copy)[2] == mode + effect * 256,
                    "Sound writes must preserve the placement byte and the original little-endian word.");
            }
        }
        const auto beforeModeChange = ObjectBytes(copy);
        copy.renderPlacementMode = 0;
        auto expectedModeChange = beforeModeChange;
        expectedModeChange[4] = 0;
        Check(ObjectBytes(copy) == expectedModeChange,
            "Placement writes must not overwrite the sound byte or any other object data.");
    }

    void TestCases()
    {
        std::size_t specific = 0;
        for (const auto& entry : BehaviorCatalog())
        {
            RawGameObject object;
            object.behavior = entry.type;
            object.health = 1;
            object.posX = -100;
            if (entry.type == BehaviorType::WaterDrop || entry.type == BehaviorType::FloatingDebris ||
                entry.type == BehaviorType::SinkingObject || entry.type == BehaviorType::MetalPole ||
                entry.type == BehaviorType::BearTrapAmmo || entry.type == BehaviorType::Mummie ||
                entry.type == BehaviorType::MummieGuard)
            {
                object.posX = 150;
                object.posZ = 4050;
            }
            if (entry.type == BehaviorType::MummieGuard) { object.soundEffectId = 15; }
            const auto before = ObjectBytes(object);
            GameContext context;
            context.tickCount = 3;
            auto level = CreateDummyLevel();
            level.heightSurfaces[0] = Level::HeightSurface{ 0, 0 };
            context.collisionMap = level.collisionMap.get();
            context.level = &level;
            const auto result = DispatchObjectCase(object, context);
            Check(result.moduleOffset == entry.moduleOffset, "Wrong case entry.");
            Check(result.behavior == entry.type, "Wrong behavior enum from handler.");
            Check(FindBehavior(entry.type) == &entry && FindBehavior(entry.name) == &entry,
                "Behavior enum/name lookup must reference the same descriptor.");
            const bool implemented = entry.type == BehaviorType::Pillar || entry.type == BehaviorType::LightSource ||
                entry.type == BehaviorType::Door ||
                entry.type == BehaviorType::Flies ||
                entry.type == BehaviorType::WeaponDamageCollider ||
                entry.type == BehaviorType::CrossbowBolt || entry.type == BehaviorType::FireballProjectile ||
                entry.type == BehaviorType::FadingParticle ||
                entry.type == BehaviorType::MetalPole || entry.type == BehaviorType::BearTrapAmmo ||
                entry.type == BehaviorType::Behavior_300 ||
                entry.type == BehaviorType::DestroyableBarrier ||
                entry.type == BehaviorType::DriftingParticle || entry.type == BehaviorType::SpawnInCrater ||
                entry.type == BehaviorType::FloatingDebris || entry.type == BehaviorType::LevelStartOverride ||
                entry.type == BehaviorType::RatAndSnake || entry.type == BehaviorType::Cockroach ||
                entry.type == BehaviorType::BatIndoor || entry.type == BehaviorType::Mummie ||
                entry.type == BehaviorType::MummieGuard ||
                entry.type == BehaviorType::Skeleton || entry.type == BehaviorType::WaterImpact ||
                entry.type == BehaviorType::SinkingObject ||
                entry.type == BehaviorType::FireParticle || entry.type == BehaviorType::BurningFireplace ||
                entry.type == BehaviorType::PulsatingLight ||
                entry.type == BehaviorType::IdleAnimation ||
                entry.type == BehaviorType::SmokeFading || entry.type == BehaviorType::Switch1 ||
                entry.type == BehaviorType::WalkablePlanks || entry.type == BehaviorType::WalkableDrawBridge ||
                entry.type == BehaviorType::Teleporter || entry.type == BehaviorType::ActivatableLadder ||
                entry.type == BehaviorType::ClimbableLadder || entry.type == BehaviorType::RoofTopThunderHandler3 ||
                entry.type == BehaviorType::SwitchGrouped || entry.type == BehaviorType::WaterDrop ||
                entry.type == BehaviorType::DisabledSentinel;
            Check(result.status == (implemented ?
                DispatchStatus::ExecutedCase : entry.hasSpecificCase ?
                DispatchStatus::UnimplementedHandler : DispatchStatus::NoSpecificCase), "Wrong case status.");
            if (!implemented)
            {
                Check(ObjectBytes(object) == before, "An unrecovered handler changed object state.");
            }
            Check(context.tickCount == 3 && context.behaviorDispatchEnabled,
                "A handler changed the dispatch inputs.");
            if (entry.hasSpecificCase) { ++specific; }
        }
        Check(specific == 187, "Expected 186 behavior cases plus the FFFF sentinel.");
        Check(BehaviorCatalog().size() == 190, "Unexpected catalog coverage.");
    }

    std::int16_t ReferenceSignedWord(std::int64_t value)
    {
        const auto bits = (value % 65536 + 65536) % 65536;
        return static_cast<std::int16_t>(bits >= 32768 ? bits - 65536 : bits);
    }

    std::int16_t ReferenceStep(std::int16_t current, std::int16_t target, std::int16_t step)
    {
        const auto difference = ReferenceSignedWord(static_cast<std::int64_t>(target) - current);
        const auto magnitude = ReferenceSignedWord(difference < 0 ? -static_cast<std::int64_t>(difference) : difference);
        return magnitude <= step ? difference :
            ReferenceSignedWord((difference < 0 ? -1LL : difference > 0 ? 1LL : 0LL) * step);
    }

    void TestLightingCache()
    {
        GameContext state;
        auto& cache = state.lightingCache;
        const auto empty = cache.contributions;
        Check(std::all_of(empty.begin(), empty.end(), [](const auto& column)
        {
            return std::all_of(column.begin(), column.end(), [](auto value) { return value == 0; });
        }), "Lighting must start cleared.");
        cache.Reset(1500, 2000);
        for (int dx = -10; dx <= 10; ++dx)
        {
            for (int dz = -10; dz <= 10; ++dz)
            {
                Check(cache.AddAtWorldXZ(static_cast<std::int16_t>(1500 + dx * 100),
                    static_cast<std::int16_t>(2000 + dz * 100), static_cast<std::int16_t>(dx * 21 + dz)),
                    "Every corner including all window edges must accept lighting.");
            }
        }
        for (int dx = -10; dx <= 10; ++dx)
        {
            for (int dz = -10; dz <= 10; ++dz)
            {
                Check(cache.contributions[dx + 10][dz + 10] == dx * 21 + dz,
                    "Lighting cache must retain native X-major, positive-Z indexing.");
            }
        }
        const auto filled = cache.contributions;
        Check(!cache.AddAtWorldXZ(2600, 2000, 1) && !cache.AddAtWorldXZ(400, 2000, 1) &&
            !cache.AddAtWorldXZ(1500, 3100, 1) && !cache.AddAtWorldXZ(1500, 900, 1) &&
            cache.contributions == filled, "Culled lights must not mutate the cache.");
        cache.Reset(1550, 2050, 0);
        Check(cache.contributions == empty && cache.centerX == 1550 && cache.centerZ == 2050 &&
            cache.AddAtWorldXZ(1500, 2000, 32767) && cache.AddAtWorldXZ(1500, 2000, 1) &&
            cache.contributions[10][10] == -32768,
            "Reset must clear stale lights; multiple sources must accumulate with native word wrapping.");
        Check(cache.AddAtWorldXZ(1500, 2000, -1) && cache.contributions[10][10] == 32767 &&
            !cache.AddAtWorldXZ(1550, 2050, 1),
            "Negative light contributions must wrap and source rounding must differ from center truncation.");

        for (const auto center : { -12899, -150, 0, 150, 12799 })
        {
            for (int axis = 0; axis < 2; ++axis)
            {
                for (int bits = 0; bits <= 65535; ++bits)
                {
                    const auto coordinate = ReferenceSignedWord(bits);
                    const auto shifted = ReferenceSignedWord(static_cast<int>(coordinate) + 50);
                    const int tile = shifted / 100;
                    const bool overflow = tile < -128 || tile > 127;
                    const int delta = tile - center / 100;
                    cache.Reset(static_cast<std::int16_t>(axis == 0 ? center : 0),
                        static_cast<std::int16_t>(axis == 1 ? center : 0), 7);
                    bool rejected = false;
                    bool applied = false;
                    try
                    {
                        applied = cache.AddAtWorldXZ(axis == 0 ? coordinate : 0,
                            axis == 1 ? coordinate : 0, 13);
                    }
                    catch (const std::overflow_error&) { rejected = true; }
                    Check(rejected == overflow, "Lighting division must reproduce signed-byte overflow boundaries.");
                    Check(applied == (!overflow && delta >= -7 && delta <= 7),
                        "Lighting culling differs from wrapped source rounding and center truncation.");
                    auto expected = empty;
                    if (applied) { expected[axis == 0 ? delta + 10 : 10][axis == 1 ? delta + 10 : 10] = 13; }
                    Check(cache.contributions == expected, "A lighting write must affect exactly one corner.");
                }
            }
        }
        cache.Reset(0, 0);
        cache.AddAtWorldXZ(0, 0, 17);
        const auto before = cache.contributions;
        for (const auto radius : { 11, 255 })
        {
            bool rejected = false;
            try { cache.Reset(100, 100, static_cast<std::uint8_t>(radius)); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && cache.centerX == 0 && cache.centerZ == 0 && cache.contributions == before,
                "Invalid radii must fail explicitly before changing cache state.");
        }
        for (const auto coordinate : { -12900, 12800 })
        {
            for (int axis = 0; axis < 2; ++axis)
            {
                bool rejected = false;
                try
                {
                    cache.Reset(static_cast<std::int16_t>(axis == 0 ? coordinate : 0),
                        static_cast<std::int16_t>(axis == 1 ? coordinate : 0));
                }
                catch (const std::overflow_error&) { rejected = true; }
                Check(rejected && cache.contributions == before, "Invalid centers must not clear existing light.");
            }
        }
        GameContext independent;
        Check(independent.lightingCache.contributions == empty, "Lighting must be owned by each game context.");
    }

    void TestLightSource()
    {
        struct Audio final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            unsigned int calls = 0;
            std::uint8_t lastSample = 0;
            std::uint8_t lastVolume = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            void StopAllSamples() override { playing.fill(false); }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                playing.at(channel) = true;
                lastSample = sample;
                lastVolume = volume;
                ++calls;
            }
        } audio;
        const auto nextRandom = [](std::uint32_t& seed, unsigned int range)
        {
            seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1u) % 4294967296ULL);
            return (seed >> 16) % range;
        };
        GameContext state;
        state.audioHandler = &audio;
        state.soundEffectsLoaded = true;
        state.soundEffects[4] = { 3, { 11, 22, 33, 0 } };
        auto& light = state.worldObjects[2];
        light.behavior = BehaviorType::LightSource;
        light.health = 1000;
        light.renderPlacementMode = 0xff;
        light.soundEffectId = 5;
        const auto empty = state.lightingCache.contributions;
        for (const int distance : { 0, 301 })
        {
            light.posX = static_cast<std::int16_t>(distance);
            const auto original = ObjectBytes(light);
            for (std::uint32_t seed = 0; seed <= 65535; ++seed)
            {
                for (const bool play : { false, true })
                {
                    state.lightingCache.Reset(0, 0);
                    state.randomSeed = seed;
                    state.field_DC34 = 65535;
                    audio.playing.fill(false);
                    const auto previousCalls = audio.calls;
                    auto expectedSeed = seed;
                    const auto perturbation = distance <= 300 ? nextRandom(expectedSeed, 0xefff) : 0;
                    const auto amount = static_cast<std::int16_t>(nextRandom(expectedSeed, 30) + 5);
                    const auto chance = nextRandom(expectedSeed, 100);
                    state.tickCount = static_cast<std::int16_t>(chance + (play ? 1 : 0));
                    const auto sample = play ? static_cast<std::uint8_t>(11 * (nextRandom(expectedSeed, 3) + 1)) : 0;
                    const auto result = DispatchObjectCase(light, state);
                    auto expectedCache = empty;
                    expectedCache[(distance + 50) / 100 + 10][10] = amount;
                    Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1488 &&
                        state.lightingCache.contributions == expectedCache && amount >= 5 && amount <= 34,
                        "LightSource must write exactly one flickering native corner in range 5..34.");
                    Check(state.randomSeed == expectedSeed &&
                        state.field_DC34 == static_cast<std::uint16_t>(65535u + perturbation),
                        "LightSource must preserve near/far RNG ordering, sound RNG, and DS:DC34 wrapping.");
                    Check(audio.calls == previousCalls + (play ? 1u : 0u) &&
                        (!play || (audio.lastSample == sample && audio.lastVolume == (distance == 0 ? 255 : 205) &&
                            state.soundChannelSources[3] == 2)),
                        "LightSource sound must use strict ticks > Random(100), high byte effect, volume and source slot.");
                    Check(ObjectBytes(light) == original, "LightSource must not mutate its own object record.");
                }
            }
        }
        // Include native signed-distance overflow without overflowing the cache's coordinate divisions.
        for (const int listenerX : { -300, -299, 0, 299, 300, 301, 32767 })
        {
            for (const int listenerZ : { 0, 1, 32767 })
            {
                for (const bool suppressed : { false, true })
                {
                    state.soundListenerX = static_cast<std::int16_t>(listenerX);
                    state.soundListenerZ = static_cast<std::int16_t>(listenerZ);
                    state.positionalSoundsSuppressed = suppressed;
                    state.tickCount = 0;
                    state.field_DC34 = 65535;
                    state.randomSeed = 0xffffffffu;
                    light.posX = 0;
                    state.lightingCache.Reset(0, 0);
                    const auto distance = suppressed ? 10000 :
                        ReferenceSignedWord(std::abs(listenerX) + std::abs(listenerZ));
                    auto expectedSeed = 0xffffffffu;
                    const auto perturbation = distance >= 0 && distance <= 300 ?
                        nextRandom(expectedSeed, 0xefff) : 0;
                    const auto amount = static_cast<std::int16_t>(nextRandom(expectedSeed, 30) + 5);
                    nextRandom(expectedSeed, 100);
                    DispatchObjectCase(light, state);
                    Check(state.GetSoundDistanceAt(0, 0) == distance && state.randomSeed == expectedSeed &&
                        state.field_DC34 == static_cast<std::uint16_t>(65535u + perturbation) &&
                        state.lightingCache.contributions[10][10] == amount,
                        "Near-light side effect must use signed wrapped Manhattan distance and suppression, not light culling.");
                }
            }
        }
        state.soundListenerX = 0;
        state.soundListenerZ = 0;
        state.positionalSoundsSuppressed = false;
        state.tickCount = 100;
        state.randomSeed = 0;
        light.renderPlacementMode = 0xff;
        light.soundEffectId = 0;
        auto expectedSeed = state.randomSeed;
        nextRandom(expectedSeed, 0xefff);
        nextRandom(expectedSeed, 30);
        nextRandom(expectedSeed, 100);
        const auto calls = audio.calls;
        DispatchObjectCase(light, state);
        Check(state.randomSeed == expectedSeed && audio.calls == calls,
            "Effect byte zero must mean no sound, without consuming sample-selection RNG.");

        light.renderPlacementMode = 0;
        light.soundEffectId = 5;
        state.lightingCache.Reset(0, 0);
        audio.playing.fill(false);
        for (int i = 0; i < 3; ++i) { DispatchObjectCase(light, state); }
        Check(audio.calls == calls + 2, "A light sound must respect the two-instance source limit.");

        state = {};
        state.randomSeed = 0x12345678u;
        state.tickCount = 0;
        for (const auto slot : { 2, 3, 164 })
        {
            auto& source = state.worldObjects[slot];
            source.health = 1000;
            source.behavior = BehaviorType::LightSource;
            source.posX = slot == 164 ? 1100 : 0;
        }
        for (int frame = 0; frame < 2; ++frame)
        {
            state.lightingCache.Reset(0, 0);
            expectedSeed = state.randomSeed;
            int expectedLight = 0;
            for (int source = 0; source < 3; ++source)
            {
                if (source < 2) { nextRandom(expectedSeed, 0xefff); }
                const auto amount = nextRandom(expectedSeed, 30) + 5;
                if (source < 2) { expectedLight += amount; }
                nextRandom(expectedSeed, 100);
            }
            const auto results = UpdateObjects(state);
            auto expectedCache = empty;
            expectedCache[10][10] = static_cast<std::int16_t>(expectedLight);
            Check(state.lightingCache.contributions == expectedCache && state.randomSeed == expectedSeed &&
                results[0].status == DispatchStatus::ExecutedCase &&
                results[1].status == DispatchStatus::ExecutedCase &&
                results[162].status == DispatchStatus::ExecutedCase,
                "World updates must accumulate all lights once, preserve sequential RNG, cull far lights, and reset between frames.");
        }
        const auto cache = state.lightingCache.contributions;
        const auto seed = state.randomSeed;
        const auto accumulator = state.field_DC34;
        state.behaviorDispatchEnabled = false;
        UpdateObjects(state);
        Check(state.lightingCache.contributions == cache && state.randomSeed == seed && state.field_DC34 == accumulator,
            "Disabled dispatch must skip all light side effects without clearing existing contributions.");
        state.behaviorDispatchEnabled = true;
        for (const auto slot : { 2, 3, 164 }) { state.worldObjects[slot].health = 0; }
        UpdateObjects(state);
        Check(state.randomSeed == seed, "Inactive light sources must not consume RNG.");
    }

    void TestPulsatingLight()
    {
        const auto nextRandom = [](std::uint32_t& seed, unsigned int range)
        {
            seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(seed) * 134775813u + 1u);
            return (seed >> 16) % range;
        };
        const auto fixedProduct = [](int amplitude, std::int32_t sine)
        {
            const auto product = amplitude * sine;
            return product >= 0 ? product / 65536 : -((-product + 65535) / 65536);
        };
        GameContext state;
        state.field_DC34 = 0xabcd;
        const std::array<std::int16_t, 6> ticks{ 0, 1, 2, 127, 256, 32767 };
        const std::array<std::int16_t, 4> positions{ 0, 100, 1100, -1100 };
        unsigned int targetChanges = 0;
        for (unsigned int sample = 0; sample < 65536; ++sample)
        {
            std::array<std::uint16_t, 27> words;
            for (std::size_t i = 0; i < words.size(); ++i)
            {
                words[i] = static_cast<std::uint16_t>(sample * 257u + i * 977u);
            }
            RawGameObject object;
            std::memcpy(&object, words.data(), sizeof(object));
            object.behavior = BehaviorType::PulsatingLight;
            object.health = 1000;
            object.posX = positions[sample % positions.size()];
            object.posZ = 0;
            object.generalPurpose2 = static_cast<std::uint16_t>(sample);
            object.velocityZ = ReferenceSignedWord(sample * 13u);
            object.velocityX = ReferenceSignedWord(sample * 17u);
            state.tickCount = ticks[sample % ticks.size()];
            state.randomSeed = sample * 65537u;
            state.lightingCache.Reset(0, 0);
            state.lightingCache.contributions[10][10] = 32760;
            auto expectedCache = state.lightingCache.contributions;
            auto expected = object;
            const auto phase = static_cast<std::int64_t>(object.generalPurpose2) +
                static_cast<std::int64_t>(object.velocityZ) * state.tickCount;
            expected.generalPurpose2 = static_cast<std::uint16_t>((phase % 512 + 512) % 512);
            auto seed = state.randomSeed;
            if (nextRandom(seed, 15) == 0)
            {
                expected.velocityX = static_cast<std::int16_t>(8 + nextRandom(seed, 10));
                ++targetChanges;
            }
            expected.velocityZ = ReferenceSignedWord(expected.velocityZ +
                ReferenceStep(expected.velocityZ, expected.velocityX, state.tickCount));
            const auto sine = ClampedSineTable[expected.generalPurpose2];
            expected.generalPurpose3 = static_cast<std::uint16_t>(800 + fixedProduct(750, sine));
            const auto amount = 30 - fixedProduct(25, sine);
            Check(expected.generalPurpose3 >= 50 && expected.generalPurpose3 <= 1550 && amount >= 5 && amount <= 55,
                "PulsatingLight pulse bounds differ from native amplitudes.");
            const auto column = (object.posX + 50) / 100 + 10;
            if (column >= 0 && column <= 20)
            {
                auto& cell = expectedCache[static_cast<std::size_t>(column)][10];
                cell = ReferenceSignedWord(cell + amount);
            }
            const auto result = DispatchObjectCase(object, state);
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x19cd &&
                result.behavior == BehaviorType::PulsatingLight &&
                ObjectBytes(object) == ObjectBytes(expected) && state.randomSeed == seed &&
                state.lightingCache.contributions == expectedCache && state.field_DC34 == 0xabcd,
                "PulsatingLight record, RNG order, signed fixed-point arithmetic, or single-corner write differs.");
        }
        Check(targetChanges > 0 && targetChanges < 65536,
            "PulsatingLight tests must exercise both random-target branches.");

        state = {};
        state.tickCount = 0;
        for (int phase = 0; phase < 512; ++phase)
        {
            auto& object = state.worldObjects[2];
            object = {};
            object.behavior = BehaviorType::PulsatingLight;
            object.health = 1000;
            object.generalPurpose2 = static_cast<std::uint16_t>(phase);
            state.lightingCache.Reset(0, 0);
            const auto sine = ClampedSineTable[static_cast<std::size_t>(phase)];
            UpdateObjects(state);
            Check(object.generalPurpose2 == phase && object.generalPurpose3 == 800 + fixedProduct(750, sine) &&
                state.lightingCache.contributions[10][10] == 30 - fixedProduct(25, sine),
                "Zero-tick world updates must still refresh both pulsating outputs using the clamped sine table.");
        }
        auto& second = state.worldObjects[3];
        second = state.worldObjects[2];
        second.generalPurpose2 = 128;
        state.worldObjects[2].generalPurpose2 = 384;
        state.lightingCache.Reset(0, 0);
        auto seed = state.randomSeed;
        for (int source = 0; source < 2; ++source)
        {
            if (nextRandom(seed, 15) == 0) { nextRandom(seed, 10); }
        }
        UpdateObjects(state);
        Check(state.lightingCache.contributions[10][10] == 60 && state.randomSeed == seed,
            "Pulsating lights must accumulate without resetting the grid and share sequential RNG.");
        const auto cache = state.lightingCache.contributions;
        const auto first = ObjectBytes(state.worldObjects[2]);
        const auto other = ObjectBytes(second);
        state.behaviorDispatchEnabled = false;
        UpdateObjects(state);
        Check(state.lightingCache.contributions == cache && state.randomSeed == seed &&
            ObjectBytes(state.worldObjects[2]) == first && ObjectBytes(second) == other,
            "Disabled pulsating dispatch must preserve object, RNG, and existing light contributions.");
        state.behaviorDispatchEnabled = true;
        state.worldObjects[2].health = 0;
        second.health = -1;
        UpdateObjects(state);
        Check(state.lightingCache.contributions == cache && state.randomSeed == seed,
            "Inactive pulsating lights must not write lighting or consume RNG.");
    }

    void TestInputAbstraction()
    {
        struct Keys final : InputHandler
        {
            std::array<bool, 20> pressed{};
            bool IsKeyPressed(InputKey key) override { return pressed.at(static_cast<std::size_t>(key)); }
        } keys;
        constexpr std::array inputs{
            InputKey::ForwardKey, InputKey::BackwardKey, InputKey::StrafeLeftKey,
            InputKey::StrafeRightKey, InputKey::TurnLeftKey, InputKey::TurnRightKey, InputKey::InteractionKey,
            InputKey::AttackKey, InputKey::SelectDaggerKey, InputKey::SelectSwordKey,
            InputKey::JumpKey, InputKey::CrouchKey,
            InputKey::SelectCrossbowKey, InputKey::SelectFireballKey, InputKey::SelectStoneKey,
            InputKey::SelectPlaceableKey, InputKey::SelectElectricityKey, InputKey::SelectShovelKey,
            InputKey::SelectScytheKey, InputKey::SelectLentilsKey
        };
        InputHandler& input = keys;
        for (unsigned mask = 0; mask < (1u << inputs.size()); ++mask)
        {
            for (std::size_t i = 0; i < inputs.size(); ++i) { keys.pressed[i] = (mask & (1u << i)) != 0; }
            for (std::size_t i = 0; i < inputs.size(); ++i)
            {
                Check(input.IsKeyPressed(inputs[i]) == ((mask & (1u << i)) != 0),
                    "Abstract input keys must remain independent through the virtual interface.");
            }
        }
    }

    void TestFlies()
    {
        struct Audio final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            unsigned int calls = 0;
            std::uint8_t lastSample = 0;
            std::uint8_t lastVolume = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            void StopAllSamples() override { playing.fill(false); }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                playing.at(channel) = true;
                lastSample = sample;
                lastVolume = volume;
                ++calls;
            }
        } audio;
        const auto nextRandom = [](std::uint32_t& seed, unsigned int range)
        {
            seed = static_cast<std::uint32_t>((static_cast<std::uint64_t>(seed) * 134775813u + 1u) % 4294967296ULL);
            return (seed >> 16) % range;
        };
        const auto reference = [&](RawGameObject object, std::int16_t ticks, std::uint32_t& seed)
        {
            if (object.displayType == DisplayType::VectorBrightness2) { object.health = 0; }
            const auto increment = (static_cast<std::int64_t>(object.itemValue) + nextRandom(seed, 5)) * ticks;
            object.blendWeightOrSpriteOpacity = ReferenceSignedWord(object.blendWeightOrSpriteOpacity + increment);
            object.winX = static_cast<std::int16_t>((static_cast<std::uint16_t>(object.winX) + 5u) % 512);
            object.winY = static_cast<std::int16_t>((object.winX + 3) % 512);
            object.winZ = static_cast<std::int16_t>((object.winX + 7) % 512);
            while (object.blendWeightOrSpriteOpacity > 100)
            {
                object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(object.blendWeightOrSpriteOpacity - 100);
                object.animFrame = object.modelFrame2;
                auto frame = static_cast<std::uint8_t>(nextRandom(seed, 3) + 1);
                while (frame == object.animFrame) { frame = static_cast<std::uint8_t>(nextRandom(seed, 3) + 1); }
                object.modelFrame2 = frame;
            }
            return object;
        };
        RawGameObject initial;
        initial.behavior = BehaviorType::Flies;
        initial.health = 1000;
        initial.modelId = 37;
        initial.modelAnim = 88;
        initial.modelFrame2 = 1;
        initial.animFrame = 9;
        initial.itemValue = 40;
        initial.blendWeightOrSpriteOpacity = 90;
        initial.winX = -4;
        initial.winY = -200;
        initial.winZ = 900;
        initial.posY = 5;
        initial.velocityX = 17;
        initial.velocityY = -23;
        initial.velocityZ = 81;
        initial.renderPlacementMode = 0xcd;
        initial.renderSortKey = -123;
        initial.soundEffectId = 5;
        initial.generalPurpose1 = 0x1234;
        initial.behaviorParameter = 0xbe;
        initial.spriteScale = 0xba;
        GameContext state;
        state.audioHandler = &audio;
        state.soundEffectsLoaded = true;
        state.soundEffects[73] = { 3, { 11, 22, 33, 0 } };
        auto& flies = state.worldObjects[2];
        const auto checkStep = [&](const RawGameObject& before, std::int16_t ticks,
            std::uint32_t seed, std::uint8_t volume)
        {
            flies = before;
            state.tickCount = ticks;
            state.randomSeed = seed;
            auto expectedSeed = seed;
            const auto expected = reference(before, ticks, expectedSeed);
            std::uint8_t sample = 0;
            if (state.soundEffectsLoaded)
            {
                sample = static_cast<std::uint8_t>(11 * (nextRandom(expectedSeed, 3) + 1));
            }
            audio.playing.fill(false);
            const auto calls = audio.calls;
            const auto result = DispatchObjectCase(flies, state);
            const bool audible = state.audioHandler && state.soundEnabled && volume > 0;
            Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1690 &&
                result.behavior == BehaviorType::Flies && ObjectBytes(flies) == ObjectBytes(expected),
                "Flies must match native signed phase arithmetic, frame rejection, angles, and field isolation.");
            Check(state.randomSeed == expectedSeed, "Flies must preserve animation/frame/sound RNG ordering.");
            Check(audio.calls == calls + (audible ? 1u : 0u) &&
                (!audible || (audio.lastSample == sample && audio.lastVolume == volume &&
                    state.soundChannelSources[3] == 2)),
                "Flies must request fixed effect 74 with its world source, attenuation, and variant.");
        };
        for (std::uint32_t seed = 0; seed <= 65535; ++seed)
        {
            for (const int frame : { 0, 1, 2, 3, 255 })
            {
                auto object = initial;
                object.modelFrame2 = static_cast<std::uint8_t>(frame);
                checkStep(object, 3, seed, 255);
            }
        }
        state.audioHandler = nullptr;
        state.soundEffectsLoaded = false;
        for (int bits = 0; bits <= 65535; ++bits)
        {
            auto object = initial;
            object.itemValue = 0;
            object.blendWeightOrSpriteOpacity = ReferenceSignedWord(bits);
            checkStep(object, 0, 0xffffffffu, 255);
            object.itemValue = ReferenceSignedWord(bits);
            object.blendWeightOrSpriteOpacity = -32760;
            checkStep(object, 32767, 0x80000000u, 255);
            object.itemValue = 0;
            object.blendWeightOrSpriteOpacity = 100;
            object.winX = ReferenceSignedWord(bits);
            checkStep(object, 0, 0, 255);
        }
        for (const int phase : { -32768, -1, 0, 99, 100, 101, 32767 })
        {
            for (const int rate : { -32768, -1, 0, 32766, 32767 })
            {
                for (const int ticks : { 0, 1, 3, 32767 })
                {
                    auto object = initial;
                    object.itemValue = static_cast<std::int16_t>(rate);
                    object.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(phase);
                    checkStep(object, static_cast<std::int16_t>(ticks), 0x12345678u, 255);
                }
            }
        }
        for (int display = 0; display <= 255; ++display)
        {
            auto object = initial;
            object.displayType = static_cast<DisplayType>(display);
            checkStep(object, 0, 0, 255);
        }
        state.soundEffectsLoaded = true;
        // A loaded table still consumes sound-variant RNG when there is no audio backend.
        checkStep(initial, 3, 0, 255);
        state.audioHandler = &audio;
        for (const int x : { -32768, -32767, -1, 0, 100, 101, 300, 354, 355, 356, 10000, 16384, 32767 })
        {
            for (const int z : { -32768, 0, 1, 32767 })
            {
                for (const bool suppressed : { false, true })
                {
                    state.soundListenerX = static_cast<std::int16_t>(x);
                    state.soundListenerZ = static_cast<std::int16_t>(z);
                    state.positionalSoundsSuppressed = suppressed;
                    const auto distance = suppressed ? 10000 : ReferenceSignedWord(
                        ReferenceSignedWord(std::abs(x)) + ReferenceSignedWord(std::abs(z)));
                    const auto doubled = ReferenceSignedWord(2LL * distance);
                    const auto volume = static_cast<std::uint8_t>(
                        std::clamp(355 - std::abs(static_cast<int>(doubled)) / 2, 0, 255));
                    checkStep(initial, 3, 0, volume);
                }
            }
        }
        state.soundListenerX = state.soundListenerZ = 0;
        state.positionalSoundsSuppressed = false;
        state.soundEnabled = false;
        checkStep(initial, 3, 0, 255);
        state.soundEnabled = true;
        state.tickCount = 0;
        flies = initial;
        audio.playing.fill(false);
        state.randomSeed = 0;
        auto expectedSeed = state.randomSeed;
        auto expected = flies;
        const auto calls = audio.calls;
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            expected = reference(expected, state.tickCount, expectedSeed);
            nextRandom(expectedSeed, 3);
            DispatchObjectCase(flies, state);
            Check(audio.calls == calls + 1 && state.randomSeed == expectedSeed &&
                ObjectBytes(flies) == ObjectBytes(expected),
                "Flies buzz must enforce a one-instance source limit after selecting its sample.");
        }
        flies = initial;
        flies.displayType = DisplayType::FirstVectorBurning;
        state.frameTickCount = 1;
        state.randomSeed = 2;
        audio.playing.fill(false);
        expectedSeed = state.randomSeed;
        Check(nextRandom(expectedSeed, 2) == 1, "Burning fixture must reach the burned display state.");
        expected = initial;
        expected.displayType = DisplayType::VectorBrightness2;
        expected = reference(expected, state.tickCount, expectedSeed);
        nextRandom(expectedSeed, 3);
        const auto beforeBurnedSound = audio.calls;
        Check(UpdateObject(flies, state).status == DispatchStatus::ExecutedCase &&
            flies.health == 0 && ObjectBytes(flies) == ObjectBytes(expected) &&
            state.randomSeed == expectedSeed && audio.calls == beforeBurnedSound + 1,
            "The shared burning prefix must feed Flies' removal state without skipping its last update.");
        const auto inactive = ObjectBytes(flies);
        Check(UpdateObject(flies, state).status == DispatchStatus::InactiveObject &&
            ObjectBytes(flies) == inactive && state.randomSeed == expectedSeed,
            "Burned flies must be inactive on subsequent world updates.");
        flies = initial;
        state.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(flies, state).status == DispatchStatus::DispatchDisabled &&
            ObjectBytes(flies) == ObjectBytes(initial) && state.randomSeed == expectedSeed,
            "Disabled dispatch must skip flies animation, orientation, RNG, and sound.");
    }

    RawGameObject MakeTestDoor()
    {
        RawGameObject door;
        door.behavior = BehaviorType::Door;
        door.collisionType = CollisionType::Door;
        door.health = 1000;
        door.posX = 150;
        door.posZ = 4050;
        door.collisionRadiusY = 10;
        door.collisionMask = 1;
        door.generalPurpose1 = 10;
        door.modelFrame2 = 1;
        door.behaviorParameter = 0;
        door.spriteScale = 0xab;
        return door;
    }

    void TestUserMessages()
    {
        MessageContext state;
        GameContext& core = state;
        for (int bits = 0; bits <= 65535; ++bits)
        {
            const auto duration = ReferenceSignedWord(bits);
            core.PromptUserMessage("test", duration);
            Check(state.userMessage == "test" && state.userMessageTicks == duration,
                "The prompt hook must deliver the original duration without owning its countdown.");
        }
        for (int byte = 0; byte <= 255; ++byte)
        {
            core.PromptUserMessage(std::string(151, static_cast<char>(byte)), 12);
            Check(state.userMessage == std::string(151, static_cast<char>(byte)),
                "The prompt hook must deliver original bytes without storing, truncating or transcoding them.");
        }
        core.PromptUserMessage("replacement", 10);
        const auto calls = state.promptCalls;
        state.tickCount = 1;
        state.frameTickCount = 3;
        RawGameObject object;
        object.health = 1000;
        DispatchObjectCase(object, state);
        UpdateObjects(state);
        Check(state.userMessageTicks == 10 && state.promptCalls == calls,
            "Object dispatch and world updates must leave app-owned prompt state alone.");
    }

    void TestDoorCollisionMap()
    {
        auto level = CreateDummyLevel();
        GameContext state;
        state.collisionMap = level.collisionMap.get();
        auto& map = *state.collisionMap;
        for (const int width : { 0, 1, 199, 319, 65535 })
        {
            for (const int x : { -32768, -32767, -100, 0, 37, 62, 87, 112, 113, 136, 137, 161, 162, 32767 })
            {
                for (const int z : { -32768, -32767, -100, 0, 37, 38, 62, 63, 4050, 4088, 32767 })
                {
                    map = {};
                    map.widthMinusOne = static_cast<std::uint16_t>(width);
                    std::fill(std::begin(map.data), std::end(map.data), std::uint8_t{ 0xa5 });
                    std::array<unsigned char, sizeof(CollisionMap)> expected;
                    std::memcpy(expected.data(), &map, sizeof(map));
                    const int column = ReferenceSignedWord(x - 137) / 25;
                    const int center = 162 - ReferenceSignedWord(z - 38) / 25;
                    bool invalid = false;
                    for (int offset = -1; offset <= 1; ++offset)
                    {
                        // Header writes can change the stride used by the following row.
                        const int stride = (expected[0] + 256 * expected[1] + 1) % 65536;
                        const auto row = ReferenceSignedWord(center + offset);
                        std::array<std::size_t, 3> addresses{};
                        for (int pixel = 0; pixel < 3; ++pixel)
                        {
                            const std::int64_t address = 4LL + row * stride + column + pixel;
                            addresses[pixel] = static_cast<std::size_t>((address % 65536 + 65536) % 65536);
                            invalid = invalid || addresses[pixel] >= expected.size();
                        }
                        if (invalid) { break; }
                        for (const auto address : addresses) { expected[address] = 0x7c; }
                    }
                    bool rejected = false;
                    try
                    {
                        state.SetDoorCollisionMapAtWorldXZ(
                            static_cast<std::int16_t>(x), static_cast<std::int16_t>(z), 0xab7c);
                    }
                    catch (const std::out_of_range&) { rejected = true; }
                    Check(rejected == invalid && std::memcmp(&map, expected.data(), sizeof(map)) == 0,
                        "Door footprints must preserve division, row order, low bytes, and wrapped addresses.");
                }
            }
        }
        state.collisionMap = nullptr;
        bool rejected = false;
        try { state.SetDoorCollisionMapAtWorldXZ(150, 4050, 0); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Door stamping without collision-map memory must fail explicitly.");
    }

    void TestDoorUnlocking()
    {
        MessageContext state;
        auto door = MakeTestDoor();
        const auto original = ObjectBytes(door);
        Check(HandlePlayerObjectInteraction(door, state).status == InteractionStatus::Applied &&
            door.generalPurpose2 == 3 && door.itemValue == 10 && door.displayType == DisplayType::SpriteNormal,
            "Unlocked door interaction must start opening and refresh the closing delay.");
        for (std::size_t slot = 0; slot < state.inventory.size(); ++slot)
        {
            state = {};
            door = MakeTestDoor();
            door.doorLockOrGroupBitmask = 7;
            door.modelFrame2 = 0;
            for (auto& key : state.inventory)
            {
                key.health = 1000;
                key.collisionType = CollisionType::Key;
                key.itemValue = 8;
                key.renderPlacementMode = 20;
                key.soundEffectId = 29;
            }
            state.inventory[slot].itemValue = 7;
            const auto before = state.inventory;
            Check(HandlePlayerObjectInteraction(door, state).status == InteractionStatus::Applied &&
                door.doorLockOrGroupBitmask == 0 && door.generalPurpose2 == 3 &&
                door.itemValue == 10 && door.displayType == DisplayType::Hidden,
                "A matching key in any hand/backpack slot must unlock and activate the door.");
            for (std::size_t index = 0; index < before.size(); ++index)
            {
                auto expected = before[index];
                if (index == slot) { expected.health = 0; }
                Check(ObjectBytes(state.inventory[index]) == ObjectBytes(expected),
                    "Key consumption must only zero its health, with no compaction or other-byte changes.");
            }
        }
        state = {};
        door = MakeTestDoor();
        door.doorLockOrGroupBitmask = 7;
        state.inventory[0].health = state.inventory[13].health = 1;
        state.inventory[0].collisionType = state.inventory[13].collisionType = CollisionType::Key;
        state.inventory[0].itemValue = state.inventory[13].itemValue = 7;
        HandlePlayerObjectInteraction(door, state);
        Check(state.inventory[0].health == 0 && state.inventory[13].health == 1,
            "The first matching live key must win over later matching keys.");
        for (const int health : { -1, 0, 1, 999, 1000 })
        {
            for (const auto type : { CollisionType::Key, CollisionType::Gold })
            {
                state = {};
                door = MakeTestDoor();
                door.doorLockOrGroupBitmask = 7;
                auto& key = state.inventory[4];
                key.health = static_cast<std::int16_t>(health);
                key.itemValue = 7;
                key.collisionType = type;
                const bool valid = health > 0 && type == CollisionType::Key;
                Check((HandlePlayerObjectInteraction(door, state).status == InteractionStatus::Applied) == valid,
                    "Unlocking must accept exactly the live key records.");
                Check(valid || (state.userMessage == "abgeschlossen" && state.userMessageTicks == 60),
                    "No eligible key must generate the locked feedback.");
            }
        }
        state = {};
        door = MakeTestDoor();
        door.doorLockOrGroupBitmask = 7;
        auto& key = state.inventory[0];
        key.health = 1000;
        key.collisionType = CollisionType::Key;
        key.itemValue = 8;
        state.PromptUserMessage("existing prompt", 17);
        const auto locked = ObjectBytes(door);
        Check(HandlePlayerObjectInteraction(door, state).status == InteractionStatus::NoEffect &&
            state.userMessage == "Falscher Schl\x81ssel" && state.userMessageTicks == 90 &&
            key.health == 1000 && ObjectBytes(door) == locked,
            "Wrong keys must overwrite active prompts without consuming keys or changing the door.");
        auto& lastKey = state.inventory[13];
        lastKey = key;
        lastKey.itemValue = 0;
        state.PromptUserMessage("existing prompt", 17);
        HandlePlayerObjectInteraction(door, state);
        Check(state.userMessage == "existing prompt" && state.userMessageTicks == 17,
            "A last eligible zero-ID key must take the no-key path and respect active feedback.");
        state.userMessageTicks = -3;
        HandlePlayerObjectInteraction(door, state);
        Check(state.userMessage == "abgeschlossen" && state.userMessageTicks == 60,
            "Expired locked feedback must be replaced with a new 60-tick prompt.");
        door.doorLockOrGroupBitmask = -7;
        key.itemValue = -7;
        state.PromptUserMessage("", 0);
        HandlePlayerObjectInteraction(door, state);
        Check(door.doorLockOrGroupBitmask == -7 && key.health == 1000 &&
            state.userMessage == "abgeschlossen", "Negative locks must reject even matching negative key IDs.");
        for (const int health : { 999, 1001 })
        {
            door = MakeTestDoor();
            door.health = static_cast<std::int16_t>(health);
            Check(HandlePlayerObjectInteraction(door, state).status == InteractionStatus::NoEffect &&
                door.generalPurpose2 == 0, "Manual door activation must require health exactly 1000.");
        }
        door = MakeTestDoor();
        door.health = 999;
        door.pickupType = PickupType::Type_1;
        state = {};
        const auto pickable = ObjectBytes(door);
        const auto pickup = HandlePlayerObjectInteraction(door, state);
        Check(pickup.status == InteractionStatus::Applied && pickup.inventorySlot == 1 &&
            door.health == 0 && ObjectBytes(state.inventory[0]) == pickable,
            "Nonstandard door health must retain the original generic pickup tail when flagged pickable.");
        door = MakeTestDoor();
        door.generalPurpose2 = 65533;
        state.PromptUserMessage("", 0);
        const auto moving = ObjectBytes(door);
        Check(HandlePlayerObjectInteraction(door, state).status == InteractionStatus::NoEffect &&
            ObjectBytes(door) == moving && state.userMessage.empty(),
            "Manual interaction with a moving door must have no effect or feedback.");
        door = MakeTestDoor();
        Check(ObjectBytes(door) == original, "Door fixtures must start from independent state.");
        door.blendWeightOrSpriteOpacity = 100;
        HandlePlayerObjectInteraction(door, state);
        Check(door.generalPurpose2 == 0 && door.itemValue == 10,
            "An already open unlocked door must refresh its delay without restarting movement.");
    }

    void TestDoorUpdates()
    {
        auto level = CreateDummyLevel();
        GameContext state;
        state.collisionMap = level.collisionMap.get();
        const auto checkStep = [&](std::int16_t phase, std::uint16_t rate, std::uint16_t delay,
            std::int16_t timer, std::uint16_t occupancy, std::int16_t ticks)
        {
            auto door = MakeTestDoor();
            door.blendWeightOrSpriteOpacity = phase;
            door.generalPurpose2 = rate;
            door.generalPurpose1 = delay;
            door.itemValue = timer;
            door.behaviorParameter = static_cast<std::uint8_t>(occupancy);
            door.spriteScale = 0xab;
            door.velocityY = 0x127c;
            state.tickCount = ticks;
            auto expected = door;
            if (ReferenceSignedWord(delay) > 0)
            {
                if (timer > 0) { timer = ReferenceSignedWord(timer - ticks); }
                if (occupancy != 0 && phase > 0) { timer = ReferenceSignedWord(delay); rate = 3; }
                if (timer < 1 && phase == 100) { rate = 65533; }
            }
            phase = ReferenceSignedWord(phase + static_cast<std::int64_t>(rate) * ticks);
            expected.collisionRadiusXZ = 54;
            if (phase > 100)
            {
                phase = 100;
                if (delay == 0)
                {
                    expected.behavior = BehaviorType::None;
                    expected.collisionRadiusXZ = 0;
                    expected.collisionMask = 0;
                }
            }
            phase = std::max<std::int16_t>(phase, 0);
            if (phase == 0 || phase == 100) { rate = 0; }
            expected.itemValue = timer;
            expected.generalPurpose2 = rate;
            expected.blendWeightOrSpriteOpacity = phase;
            expected.behaviorParameter = 0;
            expected.spriteScale = 0xab;
            Check(Function_Door(door, state).status == DispatchStatus::ExecutedCase &&
                ObjectBytes(door) == ObjectBytes(expected),
                "Door updates must preserve original ordering, signed guards, word wraps, and endpoint effects.");
            Check(state.GetCollisionMapPixelAtWorldXZ(150, 4050) == (phase == 100 ? 0x7c : 0x38),
                "Only a fully open door may stamp its saved passability byte.");
        };
        for (int bits = 0; bits <= 65535; ++bits)
        {
            for (const int ticks : { 0, 1, 3 })
            {
                for (const int rate : { 0, 3, 65533 })
                {
                    checkStep(ReferenceSignedWord(bits), static_cast<std::uint16_t>(rate),
                        10, 4, 0, static_cast<std::int16_t>(ticks));
                }
            }
        }
        for (const int phase : { -32768, -1, 0, 1, 97, 99, 100, 101, 32767 })
        {
            for (const int rate : { 0, 3, 32767, 32768, 65533, 65535 })
            {
                for (const int delay : { 0, 10, 32768, 65535 })
                {
                    for (const int ticks : { 0, 1, 3, 32767 })
                    {
                        for (const int occupancy : { 0, 1, 164 })
                        {
                            checkStep(static_cast<std::int16_t>(phase), static_cast<std::uint16_t>(rate),
                                static_cast<std::uint16_t>(delay), 1, static_cast<std::uint16_t>(occupancy),
                                static_cast<std::int16_t>(ticks));
                        }
                    }
                }
            }
        }
        state.tickCount = 1;
        auto& door = state.worldObjects[2];
        door = MakeTestDoor();
        door.health = 1001;
        door.groupIndexOrLock = 4;
        door.modelFrame2 = 0;
        state.switchGroupMasks[4] = 0;
        door.doorLockOrGroupBitmask = -7;
        Function_Door(door, state);
        Check(door.health == 1001 && door.collisionRadiusXZ == 10 && door.generalPurpose2 == 3 &&
            door.blendWeightOrSpriteOpacity == 3 && door.itemValue == 9 &&
            door.displayType == DisplayType::Hidden,
            "Group activation must allow negative locks and damaged doors without resetting their health.");
        door = MakeTestDoor();
        door.groupIndexOrLock = 4;
        door.doorLockOrGroupBitmask = 7;
        Function_Door(door, state);
        Check(door.generalPurpose2 == 0 && door.blendWeightOrSpriteOpacity == 0,
            "Positive key locks must prevent group activation.");
        state.switchGroupMasks[4] = 0xff;
        door = MakeTestDoor();
        door.blendWeightOrSpriteOpacity = 97;
        door.generalPurpose1 = 0;
        door.generalPurpose2 = 3;
        Function_Door(door, state);
        Check(door.behavior == BehaviorType::Door && door.blendWeightOrSpriteOpacity == 100 &&
            door.collisionRadiusXZ == 54 && door.collisionMask == 1,
            "Landing exactly on 100 must not disable a one-shot door.");
        door.blendWeightOrSpriteOpacity = 99;
        door.generalPurpose2 = 3;
        Function_Door(door, state);
        Check(door.behavior == BehaviorType::None && door.blendWeightOrSpriteOpacity == 100 &&
            door.collisionRadiusXZ == 0 && door.collisionMask == 0,
            "Overshooting 100 must disable a one-shot door and its object collision.");

        door = MakeTestDoor();
        door.blendWeightOrSpriteOpacity = 50;
        door.generalPurpose2 = 65533;
        auto& blocker = state.worldObjects[3];
        blocker = door;
        blocker.collisionRadiusXZ = blocker.collisionRadiusY = 1;
        Function_Door(door, state);
        Check(door.blendWeightOrSpriteOpacity == 47 && door.behaviorParameter == 3 && door.spriteScale == 0xab,
            "The final overlap scan must store the blocker slot without changing the marker high byte.");
        Function_Door(door, state);
        Check(door.blendWeightOrSpriteOpacity == 50 && door.generalPurpose2 == 3 && door.itemValue == 10,
            "An occupied closing door must reverse and refresh its automatic-close timer.");
        blocker.health = 0;
        auto& player = state.worldObjects[1];
        player = door;
        player.collisionRadiusXZ = player.collisionRadiusY = 1;
        door.behaviorParameter = 0;
        door.spriteScale = 0xab;
        door.generalPurpose2 = 0;
        door.itemValue = 7;
        struct ContactInput final : InputHandler
        {
            int calls = 0;
            bool IsKeyPressed(InputKey key) override
            {
                Check(key == InputKey::InteractionKey, "Door contact must only query the interaction key.");
                ++calls;
                return false;
            }
        } input;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::NoEffect &&
            door.behaviorParameter == 1 && door.spriteScale == 0xab && door.generalPurpose2 == 0 && input.calls == 1,
            "Player contact without input must mark occupancy without activating the door.");
        Function_Door(door, state);
        Check(door.generalPurpose2 == 3 && door.itemValue == 10 &&
            door.behaviorParameter == 0 && door.spriteScale == 0xab,
            "The player marker must reopen the door, then clear because the object scan excludes the player.");
        for (const std::int16_t group : { std::int16_t{ -1 }, std::int16_t{ 5 } })
        {
            door.groupIndexOrLock = group;
            bool rejected = false;
            try { Function_Door(door, state); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected, "Invalid door group indices must fail explicitly.");
        }
        door.groupIndexOrLock = 0;
        door.collisionType = CollisionType::None;
        state.switchGroupMasks[0] = 0;
        bool rejected = false;
        try { Function_Door(door, state); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Unrecovered general activation must fail explicitly, not pretend to activate a door.");
    }

    void TestDoorSounds()
    {
        struct DoorAudio final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            unsigned int starts = 0;
            std::uint8_t lastSample = 0;
            std::uint8_t lastVolume = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            void StopAllSamples() override { playing.fill(false); }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                playing.at(channel) = true;
                lastSample = sample;
                lastVolume = volume;
                ++starts;
            }
        } audio;
        auto level = CreateDummyLevel();
        GameContext state;
        state.collisionMap = level.collisionMap.get();
        state.audioHandler = &audio;
        state.soundEffectsLoaded = true;
        state.soundEffects[4] = { 1, { 11, 0, 0, 0 } };
        state.soundEffects[7] = { 1, { 22, 0, 0, 0 } };
        auto& door = state.worldObjects[2];
        door = MakeTestDoor();
        door.soundEffectId = 5;
        state.soundListenerX = door.posX;
        state.soundListenerZ = door.posZ;
        HandlePlayerObjectInteraction(door, state);
        Check(audio.starts == 1 && audio.lastSample == 11 && audio.lastVolume == 255 &&
            state.soundChannelSources[3] == 2, "Opening must use the configured sound and actual world slot.");
        door.generalPurpose2 = 0;
        HandlePlayerObjectInteraction(door, state);
        Check(audio.starts == 1, "Opening sound must enforce its one-instance-per-source limit.");
        audio.playing.fill(false);
        door.blendWeightOrSpriteOpacity = 100;
        door.itemValue = 0;
        door.generalPurpose2 = 0;
        Function_Door(door, state);
        Check(audio.starts == 2 && audio.lastSample == 11 && door.blendWeightOrSpriteOpacity == 97,
            "Automatic closing must request the configured sound before movement.");
        door.blendWeightOrSpriteOpacity = 1;
        Function_Door(door, state);
        Check(audio.starts == 3 && audio.lastSample == 22 && state.soundChannelSources[2] == 2 &&
            door.blendWeightOrSpriteOpacity == 0,
            "Closing overshoot must play effect 8 with a two-instance source limit.");
        audio.playing.fill(false);
        door = MakeTestDoor();
        state.switchGroupMasks[0] = 0;
        door.soundEffectId = 5;
        Function_Door(door, state);
        Check(audio.starts == 4 && audio.lastSample == 11,
            "Switch-group door activation must request the same opening sound.");
        state.switchGroupMasks[0] = 0xff;
        door = MakeTestDoor();
        const auto seed = state.randomSeed;
        HandlePlayerObjectInteraction(door, state);
        Check(audio.starts == 4 && state.randomSeed == seed,
            "Configured sound zero must be silent without entering variant selection.");
        door.blendWeightOrSpriteOpacity = 3;
        door.generalPurpose2 = 65533;
        Function_Door(door, state);
        Check(audio.starts == 4 && door.blendWeightOrSpriteOpacity == 0 && state.randomSeed == seed,
            "Landing exactly on closed must stop without playing the overshoot-only closing sound.");
    }

    void TestObjectAllocation()
    {
        GameContext state;
        const auto fill = [&]
        {
            for (auto& object : state.worldObjects)
            {
                std::memset(&object, 0x5a, sizeof(object));
                object.health = 1000;
            }
        };
        const auto empty = ObjectBytes(RawGameObject{});
        for (std::uint16_t slot = 2; slot <= 164; ++slot)
        {
            fill();
            state.worldObjects[slot].health = 0;
            const auto before = state.worldObjects;
            Check(state.FindFreeObjectIndex() == slot, "Free allocation must scan all slots 2..164 in order.");
            for (std::size_t i = 0; i < before.size(); ++i)
            {
                Check(ObjectBytes(state.worldObjects[i]) == (i == slot ? empty : ObjectBytes(before[i])),
                    "Allocation must clear exactly the selected 54-byte record.");
            }
        }
        fill();
        state.worldObjects[2].health = -1;
        Check(state.FindFreeObjectIndex() == 165 && ObjectBytes(state.worldObjects[165]) == empty &&
            state.worldObjects[2].health == -1,
            "Full allocation must clear/return reserved slot 165; negative health is not a free slot.");
        Check(state.FindFreeObjectIndex() == 165, "Allocation does not reserve slots by setting health.");
        fill();
        state.worldObjects[3].health = 2;
        state.worldObjects[4].health = 2;
        Check(state.FindFreeOrLowestHealthObjectIndex() == 3 &&
            ObjectBytes(state.worldObjects[3]) == empty && state.worldObjects[4].health == 2,
            "Reclaiming allocation must use the first strict minimum below 1000.");
        fill();
        state.worldObjects[2].health = -20;
        state.worldObjects[164].health = 0;
        Check(state.FindFreeOrLowestHealthObjectIndex() == 164,
            "A zero-health slot must beat an earlier negative-health minimum.");
        fill();
        state.worldObjects[164].health = -1;
        Check(state.FindFreeOrLowestHealthObjectIndex() == 164,
            "Reclaiming allocation must include negative health and the last ordinary slot.");
        fill();
        Check(state.FindFreeOrLowestHealthObjectIndex() == 165 &&
            ObjectBytes(state.worldObjects[165]) == empty,
            "Health >= 1000 must not be reclaimed; use the native fallback slot.");
    }

    void TestObjectUpdates()
    {
        GameContext state;
        for (const auto ticks : { 0, 1, 3, 32767 })
        {
            state.tickCount = static_cast<std::int16_t>(ticks);
            for (const bool enabled : { false, true })
            {
                state.objectCollisionsEnabled = enabled;
                for (int bits = 0; bits <= 65535; ++bits)
                {
                    RawGameObject object;
                    object.health = ReferenceSignedWord(bits);
                    object.behavior = BehaviorType::IdleAnimation;
                    object.itemValue = 5;
                    object.velocityX = 31;
                    object.velocityY = -22;
                    object.velocityZ = 19;
                    object.posX = 32760;
                    object.posY = -32760;
                    object.posZ = 150;
                    object.winX = 11;
                    object.winY = 22;
                    object.winZ = 33;
                    auto expected = object;
                    const auto result = UpdateObject(object, state);
                    if (expected.health <= 0)
                    {
                        Check(result.status == DispatchStatus::InactiveObject,
                            "Inactive-at-entry objects must skip shared and case work.");
                    }
                    else
                    {
                        if (enabled && expected.health < 1000)
                        {
                            expected.health = static_cast<std::int16_t>(std::max(0, expected.health - ticks));
                        }
                        expected.blendWeightOrSpriteOpacity =
                            ReferenceSignedWord(5LL * ticks + 200) % 200;
                        Check(result.status == DispatchStatus::ExecutedCase,
                            "Objects expiring in the prefix must still execute their last behavior update.");
                    }
                    Check(ObjectBytes(object) == ObjectBytes(expected),
                        "Shared update must match countdown arithmetic and must not integrate velocity or angles.");
                }
            }
        }
        state = {};
        state.tickCount = 3;
        state.behaviorDispatchEnabled = false;
        RawGameObject object;
        object.health = 2;
        object.behavior = BehaviorType::IdleAnimation;
        object.itemValue = 7;
        Check(UpdateObject(object, state).status == DispatchStatus::DispatchDisabled &&
            object.health == 0 && object.blendWeightOrSpriteOpacity == 0,
            "Disabling the behavior cascade must not disable the shared lifetime countdown.");
        object.health = 2;
        state.behaviorDispatchEnabled = true;
        DispatchObjectCase(object, state);
        Check(object.health == 2, "Direct case dispatch must remain isolated from the shared prefix.");

        state = {};
        for (auto& entry : state.worldObjects)
        {
            entry.health = 2;
            entry.behavior = BehaviorType::None;
        }
        const auto results = UpdateObjects(state);
        for (std::size_t slot = 0; slot < state.worldObjects.size(); ++slot)
        {
            Check(state.worldObjects[slot].health == (slot >= 2 && slot <= 164 ? 1 : 2),
                "The object loop must exclude the player and reserved slots.");
        }
        Check(results.size() == 163 &&
            std::all_of(results.begin(), results.end(), [](const DispatchResult& result)
            {
                return result.status == DispatchStatus::NoSpecificCase;
            }), "The loop must expose a dispatch result for every ordinary slot.");
        state.worldObjects[2].behavior = BehaviorType::BatOutdoor;
        Check(UpdateObjects(state)[0].status == DispatchStatus::UnimplementedHandler,
            "The loop must not hide unrecovered behavior statuses.");
        state.tickCount = -1;
        bool rejected = false;
        try { UpdateObjects(state); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "The world loop must reject negative ticks even when all objects are inactive.");
    }

    void TestSharedBurning()
    {
        for (const auto ticks : { 0, 1, 127, 32767 })
        {
            for (int display = 0; display <= 255; ++display)
            {
                GameContext state;
                state.tickCount = 0;
                state.frameTickCount = static_cast<std::int16_t>(ticks);
                state.behaviorDispatchEnabled = false;
                state.randomSeed = 0xffffffffu;
                RawGameObject object;
                object.health = 1000;
                object.displayType = static_cast<DisplayType>(display);
                const auto seed = display > 49 && display < 121 ? 0xffffffffu * 0x08088405u + 1u : 0xffffffffu;
                auto expected = display;
                if (display > 49 && display < 121)
                {
                    expected -= static_cast<int>((seed >> 16) % (ticks + 1));
                    if (expected < 50) { expected = 22; }
                }
                UpdateObject(object, state);
                Check(static_cast<int>(object.displayType) == expected && state.randomSeed == seed,
                    "Burning bounds, frame-tick random range, burnout display, or disabled-dispatch ordering differs.");
            }
        }
        struct Audio final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            int calls = 0;
            std::uint16_t GetSampleCount() const override { return 129; }
            void StopAllSamples() override { playing.fill(false); }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(sample == 7 && volume == 255, "Burning must select effect 62 at attenuated object volume.");
                playing.at(channel) = true;
                ++calls;
            }
        } audio;
        GameContext state;
        state.audioHandler = &audio;
        state.soundEffectsLoaded = true;
        state.soundEffects[61] = { 1, { 7, 0, 0, 0 } };
        for (std::size_t slot = 2; slot <= 4; ++slot)
        {
            state.worldObjects[slot].health = 1000;
            state.worldObjects[slot].displayType = DisplayType::LastVectorBurning;
        }
        UpdateObjects(state);
        Check(audio.calls == 2 && state.soundChannelSources[3] == 451 && state.soundChannelSources[2] == 451,
            "Burning sounds share native source 451 with concurrency limit two.");
        const auto seed = state.randomSeed;
        state.worldObjects[2].health = 0;
        state.worldObjects[3].health = -1;
        state.worldObjects[4].health = 0;
        UpdateObjects(state);
        Check(state.randomSeed == seed, "Inactive burning objects must not consume random numbers.");
        state.frameTickCount = -1;
        bool rejected = false;
        try { UpdateObjects(state); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Negative frame ticks must be rejected.");
    }

    void TestPlayerInteractionLoop()
    {
        struct TestInputHandler final : InputHandler
        {
            bool pressed = false;
            int calls = 0;

            bool IsKeyPressed(InputKey key) override
            {
                Check(key == InputKey::InteractionKey, "Interaction routing queried a movement key.");
                ++calls;
                return pressed;
            }
        };

        GameContext state;
        auto& player = state.worldObjects[1];
        auto& target = state.worldObjects[2];
        const auto reset = [&]
        {
            state = {};
            player.health = 1000;
            player.collisionMask = 3;
            player.collisionRadiusXZ = 10;
            player.collisionRadiusY = 20;
            target = player;
            target.collisionMask = 2;
            target.collisionRadiusXZ = 5;
            target.collisionRadiusY = 7;
            target.collisionType = CollisionType::Gold;
            target.itemValue = 10;
            target.pickupType = PickupType::Type_1;
        };
        reset();
        Check(state.FindCollidingObject(1) == 2 && state.FindPlayerCollision() == 2,
            "Player collision must find the first eligible world object.");
        target.health = 0;
        Check(state.FindCollidingObject(1) == 0, "Zero-health candidates must be ignored.");
        target.health = -1;
        Check(state.FindCollidingObject(1) == 0, "Negative-health candidates must be ignored.");
        target.health = 1;
        target.collisionMask = 4;
        Check(state.FindCollidingObject(1) == 0, "Collision masks must share a bit.");
        target.collisionMask = 2;
        target.pickupType = PickupType::None;
        player.health = 0;
        Check(state.FindCollidingObject(1) == 2,
            "The original generic query does not gate on source health or candidate pickup flag.");
        Check(state.FindCollidingObject(2) == 0, "Generic collision queries must exclude their source slot.");

        reset();
        for (int axis = 0; axis < 3; ++axis)
        {
            for (int bits = 0; bits <= 65535; ++bits)
            {
                const auto position = ReferenceSignedWord(bits);
                target.posX = axis == 0 ? position : 0;
                target.posY = axis == 1 ? position : 0;
                target.posZ = axis == 2 ? position : 0;
                const auto difference = ReferenceSignedWord(-static_cast<std::int64_t>(position));
                const auto distance = ReferenceSignedWord(difference < 0 ?
                    -static_cast<std::int64_t>(difference) : difference);
                const auto radiusSum = axis == 1 ? 27 : 15;
                Check(state.FindCollidingObject(1) == (distance < radiusSum ? 2 : 0),
                    "Collision geometry differs from signed 16-bit subtraction/ABS or strict axis bounds.");
            }
        }
        reset();
        for (int bits = 0; bits <= 65535; ++bits)
        {
            player.collisionRadiusXZ = ReferenceSignedWord(bits);
            target.collisionRadiusXZ = 1;
            target.posX = 1;
            const auto sum = ReferenceSignedWord(static_cast<std::int64_t>(player.collisionRadiusXZ) + 1);
            Check(state.FindCollidingObject(1) == (1 < sum ? 2 : 0),
                "XZ radius sums must wrap before signed comparison.");
        }
        reset();
        player.posX = 32767;
        target.posX = -32768;
        Check(state.FindCollidingObject(1) == 2, "Coordinate differences must wrap across the word boundary.");
        reset();
        player.collisionRadiusXZ = 0;
        target.collisionRadiusXZ = 0;
        target.posX = -32768;
        target.posZ = -32768;
        Check(state.FindCollidingObject(1) == 2,
            "The original signed ABS overflow must remain observable, even with zero XZ radii.");
        reset();
        for (const auto position : { -15, 15 })
        {
            target.posX = static_cast<std::int16_t>(position);
            Check(state.FindCollidingObject(1) == 0, "Merely touching an X boundary is not a collision.");
        }
        target.posX = 0;
        for (const auto position : { -27, 27 })
        {
            target.posY = static_cast<std::int16_t>(position);
            Check(state.FindCollidingObject(1) == 0, "Merely touching a Y boundary is not a collision.");
        }

        reset();
        state.worldObjects[0] = target;
        state.worldObjects[165] = target;
        target.health = 0;
        Check(state.FindCollidingObject(1) == 0 && state.FindPlayerCollision() == 0,
            "Reserved slots must not become initial collision candidates.");
        state.worldObjects[164] = state.worldObjects[165];
        Check(state.FindCollidingObject(1) == 164 && state.FindPlayerCollision() == 165,
            "The player wrapper must preserve its post-test slot-165 boundary read.");
        state.worldObjects[165].pickupType = PickupType::None;
        Check(state.FindPlayerCollision() == 164, "An ineligible boundary candidate must not replace slot 164.");
        Check(state.FindCollidingObject(164) == 0, "Last-slot generic queries must exclude themselves.");
        Check(state.FindCollidingObject(165) == 164, "A valid source in slot 165 must use the original candidate range.");

        reset();
        state.worldObjects[3] = target;
        state.worldObjects[3].posX = 10;
        state.worldObjects[4] = target;
        Check(state.FindPlayerCollision() == 3,
            "Player selection must prefer the first qualifying later object, not the nearest or the last.");
        state.worldObjects[4].health = 0;
        for (const auto health : { -1, 0, 1, 999 })
        {
            state.worldObjects[3].health = static_cast<std::int16_t>(health);
            Check(state.FindPlayerCollision() == 2, "Preferred candidates require signed health above 999.");
        }
        state.worldObjects[3].health = 1000;
        state.worldObjects[3].pickupType = PickupType::None;
        Check(state.FindPlayerCollision() == 2, "Preferred candidates require a nonzero pickup flag.");
        state.worldObjects[3].pickupType = PickupType::Type_1;
        state.worldObjects[3].collisionMask = 4;
        Check(state.FindPlayerCollision() == 2, "Preferred candidates must still pass mask filtering.");
        state.worldObjects[3].collisionMask = 2;
        state.worldObjects[3].posX = 15;
        Check(state.FindPlayerCollision() == 2, "Preferred candidates must still strictly overlap.");
        state.worldObjects[3].posX = 14;
        Check(state.FindPlayerCollision() == 3, "A valid later overlap must replace the initial candidate.");

        reset();
        for (const auto source : { -32768, -1, 166, 32767 })
        {
            bool rejected = false;
            try { state.FindCollidingObject(static_cast<std::int16_t>(source)); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected, "Out-of-storage source slots must fail explicitly.");
        }
        state.objectCollisionsEnabled = false;
        Check(state.FindCollidingObject(-1) == 0 && state.FindPlayerCollision() == 0,
            "The original DS:D500 gate must return before unused source access.");

        reset();
        TestInputHandler input;
        auto result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::NoEffect && state.playerCollisionSlot == 2 &&
            input.calls == 1 && target.health == 1000 && state.gold == 0,
            "Overlap without interaction input must select the target without collecting it.");
        input.pressed = true;
        state.behaviorDispatchEnabled = false;
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::Applied && state.playerCollisionSlot == 2 &&
            state.gold == 10 && target.health == 0 && input.calls == 2,
            "Collision then input must invoke the interaction independently of behavior dispatch.");
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::NoEffect && state.playerCollisionSlot == 0 &&
            state.gold == 10 && input.calls == 2,
            "Collected objects must stop being selected; no target must not invoke input/interaction.");

        reset();
        input = {};
        input.pressed = true;
        state.worldObjects[3] = target;
        state.worldObjects[3].itemValue = 20;
        UpdatePlayerInteraction(state, input);
        Check(state.playerCollisionSlot == 3 && state.gold == 20 && target.health == 1000,
            "Input must act on the preferred selected object, not the initial generic hit.");
        UpdatePlayerInteraction(state, input);
        Check(state.playerCollisionSlot == 2 && state.gold == 30 && input.calls == 2,
            "Input edge/held semantics must belong to the input handler, not an invented loop debounce.");

        reset();
        input = {};
        input.pressed = true;
        state.objectCollisionsEnabled = false;
        state.playerCollisionSlot = 123;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::NoEffect &&
            state.playerCollisionSlot == 0 && input.calls == 0 && target.health == 1000,
            "Disabled collision queries must clear stale selection and never interact.");
        state.objectCollisionsEnabled = true;
        target.posZ = 15;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::NoEffect &&
            input.calls == 0 && state.gold == 0, "A pressed key without overlap must not collect anything.");
        target.posZ = 0;
        target.collisionType = CollisionType::Door;
        Check(UpdatePlayerInteraction(state, input).status == InteractionStatus::Applied &&
            target.health == 1000 && target.generalPurpose2 == 3 && target.behaviorParameter == 1,
            "Door contact must mark occupancy and interaction must activate an unlocked door.");

        reset();
        input = {};
        input.pressed = true;
        target.collisionType = CollisionType::Key;
        target.itemValue = 7;
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::Applied && result.inventorySlot == 1 &&
            state.inventory[0].collisionType == CollisionType::Key &&
            state.inventory[0].itemValue == 7 && target.health == 0,
            "Collision/input-driven key pickup must populate the real game-context inventory.");

        reset();
        input = {};
        input.pressed = true;
        target.collisionType = CollisionType::Weapon;
        target.itemValue = static_cast<std::int16_t>(WeaponType::Dagger);
        state.inventory[0] = target;
        result = UpdatePlayerInteraction(state, input);
        Check(result.status == InteractionStatus::Applied && state.playerCollisionSlot == 0 &&
            target.health == 1000 && target.collisionType == CollisionType::None,
            "Duplicate weapon interaction must immediately clear the now-modeled DS:DC60 selection.");
    }

    void TestObjectInteractions()
    {
        const auto pickup = [](CollisionType type, std::int16_t value = 1)
        {
            RawGameObject object;
            object.collisionType = type;
            object.itemValue = value;
            object.health = 1000;
            object.pickupType = PickupType::Type_1;
            object.modelId = 17;
            object.posX = -123;
            return object;
        };
        const auto sameObject = [](const RawGameObject& left, const RawGameObject& right)
        {
            return std::memcmp(&left, &right, sizeof(left)) == 0;
        };
        GameContext state;
        Check(state.inventory.size() == 14 && state.weaponAmounts.size() == 32 &&
            state.weaponInventorySlots.size() == 32, "Wrong original inventory/counter capacities.");
        Check(state.gold == 0 && state.arrowPoison == 0 && !state.hasMap &&
            state.weaponAmounts[8] == 100 && state.weaponAmounts[20] == 1000 &&
            state.weaponAmounts[23] == 100, "Original new-game interaction defaults changed.");
        for (const auto& object : state.inventory)
        {
            Check(sameObject(object, RawGameObject{}), "Inventory must start empty.");
        }
        for (std::size_t i = 0; i < state.weaponAmounts.size(); ++i)
        {
            Check(state.weaponInventorySlots[i] == 0 &&
                state.weaponAmounts[i] == (i == 8 || i == 23 ? 100 : i == 20 ? 1000 : 0),
                "Unexpected original weapon/resource initialization.");
        }

        for (int bits = 0; bits <= 65535; ++bits)
        {
            const auto value = ReferenceSignedWord(bits);
            auto gold = pickup(CollisionType::Gold, value);
            auto expected = gold;
            expected.health = 0;
            state.gold = 12345;
            Check(HandlePlayerObjectInteraction(gold, state).status == InteractionStatus::Applied &&
                state.gold == ReferenceSignedWord(12345LL + value) && sameObject(gold, expected),
                "Gold pickup must wrap one word and clear only world health.");
            const auto amount = state.gold;
            Check(HandlePlayerObjectInteraction(gold, state).status == InteractionStatus::InactiveObject &&
                state.gold == amount, "Collected gold must not be awarded twice.");

            auto poison = pickup(CollisionType::Poison, value);
            state.arrowPoison = -123;
            HandlePlayerObjectInteraction(poison, state);
            Check(state.arrowPoison == std::max<std::int16_t>(-123, value) && poison.health == 0,
                "Arrow poison pickup must take the signed maximum.");

            state.inventory[0] = pickup(CollisionType::Medicine, 23456);
            auto medicine = pickup(CollisionType::Medicine, value);
            medicine.pickupType = PickupType::None;
            const auto result = HandlePlayerObjectInteraction(medicine, state);
            Check(result.status == InteractionStatus::Applied && result.inventorySlot == 1 &&
                state.inventory[0].itemValue == ReferenceSignedWord(23456LL + value) && medicine.health == 0,
                "Medicine must stack with word wrapping, even without a pickup flag.");
        }

        state = {};
        auto map = pickup(CollisionType::PickableMap);
        map.pickupType = PickupType::None;
        Check(HandlePlayerObjectInteraction(map, state).status == InteractionStatus::Applied &&
            state.hasMap && map.health == 0, "Map interaction must set ownership without consuming a slot.");
        auto medicine = pickup(CollisionType::Medicine, 25);
        const auto medicineCopy = medicine;
        auto result = HandlePlayerObjectInteraction(medicine, state);
        Check(result.inventorySlot == 1 && sameObject(state.inventory[0], medicineCopy) && medicine.health == 0,
            "The first medicine must copy all 54 bytes into the first slot.");

        state = {};
        state.inventory[0].health = -1;
        state.inventory[1] = pickup(CollisionType::Medicine, 40);
        state.inventory[1].health = 1;
        auto key = pickup(CollisionType::Key, 7);
        key.renderPlacementMode = 0x14;
        key.soundEffectId = 0xab;
        key.behaviorParameter = 0x21;
        key.spriteScale = 0xdc;
        auto expectedKey = key;
        expectedKey.renderPlacementMode = 1;
        expectedKey.doorLockOrGroupBitmask = 0xdc;
        result = HandlePlayerObjectInteraction(key, state);
        Check(result.status == InteractionStatus::Applied && result.inventorySlot == 3 &&
            sameObject(state.inventory[2], expectedKey) && key.health == 0 &&
            key.renderPlacementMode == 0x14 && key.soundEffectId == 0xab && key.doorLockOrGroupBitmask == 0,
            "Inventory occupancy/copy normalization must match the original and not normalize the world object.");
        auto secondKey = pickup(CollisionType::Key, 7);
        Check(HandlePlayerObjectInteraction(secondKey, state).inventorySlot == 4,
            "Keys must occupy individual slots, not stack by key index.");
        medicine = pickup(CollisionType::Medicine, 5);
        Check(HandlePlayerObjectInteraction(medicine, state).inventorySlot == 5 &&
            state.inventory[1].itemValue == 40,
            "Medicine may stack only with health exactly 1000, not merely positive health.");

        for (auto& stored : state.inventory)
        {
            stored = pickup(CollisionType::Key);
        }
        key = pickup(CollisionType::Key);
        const auto fullInventory = state.inventory;
        const auto uncollectedKey = key;
        Check(HandlePlayerObjectInteraction(key, state).status == InteractionStatus::InventoryFull &&
            sameObject(key, uncollectedKey) &&
            std::memcmp(state.inventory.data(), fullInventory.data(), sizeof(state.inventory)) == 0,
            "Full inventory must not overwrite any slot or remove the world pickup.");
        state.inventory.back() = pickup(CollisionType::Medicine, 20);
        medicine = pickup(CollisionType::Medicine, 10);
        Check(HandlePlayerObjectInteraction(medicine, state).inventorySlot == 14 &&
            state.inventory.back().itemValue == 30,
            "Medicine must still stack when every inventory slot is occupied.");
        state.inventory.back() = {};
        key = pickup(CollisionType::Key);
        Check(HandlePlayerObjectInteraction(key, state).inventorySlot == 14,
            "The final inventory slot must remain usable.");
        auto gold = pickup(CollisionType::Gold, 9);
        Check(HandlePlayerObjectInteraction(gold, state).status == InteractionStatus::Applied &&
            state.gold == 9, "Gold must not require a free inventory slot.");

        for (std::int16_t index = 0; index < 32; ++index)
        {
            state = {};
            state.weaponAmounts[index] = 32767;
            auto ammo = pickup(CollisionType::Ammo, index);
            Check(HandlePlayerObjectInteraction(ammo, state).status == InteractionStatus::Applied &&
                state.weaponAmounts[index] == -32768 && ammo.groupIndexOrLock == 1 && ammo.health == 0,
                "Ammo must use the full 32-entry table, default zero quantity to one, and wrap.");
            ammo = pickup(CollisionType::Ammo, index);
            ammo.groupIndexOrLock = -2;
            HandlePlayerObjectInteraction(ammo, state);
            Check(state.weaponAmounts[index] == 32766,
                "Signed ammo quantity must be added without saturation.");
        }
        for (const auto invalid : { -32768, -1, 32, 32767 })
        {
            state = {};
            auto ammo = pickup(CollisionType::Ammo, static_cast<std::int16_t>(invalid));
            const auto before = ammo;
            const auto amounts = state.weaponAmounts;
            bool rejected = false;
            try { HandlePlayerObjectInteraction(ammo, state); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && sameObject(ammo, before) && state.weaponAmounts == amounts,
                "Invalid ammo indices must fail explicitly before mutation.");
            auto weapon = pickup(CollisionType::Weapon, static_cast<std::int16_t>(invalid));
            if (invalid >= 0)
            {
                rejected = false;
                try { HandlePlayerObjectInteraction(weapon, state); }
                catch (const std::out_of_range&) { rejected = true; }
                Check(rejected && weapon.health == 1000 && state.inventory[0].health == 0,
                    "Invalid weapon indices must not leave partially stored items.");
            }
        }

        for (std::int16_t type = 0; type <= 8; ++type)
        {
            state = {};
            state.weaponAmounts[type] = 123;
            auto weapon = pickup(CollisionType::Weapon, type);
            weapon.generalPurpose1 = 0x8002;
            result = HandlePlayerObjectInteraction(weapon, state);
            const auto expectedAmount = type == static_cast<std::int16_t>(WeaponType::Fireball) ? -32766 :
                type == static_cast<std::int16_t>(WeaponType::None) ||
                type == static_cast<std::int16_t>(WeaponType::Crossbow) ? 123 : 360;
            Check(result.inventorySlot == 1 && state.weaponInventorySlots[type] == 1 &&
                state.weaponAmounts[type] == expectedAmount && weapon.health == 0,
                "Weapon collection must set its original slot and type-specific amount.");

            auto duplicate = pickup(CollisionType::Weapon, type);
            duplicate.generalPurpose1 = 0xffff;
            duplicate.collisionMask = 0xff;
            result = HandlePlayerObjectInteraction(duplicate, state);
            if (type == static_cast<std::int16_t>(WeaponType::Stone) ||
                type == static_cast<std::int16_t>(WeaponType::TrapOrPole))
            {
                Check(result.inventorySlot == 2 && state.weaponInventorySlots[type] == 2 &&
                    duplicate.health == 0, "Stone/trap weapons must permit duplicate inventory objects.");
            }
            else if (type == static_cast<std::int16_t>(WeaponType::Fireball))
            {
                Check(result.inventorySlot == 1 && state.inventory[0].generalPurpose1 == 0x8001 &&
                    state.weaponAmounts[type] == -32767 && duplicate.health == 0,
                    "Duplicate fireball weapons must merge their field-24 charges.");
            }
            else
            {
                Check(result.inventorySlot == 1 && duplicate.health == 1000 &&
                    duplicate.collisionType == CollisionType::None &&
                    duplicate.collisionMask == 0 && duplicate.pickupType == PickupType::None &&
                    state.inventory[1].health == 0,
                    "Other duplicate weapons become non-colliding world objects, not new inventory items.");
            }
        }
        state = {};
        auto specialWeapon = pickup(CollisionType::Weapon, -1);
        Check(HandlePlayerObjectInteraction(specialWeapon, state).inventorySlot == 1 &&
            state.weaponInventorySlots == GameContext{}.weaponInventorySlots,
            "Negative weapon identifiers must skip the original table registration.");
        auto extendedWeapon = pickup(CollisionType::Weapon, 31);
        Check(HandlePlayerObjectInteraction(extendedWeapon, state).inventorySlot == 2 &&
            state.weaponInventorySlots[31] == 2,
            "Recovered tables must not be incorrectly truncated to the nine named weapon types.");

        state = {};
        RawGameObject grouped;
        grouped.health = 1000;
        grouped.collisionType = CollisionType::GroupedSwitch;
        grouped.behavior = BehaviorType::SwitchGrouped;
        grouped.itemValue = 5;
        grouped.renderPlacementMode = 0x5a;
        grouped.groupIndexOrLock = 2;
        grouped.doorLockOrGroupBitmask = 3;
        Check(HandlePlayerObjectInteraction(grouped, state).status == InteractionStatus::Applied &&
            grouped.generalPurpose2 == 10 && grouped.generalPurpose1 == 5 && grouped.renderPlacementMode == 0x5a &&
            grouped.soundEffectId == 2 &&
            state.switchGroupMasks[2] == 0xff,
            "Grouped interaction must arm the timer, start activation, and defer mask writes.");
        const auto movingSwitch = grouped;
        Check(HandlePlayerObjectInteraction(grouped, state).status == InteractionStatus::NoEffect &&
            sameObject(grouped, movingSwitch), "Moving switches must ignore reactivation.");
        for (int i = 0; i < 10; ++i) { DispatchObjectCase(grouped, state); }
        Check(grouped.blendWeightOrSpriteOpacity == 100 && grouped.generalPurpose1 == 4 &&
            state.switchGroupMasks[2] == 0xfc, "Grouped timer must start only at full activation.");
        for (int i = 0; i < 4; ++i) { DispatchObjectCase(grouped, state); }
        Check(grouped.generalPurpose1 == 0 && grouped.generalPurpose2 == 65526 &&
            state.switchGroupMasks[2] == 0xff, "Interaction and behavior must produce timed mask restoration.");
        for (int i = 0; i < 11; ++i) { DispatchObjectCase(grouped, state); }
        Check(grouped.blendWeightOrSpriteOpacity == 0 && grouped.generalPurpose2 == 0,
            "Grouped return animation must stop at the inactive endpoint.");
        grouped.blendWeightOrSpriteOpacity = 100;
        grouped.renderPlacementMode = 0x11;
        grouped.soundEffectId = 0x7f;
        state.switchGroupMasks[2] = 0xfc;
        Check(HandlePlayerObjectInteraction(grouped, state).status == InteractionStatus::Applied &&
            grouped.generalPurpose2 == 65526 && grouped.generalPurpose1 == 5 && grouped.renderPlacementMode == 0x11 &&
            grouped.soundEffectId == 0x7f &&
            state.switchGroupMasks[2] == 0xfc, "Manual grouped return must not restore the mask immediately.");

        state = {};
        grouped = pickup(CollisionType::GroupedSwitch, 5);
        grouped.generalPurpose1 = 99;
        const auto switchPreview = grouped;
        result = HandlePlayerObjectInteraction(grouped, state);
        Check(result.inventorySlot == 1 && sameObject(state.inventory[0], switchPreview) &&
            grouped.health == 0 && grouped.generalPurpose2 == 10 && grouped.generalPurpose1 == 5,
            "Generic switch pickup must copy the pre-interaction snapshot, not the newly mutated world object.");
        for (auto& stored : state.inventory) { stored = pickup(CollisionType::Key); }
        grouped = switchPreview;
        result = HandlePlayerObjectInteraction(grouped, state);
        Check(result.status == InteractionStatus::InventoryFull && grouped.health == 1000 &&
            grouped.generalPurpose2 == 10 && grouped.generalPurpose1 == 5 &&
            state.inventory[0].collisionType == CollisionType::Key,
            "Original local activation must survive a subsequent generic inventory-full result.");

        state = {};
        key = pickup(CollisionType::Key);
        key.pickupType = PickupType::None;
        Check(HandlePlayerObjectInteraction(key, state).status == InteractionStatus::NoEffect &&
            key.health == 1000, "Unpickable generic objects must remain in the world.");
        auto ammo = pickup(CollisionType::Ammo, -1);
        ammo.pickupType = PickupType::None;
        Check(HandlePlayerObjectInteraction(ammo, state).status == InteractionStatus::NoEffect,
            "Unpickable ammo must not access its unused amount index.");
        key.health = -1;
        Check(HandlePlayerObjectInteraction(key, state).status == InteractionStatus::InactiveObject,
            "Host interaction requests must reject inactive selected objects.");
        for (const auto type : { CollisionType::BookWithCode, CollisionType::BuyItem,
            CollisionType::WheelToLiftRope, CollisionType::VaseToGrab, CollisionType::MirrorToGrab,
            CollisionType::LevelEnd, CollisionType::SarcophagusWithBones,
            CollisionType::PictureMap, CollisionType::Lorry })
        {
            auto unsupported = pickup(type);
            const auto before = unsupported;
            Check(HandlePlayerObjectInteraction(unsupported, state).status == InteractionStatus::UnimplementedHandler &&
                sameObject(unsupported, before) && state.inventory[0].health == 0,
                "Unimplemented special interactions must not fall through into fake successful pickups.");
        }
        auto generic = pickup(static_cast<CollisionType>(255));
        Check(HandlePlayerObjectInteraction(generic, state).inventorySlot == 1,
            "Collision bytes without a specialized branch must preserve the original generic pickup tail.");
        for (const auto type : { CollisionType::PuzzleGameTile, CollisionType::PuzzleSpool,
            CollisionType::GrandfatherClockItem })
        {
            auto local = pickup(type);
            local.pickupType = PickupType::None;
            local.velocityX = 32767;
            local.generalPurpose1 = 1;
            Check(HandlePlayerObjectInteraction(local, state).status == InteractionStatus::Applied,
                "Known object-local interactions must execute.");
            Check(type != CollisionType::PuzzleGameTile || local.generalPurpose3 == 1,
                "Puzzle tile interaction must set field 28.");
            Check(type != CollisionType::PuzzleSpool || local.velocityX == 1,
                "Puzzle spool interaction must set velocity X.");
            Check(type != CollisionType::GrandfatherClockItem || local.velocityX == -32768,
                "Clock interaction must add a wrapped signed word.");
        }

        state = {};
        gold = pickup(CollisionType::Gold, 10);
        gold.behavior = BehaviorType::IdleAnimation;
        DispatchObjectCase(gold, state);
        Check(gold.health == 1000 && state.gold == 0 && state.inventory[0].health == 0,
            "Per-frame behavior must never implicitly request interaction.");
        state.behaviorDispatchEnabled = false;
        HandlePlayerObjectInteraction(gold, state);
        Check(state.gold == 10 && gold.health == 0,
            "Explicit interaction must not be gated by the separate behavior-cascade flag.");
        GameContext independent;
        Check(independent.gold == 0 && independent.arrowPoison == 0 &&
            !independent.hasMap && independent.inventory[0].health == 0,
            "Interaction state must belong to each game context.");
    }

    void TestGameMath()
    {
        for (int bits = 0; bits <= 65535; ++bits)
        {
            const auto value = ReferenceSignedWord(bits);
            Check(WrapSignedWord(bits) == value && SignWord(value) == (value < 0 ? -1 : value > 0 ? 1 : 0),
                "Signed-word conversion/sign differs from the original.");
            for (const auto step : { 0, 1, 2, 32767, -32768, -1 })
            {
                for (const auto target : { 0, 32767, -32768 })
                {
                    Check(GetStepToward(value, static_cast<std::int16_t>(target), static_cast<std::int16_t>(step)) ==
                        ReferenceStep(value, static_cast<std::int16_t>(target), static_cast<std::int16_t>(step)),
                        "Step helper differs at a signed/absolute-value boundary.");
                }
            }
            std::int16_t whole = 32767;
            std::int16_t fraction = 250;
            const auto sumBits = static_cast<std::uint16_t>((250u + static_cast<unsigned int>(bits)) % 65536);
            const int highByte = sumBits / 256;
            const int carry = highByte >= 128 ? highByte - 256 : highByte;
            AddFixedPoint8(whole, fraction, value);
            Check(whole == ReferenceSignedWord(32767LL + carry) && fraction == sumBits % 256,
                "Fixed-point helper must use the signed high-byte carry and retain only the low byte.");
        }
        std::int16_t aliased = 300;
        AddFixedPoint8(aliased, aliased, 1);
        Check(aliased == 46, "Fixed-point reference update order must match the original when arguments alias.");
    }

    void TestSmokeFading()
    {
        RawGameObject object;
        std::array<std::uint16_t, 27> fixture;
        for (std::size_t i = 0; i < fixture.size(); ++i)
        {
            fixture[i] = static_cast<std::uint16_t>(0x9876u + i * 2);
        }
        std::memcpy(&object, fixture.data(), sizeof(object));
        object.behavior = BehaviorType::SmokeFading;
        object.health = 40;
        object.posY = 100;
        object.itemValue = 250;
        object.velocityY = 300;
        object.velocityZ = 3;
        object.behaviorParameter = 0x34;
        object.spriteScale = 0xc7;
        GameContext state;
        state.tickCount = 2;
        auto expected = object;
        expected.posY = 103;
        expected.itemValue = 82;
        expected.blendWeightOrSpriteOpacity = 5;
        expected.spriteScale = 0xcd;
        expected.velocityY = 296;
        const auto result = DispatchObjectCase(object, state);
        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1a9d &&
            result.behavior == BehaviorType::SmokeFading, "SmokeFading did not execute.");
        Check(ObjectBytes(object) == ObjectBytes(expected) && state.collisionMap == nullptr &&
            state.tickCount == 2 && state.behaviorDispatchEnabled,
            "SmokeFading must modify only Y, fraction, opacity, scale high byte, and vertical velocity.");

        for (int bits = 0; bits <= 65535; ++bits)
        {
            for (const auto ticks : { 0, 1, 16384, 32767 })
            {
                object.posY = ReferenceSignedWord(bits * 3LL);
                object.itemValue = ReferenceSignedWord(bits * 19LL);
                object.velocityY = ReferenceSignedWord(bits);
                object.velocityZ = ReferenceSignedWord(bits * 5LL);
                object.health = ReferenceSignedWord(bits);
                object.behaviorParameter = static_cast<std::uint8_t>(bits ^ 0xc7a5);
                object.spriteScale = static_cast<std::uint8_t>((bits ^ 0xc7a5) >> 8);
                state.tickCount = static_cast<std::int16_t>(ticks);
                auto words = ObjectWords(object);
                const auto sum = (static_cast<std::uint64_t>(words[10]) * ticks + words[17]) % 65536;
                const auto carry = sum / 256 >= 128 ? static_cast<std::int64_t>(sum / 256) - 256 :
                    static_cast<std::int64_t>(sum / 256);
                words[7] = static_cast<std::uint16_t>(ReferenceSignedWord(words[7] + carry));
                words[17] = static_cast<std::uint16_t>(sum % 256);
                const auto shiftedHealth = static_cast<std::uint16_t>((words[5] >> 2) |
                    ((words[5] & 0x8000) != 0 ? 0xc000 : 0));
                words[21] = ReferenceSignedWord(shiftedHealth) > 5 ? 5 : shiftedHealth;
                const auto highByte = words[26] / 256;
                if (highByte < 200)
                {
                    const auto newHighByte = (highByte + static_cast<std::uint64_t>(words[11]) * ticks) % 256;
                    words[26] = static_cast<std::uint16_t>((words[26] % 256) + newHighByte * 256);
                }
                const auto velocity = ReferenceSignedWord(words[10]);
                words[10] = static_cast<std::uint16_t>(ReferenceSignedWord(static_cast<std::int64_t>(velocity) +
                    ReferenceStep(velocity, 0, ReferenceSignedWord(ticks * 2LL))));
                Check(Function_SmokeFading(object, state).status == DispatchStatus::ExecutedCase &&
                    ObjectWords(object) == words,
                    "SmokeFading differs from the original word/byte arithmetic or changed unrelated data.");
            }
        }
        object.health = 0;
        const auto inactive = ObjectBytes(object);
        Check(DispatchObjectCase(object, state).status == DispatchStatus::InactiveObject &&
            ObjectBytes(object) == inactive, "Inactive smoke must not be updated.");
        object.health = 1;
        state.behaviorDispatchEnabled = false;
        const auto disabled = ObjectBytes(object);
        Check(DispatchObjectCase(object, state).status == DispatchStatus::DispatchDisabled &&
            ObjectBytes(object) == disabled, "Disabled smoke must not be updated.");
    }

    void TestSwitches()
    {
        GameContext initial;
        for (const auto mask : initial.switchGroupMasks)
        {
            Check(mask == 0xff, "All five group masks must initialize to FF as in the executable.");
        }
        GameContext independent;
        independent.switchGroupMasks[0] = 0;
        Check(initial.switchGroupMasks[0] == 0xff, "Game states must not share group-mask storage.");
        initial.switchGroupMasks[1] = 0x55;
        initial.switchGroupMasks[2] = 0xaa;
        initial.switchGroupMasks[3] = 0;
        initial.switchGroupMasks[4] = 0x33;

        const auto checkUpdate = [&](RawGameObject object, std::int16_t ticks)
        {
            auto expected = object;
            GameContext state;
            std::copy(std::begin(initial.switchGroupMasks), std::end(initial.switchGroupMasks),
                std::begin(state.switchGroupMasks));
            state.tickCount = ticks;
            std::array<std::uint8_t, 5> expectedGroups;
            std::copy(std::begin(state.switchGroupMasks), std::end(state.switchGroupMasks), expectedGroups.begin());
            const bool grouped = object.behavior == BehaviorType::SwitchGrouped;
            if (!grouped && expected.generalPurpose1 == 0) { expected.generalPurpose1 = 10; }
            const auto sum = static_cast<std::int64_t>(static_cast<std::uint16_t>(object.blendWeightOrSpriteOpacity)) +
                static_cast<std::int64_t>(object.generalPurpose2) * static_cast<std::uint16_t>(ticks);
            auto phase = ReferenceSignedWord(sum);
            if (phase < 0) { phase = 0; expected.generalPurpose2 = 0; }
            if (phase > 100)
            {
                phase = 100;
                expected.generalPurpose2 = grouped ? 0 :
                    static_cast<std::uint16_t>(ReferenceSignedWord(-static_cast<std::int64_t>(expected.generalPurpose1)));
            }
            expected.blendWeightOrSpriteOpacity = phase;
            if (grouped)
            {
                expected.collisionType = CollisionType::GroupedSwitch;
                const auto mask = static_cast<std::uint16_t>(object.doorLockOrGroupBitmask) % 256;
                auto& group = expectedGroups[object.groupIndexOrLock];
                if (phase == 100) { group = static_cast<std::uint8_t>(group & (255 - mask)); }
                if (phase == 0) { group = static_cast<std::uint8_t>(group | mask); }
                if (phase == 100 && ReferenceSignedWord(expected.generalPurpose1) > 0 && object.itemValue > 0)
                {
                    const auto countdown = ReferenceSignedWord(static_cast<std::int64_t>(expected.generalPurpose1) - ticks);
                    expected.generalPurpose1 = countdown < 1 ? 0 : static_cast<std::uint16_t>(countdown);
                    if (countdown < 1)
                    {
                        expected.generalPurpose2 = 0xfff6;
                        group = static_cast<std::uint8_t>(group | mask);
                    }
                }
            }
            else if (phase == 100 && object.behaviorParameter != 0)
            {
                expected.generalPurpose2 = 0;
            }
            const auto result = DispatchObjectCase(object, state);
            Check(result.status == DispatchStatus::ExecutedCase && result.behavior == expected.behavior &&
                result.moduleOffset == (grouped ? 0x2065 : 0x1fdd),
                "Switch case did not execute at its original address.");
            Check(ObjectBytes(object) == ObjectBytes(expected),
                "Switch differs from original word arithmetic or changed an unrelated object field.");
            Check(std::equal(std::begin(state.switchGroupMasks), std::end(state.switchGroupMasks),
                expectedGroups.begin()) && state.tickCount == ticks &&
                state.behaviorDispatchEnabled && state.collisionMap == nullptr,
                "Switch changed the wrong group bits or unrelated context state.");
        };

        RawGameObject object;
        auto* bytes = reinterpret_cast<unsigned char*>(&object);
        for (std::size_t i = 0; i < sizeof(object); ++i) { bytes[i] = static_cast<unsigned char>(0xa5 + i); }
        object.health = 1;
        object.itemValue = 1;
        for (const auto behavior : { BehaviorType::Switch1, BehaviorType::SwitchGrouped })
        {
            object.behavior = behavior;
            for (int bits = 0; bits <= 65535; ++bits)
            {
                object.groupIndexOrLock = static_cast<std::int16_t>(bits % 5);
                object.doorLockOrGroupBitmask = ReferenceSignedWord(bits);
                for (const auto ticks : { 0, 1, 32767 })
                {
                    object.behaviorParameter = bits % 3 == 0 ? 1 : 0;
                    object.spriteScale = bits % 3 == 1 ? 1 : 0;
                    object.generalPurpose1 = 10;
                    object.generalPurpose2 = static_cast<std::uint16_t>(bits);
                    object.blendWeightOrSpriteOpacity = 95;
                    checkUpdate(object, static_cast<std::int16_t>(ticks));
                    object.generalPurpose2 = 10;
                    object.blendWeightOrSpriteOpacity = ReferenceSignedWord(bits);
                    checkUpdate(object, static_cast<std::int16_t>(ticks));
                    object.behaviorParameter = 0;
                    object.spriteScale = 0;
                    object.generalPurpose1 = static_cast<std::uint16_t>(bits);
                    object.generalPurpose2 = behavior == BehaviorType::SwitchGrouped ? 0 : 1;
                    object.blendWeightOrSpriteOpacity = 100;
                    checkUpdate(object, static_cast<std::int16_t>(ticks));
                }
            }
            for (const auto enable : { -32768, -1, 0, 1, 32767 })
            {
                object.itemValue = static_cast<std::int16_t>(enable);
                object.generalPurpose1 = 3;
                object.generalPurpose2 = 0;
                object.blendWeightOrSpriteOpacity = 100;
                checkUpdate(object, 3);
            }
            object.itemValue = 1;
        }

        GameContext shared;
        shared.switchGroupMasks[2] = 3;
        RawGameObject first;
        first.behavior = BehaviorType::SwitchGrouped;
        first.health = 1;
        first.groupIndexOrLock = 2;
        first.doorLockOrGroupBitmask = 1;
        first.itemValue = 1;
        first.generalPurpose1 = 3;
        first.generalPurpose2 = 10;
        first.blendWeightOrSpriteOpacity = 90;
        auto second = first;
        second.doorLockOrGroupBitmask = 2;
        second.itemValue = 0;
        DispatchObjectCase(first, shared);
        DispatchObjectCase(second, shared);
        Check(shared.switchGroupMasks[2] == 0 && first.generalPurpose1 == 2,
            "Two grouped switches must combine to clear their shared group.");
        DispatchObjectCase(first, shared);
        Check(first.generalPurpose1 == 1 && shared.switchGroupMasks[2] == 0,
            "An active switch must count elapsed ticks while holding its bits clear.");
        DispatchObjectCase(first, shared);
        Check(first.generalPurpose1 == 0 && first.generalPurpose2 == 0xfff6 &&
            first.blendWeightOrSpriteOpacity == 100 && shared.switchGroupMasks[2] == 1,
            "Timer expiry must restore bits immediately, before the return animation.");
        DispatchObjectCase(first, shared);
        DispatchObjectCase(second, shared);
        Check(first.blendWeightOrSpriteOpacity == 90 && shared.switchGroupMasks[2] == 1,
            "Another active switch must not clear an expired switch's different bits.");

        for (const auto invalid : { -32768, -1, 5, 32767 })
        {
            first.groupIndexOrLock = static_cast<std::int16_t>(invalid);
            const auto before = ObjectBytes(first);
            std::array<std::uint8_t, 5> masksBefore;
            std::copy(std::begin(shared.switchGroupMasks), std::end(shared.switchGroupMasks), masksBefore.begin());
            bool rejected = false;
            try { DispatchObjectCase(first, shared); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && ObjectBytes(first) == before &&
                std::equal(std::begin(shared.switchGroupMasks), std::end(shared.switchGroupMasks),
                    masksBefore.begin()),
                "Invalid group indices must fail explicitly without modifying object or masks.");
        }
        for (const auto behavior : { BehaviorType::Switch1, BehaviorType::SwitchGrouped })
        {
            first.behavior = behavior;
            first.health = 0;
            const auto before = ObjectBytes(first);
            std::array<std::uint8_t, 5> masksBefore;
            std::copy(std::begin(shared.switchGroupMasks), std::end(shared.switchGroupMasks), masksBefore.begin());
            Check(DispatchObjectCase(first, shared).status == DispatchStatus::InactiveObject &&
                ObjectBytes(first) == before, "Inactive switches must not update or access group state.");
            first.health = 1;
            shared.behaviorDispatchEnabled = false;
            const auto disabled = ObjectBytes(first);
            Check(DispatchObjectCase(first, shared).status == DispatchStatus::DispatchDisabled &&
                ObjectBytes(first) == disabled &&
                std::equal(std::begin(shared.switchGroupMasks), std::end(shared.switchGroupMasks),
                    masksBefore.begin()),
                "Disabled switches must not update or access group state.");
            shared.behaviorDispatchEnabled = true;
        }
    }

    void TestIdleAnimation()
    {
        RawGameObject object;
        std::array<std::uint16_t, 27> fixture;
        for (std::size_t i = 0; i < fixture.size(); ++i)
        {
            fixture[i] = static_cast<std::uint16_t>(0x9876u + i * 2);
        }
        std::memcpy(&object, fixture.data(), sizeof(object));
        object.behavior = BehaviorType::IdleAnimation;
        object.health = 1;
        object.itemValue = 3;
        object.blendWeightOrSpriteOpacity = 195;
        GameContext state;
        state.tickCount = 4;
        auto expected = object;
        expected.blendWeightOrSpriteOpacity = 7;
        const auto result = DispatchObjectCase(object, state);
        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x1666 &&
            result.behavior == BehaviorType::IdleAnimation, "IdleAnimation did not execute.");
        Check(ObjectBytes(object) == ObjectBytes(expected),
            "IdleAnimation must update only the blend-phase word.");
        Check(state.tickCount == 4 && state.behaviorDispatchEnabled && state.collisionMap == nullptr,
            "IdleAnimation must not modify shared state or require a collision map.");

        const auto checkArithmetic = [&](std::uint16_t rate, std::uint16_t phase, std::int16_t ticks)
        {
            object.itemValue = ReferenceSignedWord(rate);
            object.blendWeightOrSpriteOpacity = ReferenceSignedWord(phase);
            state.tickCount = ticks;
            const auto lowWord = (static_cast<std::uint64_t>(rate) * static_cast<std::uint16_t>(ticks) +
                phase + 200) % 65536;
            const auto signedSum = lowWord >= 32768 ? static_cast<std::int64_t>(lowWord) - 65536 :
                static_cast<std::int64_t>(lowWord);
            const auto remainder = signedSum < 0 ? -((-signedSum) % 200) : signedSum % 200;
            auto expectedObject = object;
            expectedObject.blendWeightOrSpriteOpacity = static_cast<std::int16_t>(remainder);
            Check(Function_IdleAnimation(object, state).status == DispatchStatus::ExecutedCase &&
                ObjectBytes(object) == ObjectBytes(expectedObject),
                "IdleAnimation differs from the original low-word multiply/add and signed remainder.");
        };
        for (const auto rate : { 0, 1, 3, 200, 0x7fff, 0x8000, 0xffff })
        {
            for (const auto phase : { 0, 100, 195, 199, 200, 0x7fff, 0x8000, 0xffff })
            {
                for (const auto ticks : { 0, 1, 4, 15, 32767 })
                {
                    checkArithmetic(static_cast<std::uint16_t>(rate), static_cast<std::uint16_t>(phase),
                        static_cast<std::int16_t>(ticks));
                }
            }
        }
        for (int bits = 0; bits <= 65535; ++bits)
        {
            for (const auto ticks : { 1, 32767 })
            {
                checkArithmetic(3, static_cast<std::uint16_t>(bits), static_cast<std::int16_t>(ticks));
                checkArithmetic(0xffff, static_cast<std::uint16_t>(bits), static_cast<std::int16_t>(ticks));
                checkArithmetic(static_cast<std::uint16_t>(bits), 195, static_cast<std::int16_t>(ticks));
                checkArithmetic(static_cast<std::uint16_t>(bits), 0x8000, static_cast<std::int16_t>(ticks));
            }
        }
        object.itemValue = 0;
        object.blendWeightOrSpriteOpacity = -300;
        state.tickCount = 0;
        Function_IdleAnimation(object, state);
        Check(object.blendWeightOrSpriteOpacity == -100, "Negative remainder must not be normalized positive.");
        object.health = 0;
        const auto inactive = ObjectBytes(object);
        Check(DispatchObjectCase(object, state).status == DispatchStatus::InactiveObject &&
            ObjectBytes(object) == inactive, "Inactive IdleAnimation must not change the phase.");
        object.health = 1;
        state.behaviorDispatchEnabled = false;
        const auto disabled = ObjectBytes(object);
        Check(DispatchObjectCase(object, state).status == DispatchStatus::DispatchDisabled &&
            ObjectBytes(object) == disabled, "Disabled IdleAnimation must not change the phase.");
    }

    void TestCollisionMap()
    {
        auto level = CreateDummyLevel();
        Check(level.collisionMap != nullptr, "Dummy level must own a heap-allocated collision map.");
        auto& map = *level.collisionMap;
        Check(sizeof(map) == 200 * 200 + 4, "Dummy allocation must contain only its header and pixels.");
        Check(map.widthMinusOne == 199 && map.heightMinusOne == 199,
            "Dummy level must contain a 200x200 collision map.");
        Check((static_cast<std::size_t>(map.widthMinusOne) + 1) * (map.heightMinusOne + 1) == 40000,
            "Dummy level must expose exactly 40000 logical collision pixels.");
        for (const auto value : map.data)
        {
            Check(value == 0, "Dummy level must initialize all collision-map storage to zero.");
        }
        auto* bytes = reinterpret_cast<unsigned char*>(&map);
        Check(map.data == bytes + 4,
            "Map pixels must begin immediately after the four-byte header.");
        map.widthMinusOne = 7;
        std::fill(std::begin(map.data), std::end(map.data), std::uint8_t{0xa5});
        GameContext context;
        context.collisionMap = &map;
        const auto before = map;
        const auto* beforeBytes = reinterpret_cast<const unsigned char*>(&before);
        context.SetCollisionMapPixelAtWorldXZ(137, 63, 0x1238);
        const std::size_t expected = 4 + 161 * 8 + 1;
        for (std::size_t i = 0; i < sizeof(map); ++i)
        {
            Check(bytes[i] == (i == expected ? 0x38 : beforeBytes[i]),
                "Point writer must overwrite exactly one byte with the low argument byte.");
        }

        RawGameObject object;
        object.behavior = BehaviorType::Pillar;
        object.health = 1;
        object.posX = 137;
        object.posY = -32768;
        object.posZ = 63;
        const auto objectBefore = ObjectBytes(object);
        for (const auto ticks : { 0, 1, 15 })
        {
            context.tickCount = static_cast<std::int16_t>(ticks);
            bytes[expected] = 0xff;
            Check(DispatchObjectCase(object, context).status == DispatchStatus::ExecutedCase,
                "Pillar did not execute its recovered case.");
            Check(bytes[expected] == 0x38 && ObjectBytes(object) == objectBefore,
                "Pillar must use X/Z, ignore Y/ticks, and leave the object unchanged.");
        }
        context.collisionMap = nullptr;
        object.health = 0;
        Check(DispatchObjectCase(object, context).status == DispatchStatus::InactiveObject,
            "Inactive Pillar must not access the map.");
        object.health = 1;
        context.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(object, context).status == DispatchStatus::DispatchDisabled,
            "Disabled Pillar must not access the map.");
        context.behaviorDispatchEnabled = true;
        bool rejected = false;
        try { DispatchObjectCase(object, context); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Missing map memory must be an explicit error.");

        // Independent modular-arithmetic oracle covers every signed X and Z input.
        const auto signed16 = [](std::int64_t value) -> std::int64_t
        {
            value = (value % 65536 + 65536) % 65536;
            return value >= 32768 ? value - 65536 : value;
        };
        context.collisionMap = &map;
        for (const auto strideHeader : { 0, 7, 0x7fff, 0xffff })
        {
            map.widthMinusOne = static_cast<std::uint16_t>(strideHeader);
            const auto stride = (strideHeader + 1) % 65536;
            for (int raw = 0; raw < 65536; ++raw)
            {
                const auto coordinate = static_cast<std::int16_t>(signed16(raw));
                for (const bool varyX : { false, true })
                {
                    const std::int16_t x = varyX ? coordinate : 137;
                    const std::int16_t z = varyX ? 63 : coordinate;
                    const auto column = signed16(static_cast<std::int64_t>(x) - 112) / 25;
                    const auto row = signed16(162 - signed16(static_cast<std::int64_t>(z) - 38) / 25);
                    const auto originalAddress =
                        (65534 + row * stride + column + 4 + 0x100000000LL) % 65536;
                    const auto address = static_cast<std::size_t>((originalAddress - 65534 + 65536) % 65536);
                    if (address >= sizeof(map))
                    {
                        bool outOfAllocation = false;
                        try { context.SetCollisionMapPixelAtWorldXZ(x, z, 0x1ab); }
                        catch (const std::out_of_range&) { outOfAllocation = true; }
                        Check(outOfAllocation, "Writes beyond the actual allocation must fail explicitly.");
                        continue;
                    }
                    const auto previous = bytes[address];
                    context.SetCollisionMapPixelAtWorldXZ(x, z, 0x1ab);
                    Check(bytes[address] == 0xab,
                        "Point writer differs from 16-bit coordinate/address arithmetic.");
                    bytes[address] = previous;
                }
            }
        }
        map.widthMinusOne = 7;
        context.SetCollisionMapPixelAtWorldXZ(62, 4088, 0x12);
        Check(bytes[2] == 0x12, "Original offset wrapping must still be able to reach the header.");
        rejected = false;
        try { context.SetCollisionMapPixelAtWorldXZ(-13, 4088, 0x34); }
        catch (const std::out_of_range&) { rejected = true; }
        Check(rejected, "A wrapped address outside the allocated map must fail explicitly.");
        const auto fresh = CreateDummyLevel();
        Check(fresh.collisionMap->widthMinusOne == 199 && fresh.collisionMap->data[expected - 4] == 0 &&
            fresh.collisionMap.get() != level.collisionMap.get(),
            "Dummy levels must not share mutable collision-map state.");
    }

    void TestCollisionMapReader()
    {
        auto level = CreateDummyLevel();
        auto& map = *level.collisionMap;
        GameContext context;
        context.collisionMap = &map;
        const GameContext& reader = context;
        Check(reader.GetCollisionMapPixelAtWorldXZ(137, 63) == 0,
            "Reader must return zero from a fresh dummy map.");
        context.SetCollisionMapPixelAtWorldXZ(137, 63, 0x1238);
        Check(reader.GetCollisionMapPixelAtWorldXZ(137, 63) == 0x38,
            "Reader and writer must address the same world-coordinate pixel.");
        for (std::size_t i = 0; i < CollisionMap::VARIABLE_SIZE; ++i)
        {
            map.data[i] = static_cast<std::uint8_t>(i * 37 + 11);
        }
        map.heightMinusOne = 0;
        const auto* bytes = reinterpret_cast<const unsigned char*>(&map);
        const auto signed16 = [](std::int64_t value) -> std::int64_t
        {
            value = (value % 65536 + 65536) % 65536;
            return value >= 32768 ? value - 65536 : value;
        };
        for (const auto widthHeader : { 0, 7, 199, 0x7fff, 0xfffe, 0xffff })
        {
            map.widthMinusOne = static_cast<std::uint16_t>(widthHeader);
            const auto before = map;
            const auto checkRead = [&](std::int16_t x, std::int16_t z)
            {
                const auto column = signed16(static_cast<std::int64_t>(x) - 112) / 25;
                const auto row = signed16(162 - signed16(static_cast<std::int64_t>(z) - 38) / 25);
                if (column < 0 || column > 319 || row < 0 || row > 239)
                {
                    Check(reader.GetCollisionMapPixelAtWorldXZ(x, z) == 0,
                        "Original hardcoded coordinate bounds must return zero.");
                    return;
                }
                const auto stride = (widthHeader + 1) % 65536;
                const auto address = static_cast<std::size_t>((4 + row * stride + column) % 65536);
                if (address >= sizeof(map))
                {
                    bool rejected = false;
                    try { reader.GetCollisionMapPixelAtWorldXZ(x, z); }
                    catch (const std::out_of_range&) { rejected = true; }
                    Check(rejected, "Reads beyond the host allocation must fail explicitly.");
                }
                else
                {
                    Check(reader.GetCollisionMapPixelAtWorldXZ(x, z) == bytes[address],
                        "Reader differs from the original coordinate, stride, or offset arithmetic.");
                }
            };
            for (int raw = 0; raw < 65536; ++raw)
            {
                const auto coordinate = static_cast<std::int16_t>(signed16(raw));
                checkRead(coordinate, 63);
                checkRead(137, coordinate);
            }
            for (int row = -1; row <= 240; ++row)
            {
                for (int column = -1; column <= 320; ++column)
                {
                    checkRead(static_cast<std::int16_t>(112 + column * 25),
                        static_cast<std::int16_t>(38 + (162 - row) * 25));
                }
            }
            const auto* beforeBytes = reinterpret_cast<const unsigned char*>(&before);
            Check(std::equal(bytes, bytes + sizeof(map), beforeBytes),
                "Reading pixels must not modify the header or data.");
        }
        context.collisionMap = nullptr;
        bool rejected = false;
        try { reader.GetCollisionMapPixelAtWorldXZ(137, 63); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Reading without collision-map memory must fail explicitly.");
    }

    void TestGatesAndWords()
    {
        RawGameObject object;
        GameContext context;
        object.behavior = BehaviorType::BatIndoor;
        for (const auto health : { 0, 0xffff, 0x8000 })
        {
            object.health = ReferenceSignedWord(health);
            Check(DispatchObjectCase(object, context).status == DispatchStatus::InactiveObject,
                "Health gate must use a signed > 0 check, not != 0.");
        }
        object.health = 1;
        context.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(object, context).status == DispatchStatus::DispatchDisabled,
            "Disabled behavior dispatch was executed.");
        context.behaviorDispatchEnabled = true;
        Check(DispatchObjectCase(object, context).moduleOffset == 0x161d, "BatIndoor anchor changed.");
        object.behavior = BehaviorType::Behavior_300;
        Check(DispatchObjectCase(object, context).moduleOffset == 0x5108, "Behavior 300 must not be truncated.");
        object.behavior = BehaviorType::DisabledSentinel;
        Check(DispatchObjectCase(object, context).moduleOffset == 0x1156, "FFFF sentinel changed.");
        Check(object.health == 0, "The FFFF sentinel must clear health.");
        object.health = 1;
        object.behavior = static_cast<BehaviorType>(0x010e);
        const auto unmapped = DispatchObjectCase(object, context);
        Check(unmapped.status == DispatchStatus::NoSpecificCase && unmapped.behavior == object.behavior &&
            FindBehavior(object.behavior) == nullptr,
            "A nonzero high byte must not route to BatIndoor.");
        context.tickCount = -1;
        bool rejected = false;
        try { DispatchObjectCase(object, context); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "Invalid tick count was accepted.");
    }

    void TestHeightfields()
    {
        auto level = CreateDummyLevel();
        GameContext state;
        state.level = &level;
        std::array<std::uint8_t, Level::GeometryStreamSize + 5> data{};
        data[0] = 49;
        data[2] = 39;
        constexpr auto planeSize = Level::Width * Level::Height;
        for (std::size_t plane = 0; plane < 7; ++plane)
        {
            for (std::size_t i = 0; i < planeSize; ++i)
            {
                data[4 + plane * planeSize + i] = static_cast<std::uint8_t>(plane * 31 + i * 7);
            }
        }
        level.collisionMap->data[127] = 123;
        level.LoadGeometry(data.data(), data.size());
        const auto checkPlanes = [&]()
        {
            for (std::size_t row = 0; row < Level::Height; ++row)
            {
                for (std::size_t column = 0; column < Level::Width; ++column)
                {
                    const auto index = row * Level::Width + column;
                    Check(level.wallTextures[row][column] == static_cast<std::uint8_t>(index * 7) &&
                        level.ceilingObjects[row][column] == static_cast<std::uint8_t>(62 + index * 7) &&
                        level.floorHeights[row][column] == static_cast<std::uint8_t>(93 + index * 7) &&
                        level.roomHeights[row][column] == static_cast<std::uint8_t>(124 + index * 7) &&
                        level.surfaceFlags[row][column] == static_cast<std::uint8_t>(155 + index * 7),
                        "Geometry plane offsets, orientation, or retained values differ from the renderer.");
                }
            }
            Check(level.collisionMap->data[127] == 123, "Geometry loading modified the independent collision map.");
        };
        checkPlanes();
        for (int invalid = 0; invalid < 4; ++invalid)
        {
            bool rejected = false;
            data[0] = invalid == 2 ? 50 : 49;
            data[3] = invalid == 3 ? 1 : 0;
            try
            {
                level.LoadGeometry(invalid == 0 ? nullptr : data.data(),
                    invalid == 1 ? Level::GeometryStreamSize - 1 : data.size());
            }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "Invalid or truncated geometry was accepted.");
            checkPlanes();
        }

        level = CreateDummyLevel();
        const auto referenceHeight = [](const std::array<int, 4>& corners, int dx, int dz)
        {
            const auto wx = static_cast<std::uint64_t>(dx * 661);
            const auto wz = static_cast<std::uint64_t>(dz * 661);
            const auto lower = (static_cast<std::uint16_t>(corners[3]) * wx +
                static_cast<std::uint16_t>(corners[2]) * (65535 - wx)) / 65536;
            const auto upper = (static_cast<std::uint16_t>(corners[1]) * wx +
                static_cast<std::uint16_t>(corners[0]) * (65535 - wx)) / 65536;
            return static_cast<int>((lower * (65535 - wz) + upper * wz) / 65536);
        };
        const std::array<int, 4> floor = { 10, 40, 70, 110 };
        const std::array<int, 4> room = { 22, 55, 66, 123 };
        for (std::size_t i = 0; i < 4; ++i)
        {
            level.floorHeights[13 + i / 2][11 + i % 2] = static_cast<std::uint8_t>(floor[i]);
            level.roomHeights[13 + i / 2][11 + i % 2] = static_cast<std::uint8_t>(room[i]);
        }
        for (int dx = 0; dx < 100; ++dx)
        {
            for (int dz = 0; dz < 100; ++dz)
            {
                const auto x = static_cast<std::int16_t>(1200 + dx);
                const auto z = static_cast<std::int16_t>(2700 + dz);
                Check(state.GetFloorHeight(x, 0, z) == -2 * referenceHeight(floor, dx, dz) &&
                    state.GetRoomHeight(x, z) == 2 * referenceHeight(room, dx, dz),
                    "Native fixed-point interpolation or Pascal row/column conversion is incorrect.");
            }
        }
        for (int raw = 0; raw <= 255; ++raw)
        {
            for (std::size_t i = 0; i < 4; ++i)
            {
                level.floorHeights[i / 2][i % 2] = static_cast<std::uint8_t>(raw);
                level.roomHeights[i / 2][i % 2] = static_cast<std::uint8_t>(raw);
            }
            Check(state.GetFloorHeight(100, 0, 4000) == -2 * std::max(0, raw - 2) &&
                state.GetRoomHeight(100, 4000) == 2 * std::max(0, raw - 2),
                "Native 65535-weight/truncation bias was replaced by floating-point interpolation.");
        }
        level.ceilingObjects[0][0] = 0;
        Check(state.GetRoomHeight(100, 4000) == 20000, "Missing ceiling must return the native relative-height sentinel.");
        level.ceilingObjects[39][49] = 0;
        Check(state.GetRoomHeight(5000, 100) == 20000,
            "No-ceiling lookup must not require unused neighboring samples at the level edge.");
        Check(state.GetFloorHeight(4900, 0, 200) == 0 && state.GetRoomHeight(4900, 200) == 96,
            "Last complete interpolation cell was rejected or mapped to the wrong row.");
        for (const auto x : { 0, 5000, 12799, 12800, -12900 })
        {
            bool rejected = false;
            bool overflow = false;
            try { state.GetFloorHeight(static_cast<std::int16_t>(x), 0, 4000); }
            catch (const std::overflow_error&) { rejected = true; overflow = true; }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected && overflow == (x == 12800 || x == -12900),
                "Height lookup failed to distinguish native byte-division overflow from sample bounds.");
        }
        bool rejected = false;
        try { state.GetRoomHeight(100, -12900); }
        catch (const std::overflow_error&) { rejected = true; }
        Check(rejected, "Room-height Z division must also detect native quotient overflow.");
        state.level = nullptr;
        rejected = false;
        try { state.GetRoomHeight(100, 4000); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Missing room geometry must not silently return zero.");
        state.level = &level;

        for (std::size_t i = 0; i < 4; ++i)
        {
            level.floorHeights[i / 2][i % 2] = 100;
            level.roomHeights[i / 2][i % 2] = 50;
        }
        level.surfaceFlags[0][0] = 1;
        rejected = false;
        try { state.GetFloorHeight(150, 0, 4050); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "Special-surface geometry must require its separate height/value metadata.");
        for (std::size_t slot = 0; slot < level.heightSurfaces.size(); ++slot)
        {
            level.heightSurfaces[slot] = Level::HeightSurface{
                static_cast<std::int16_t>(100 + slot), static_cast<std::uint8_t>(7 + slot) };
            level.surfaceFlags[0][0] = static_cast<std::uint8_t>(0x20u | (slot + 1));
            Check(state.GetFloorHeight(150, 0, 4050) == -100 - static_cast<int>(slot) &&
                state.floorSurfaceValue == 7 + static_cast<int>(slot),
                "Floor surface slots, flag mask, or shared lookup value are incorrect.");
        }
        level.surfaceFlags[0][0] = 1;
        for (const auto depth : { 0, 900, 901, -200, -32768 })
        {
            level.heightSurfaces[0] = Level::HeightSurface{ static_cast<std::int16_t>(depth), 42 };
            const auto flat = ReferenceSignedWord(-depth);
            Check(state.GetFloorHeight(150, 0, 4050) == (flat < -900 ? -196 : flat) &&
                state.floorSurfaceValue == 42, "Special-surface fallback threshold or signed negation is incorrect.");
        }
        level.surfaceFlags[0][0] = 5;
        Check(state.GetFloorHeight(150, 0, 4050) == -196 && state.floorSurfaceValue == -1,
            "Non-surface terrain must reset the surface lookup value.");
        level.surfaceFlags[0][0] = 28;
        Check(state.GetFloorHeight(150, -90, 4050) == -96 &&
            state.GetFloorHeight(150, -91, 4050) == -196,
            "Stacked-floor selection must use signed maximum, strict threshold, and arithmetic half-height.");
        for (std::size_t i = 0; i < 4; ++i) { level.floorHeights[i / 2][i % 2] = 20; }
        Check(state.GetFloorHeight(150, 100, 4050) == 64,
            "Negative stacked-floor corners must retain the native unsigned multiplication semantics.");
        for (std::size_t i = 0; i < 4; ++i) { level.floorHeights[i / 2][i % 2] = 100; }
        level.surfaceFlags[0][0] = 16;
        state.floorRepeatHeight = 50;
        Check(state.GetFloorHeight(150, -100, 4050) == -96 &&
            state.GetFloorHeight(150, 0, 4050) == 0 &&
            state.GetFloorHeight(150, -800, 4050) == -506,
            "Repeated-floor stepping or signed 0..255 clamp differs from the executable.");
        level.surfaceFlags[0][0] = 0;
        Check(state.GetFloorHeight(150, -100, 4050) == -196,
            "Zero surface flags must bypass repeated-floor adjustment.");
        level.surfaceFlags[0][0] = 28;
        for (std::size_t i = 0; i < 4; ++i) { level.floorHeights[i / 2][i % 2] = 20; }
        Check(state.GetFloorHeight(150, 100, 4050) == 0,
            "Negative stacked-floor corners must use signed remainder before repeated-floor stepping.");
    }

    void TestWaterDrop()
    {
        auto level = CreateDummyLevel();
        GameContext state;
        state.level = &level;
        RawGameObject drop;
        drop.behavior = BehaviorType::WaterDrop;
        drop.health = 1000;
        drop.posX = 150;
        drop.posZ = 4050;
        drop.posY = 100;
        drop.velocityY = -2;
        drop.generalPurpose2 = 4;
        drop.behaviorParameter = 0xab;
        drop.spriteScale = 2;
        drop.doorLockOrGroupBitmask = 2;
        auto expected = drop;
        expected.posY = 94;
        expected.spriteScale = 5;
        expected.displayType = DisplayType::SpriteTransparent;
        expected.blendWeightOrSpriteOpacity = 4;
        state.tickCount = 3;
        Check(DispatchObjectCase(drop, state).status == DispatchStatus::ExecutedCase &&
            ObjectBytes(drop) == ObjectBytes(expected),
            "WaterDrop growth must integrate old velocity first and preserve unrelated bytes.");
        DispatchObjectCase(drop, state);
        Check(drop.posY == 88 && drop.velocityY == -5 &&
            drop.behaviorParameter == 0xab && drop.spriteScale == 5,
            "WaterDrop did not accelerate only after exceeding its target size.");
        drop.posY = 1000;
        drop.velocityY = -15;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 955 && drop.velocityY == -16, "WaterDrop terminal velocity or integration order differs.");
        drop.posY = 100;
        drop.velocityY = -30;
        drop.generalPurpose1 = 1;
        state.tickCount = 3;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 10 && drop.velocityY == -30 && drop.generalPurpose1 == 65534 &&
            drop.behaviorParameter == 0xab && drop.spriteScale == 5,
            "WaterDrop waiting must wrap and suppress both growth and velocity clamp.");

        drop.posY = 100;
        drop.velocityY = 0;
        drop.generalPurpose1 = 0xffff;
        drop.behaviorParameter = 0xab;
        drop.spriteScale = 0xff;
        drop.doorLockOrGroupBitmask = 255;
        state.tickCount = 258;
        DispatchObjectCase(drop, state);
        Check(drop.behaviorParameter == 0xab && drop.spriteScale == 1 && drop.generalPurpose1 == 0xffff,
            "WaterDrop sprite scale must wrap using the low tick byte and signed timer test.");
        drop.behaviorParameter = 0;
        drop.spriteScale = 0;
        drop.doorLockOrGroupBitmask = -1;
        drop.posY = -100;
        drop.velocityY = -32768;
        state.tickCount = 1;
        DispatchObjectCase(drop, state);
        Check(drop.velocityY == 32767 && drop.posY == 32668,
            "WaterDrop acceleration must wrap before applying its signed terminal-speed comparison.");

        drop.posY = 1;
        drop.velocityY = -1;
        drop.generalPurpose1 = 100;
        drop.generalPurpose2 = 4;
        drop.animFrame = 9;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 0 && drop.generalPurpose2 == 4 && drop.animFrame == 9,
            "WaterDrop equality with floor must not count as impact.");
        drop.behaviorParameter = 0xab;
        drop.spriteScale = 0x17;
        drop.itemValue = 20;
        drop.winX = 1;
        state.tickCount = 7;
        for (int update = 1; update <= 4; ++update)
        {
            DispatchObjectCase(drop, state);
            if (update < 4)
            {
                Check(drop.posY == -1 && drop.velocityY == 0 && drop.animFrame == 2 &&
                    drop.generalPurpose2 == 4 - update && drop.winX == 0,
                    "WaterDrop splash must last four updates, not four ticks.");
            }
        }
        Check(drop.posY == 96 && drop.velocityY == 0 && drop.animFrame == 1 && drop.generalPurpose2 == 4 &&
            drop.generalPurpose1 == 20 && drop.behaviorParameter == 0xab && drop.spriteScale == 0 &&
            drop.winX == 1 && drop.health == 1000,
            "WaterDrop ceiling respawn did not restore timer, frame, size, and splash latch.");
        drop.posY = -1;
        drop.generalPurpose2 = 0;
        level.ceilingObjects[0][0] = 0;
        state.tickCount = 0;
        DispatchObjectCase(drop, state);
        Check(drop.posY == 20000 && drop.generalPurpose2 == 4 && drop.generalPurpose1 == 20,
            "WaterDrop zero-tick impact must still decrement its splash counter and use no-ceiling height.");

        level = CreateDummyLevel();
        drop.posY = 96;
        drop.velocityY = 0;
        drop.generalPurpose1 = 20;
        drop.generalPurpose2 = 4;
        drop.behaviorParameter = 0xab;
        drop.spriteScale = 0;
        drop.doorLockOrGroupBitmask = 2;
        drop.animFrame = 1;
        state.tickCount = 1;
        bool grew = false;
        bool fell = false;
        bool splashed = false;
        int cycleUpdates = 0;
        do
        {
            DispatchObjectCase(drop, state);
            ++cycleUpdates;
            grew = grew || (drop.behaviorParameter == 0xab && drop.spriteScale == 3);
            fell = fell || (drop.posY < 96 && drop.posY >= 0 && drop.velocityY < 0);
            splashed = splashed || drop.animFrame == 2;
        } while (cycleUpdates < 200 && !(splashed && drop.animFrame == 1));
        Check(grew && fell && splashed && cycleUpdates == 41 && drop.posY == 96 &&
            drop.generalPurpose1 == 20 && drop.generalPurpose2 == 4 && drop.behaviorParameter == 0xab && drop.spriteScale == 0,
            "WaterDrop did not complete the exact wait/grow/fall/four-update-splash/ceiling-reset cycle.");

        for (std::size_t i = 0; i < 4; ++i)
        {
            level.floorHeights[i / 2][i % 2] = 100;
            level.roomHeights[i / 2][i % 2] = 50;
        }
        level.surfaceFlags[0][0] = 28;
        drop.posY = -89;
        drop.velocityY = -20;
        drop.generalPurpose1 = 100;
        drop.generalPurpose2 = 4;
        state.tickCount = 1;
        DispatchObjectCase(drop, state);
        Check(drop.posY == -97 && drop.animFrame == 2 && drop.generalPurpose2 == 3,
            "WaterDrop floor lookup must use the position before motion, including stacked-floor selection.");

        const auto before = ObjectBytes(drop);
        state.level = nullptr;
        bool rejected = false;
        try { DispatchObjectCase(drop, state); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && ObjectBytes(drop) == before, "Missing level geometry must fail before mutating WaterDrop.");
        state.level = &level;
        level.surfaceFlags[0][0] = 1;
        rejected = false;
        try { DispatchObjectCase(drop, state); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && ObjectBytes(drop) == before, "Missing surface metadata must fail before mutating WaterDrop.");
        state.level = nullptr;
        state.behaviorDispatchEnabled = false;
        Check(DispatchObjectCase(drop, state).status == DispatchStatus::DispatchDisabled,
            "Disabled WaterDrop must not query missing geometry.");
        state.behaviorDispatchEnabled = true;
        drop.health = 0;
        Check(DispatchObjectCase(drop, state).status == DispatchStatus::InactiveObject,
            "Inactive WaterDrop must not query missing geometry.");
    }

    void TestRotationTables()
    {
        const auto fingerprint = [](const std::array<std::int32_t, 513>& table)
        {
            std::uint32_t hash = 2166136261u;
            for (const auto value : table)
            {
                const auto bits = static_cast<std::uint32_t>(value);
                for (int shift = 0; shift < 32; shift += 8)
                {
                    hash = (hash ^ ((bits >> shift) & 255u)) * 16777619u;
                }
            }
            return hash;
        };
        Check(fingerprint(SineTable) == 0x7ccfeecau && fingerprint(ClampedSineTable) == 0xf0dfdddau,
            "Rotation tables differ from the verified original x87-generated integers.");
        Check(SineTable[0] == 0 && SineTable[128] == 65536 && SineTable[256] == 0 &&
            SineTable[384] == -65536 && SineTable[512] == 0 &&
            ClampedSineTable[0] == 0 && ClampedSineTable[128] == 65536 &&
            ClampedSineTable[256] == 0 && ClampedSineTable[384] == -65536 &&
            ClampedSineTable[512] == 0, "Rotation table cardinal directions or extra endpoint differ.");
        Check(SineTable[64] == 46341 && ClampedSineTable[64] == 47376 &&
            ClampedSineTable[110] == 65372 && ClampedSineTable[111] == 65536 &&
            ClampedSineTable[145] == 65536 && ClampedSineTable[146] == 65372,
            "The 67000-amplitude table must retain its boost and exact clamped plateau boundaries.");
        for (std::size_t i = 0; i <= 512; ++i)
        {
            // Independent formula uses the original stored angular constant, not modern pi.
            const auto wave = std::sin(static_cast<double>(i) * 0.012271846484375);
            const auto standard = static_cast<std::int32_t>(std::lround(wave * 65536));
            const auto boosted = static_cast<std::int32_t>(std::lround(wave * 67000));
            Check(SineTable[i] == standard && ClampedSineTable[i] == std::clamp(boosted, -65536, 65536),
                "Rotation entries differ from the original angular step, amplitudes, or integer conversion.");
        }
        for (int angle = -32768; angle <= 32767; ++angle)
        {
            const auto sineIndex = static_cast<std::size_t>((angle % 512 + 512) % 512);
            const auto cosineIndex = static_cast<std::size_t>(((angle + 128) % 512 + 512) % 512);
            Check(Sine(static_cast<std::int16_t>(angle)) == ClampedSineTable[sineIndex] &&
                Cosine(static_cast<std::int16_t>(angle)) == ClampedSineTable[cosineIndex],
                "Native sine/cosine helpers must mask every word angle and use the clamped table.");
        }
    }

    void TestSoundEffects(const char* tablePath)
    {
        struct TestAudioHandler final : AudioHandler
        {
            std::array<bool, SoundChannelCount> playing{};
            std::uint16_t sampleCount = 129;
            std::size_t lastChannel = SoundChannelCount;
            std::uint8_t lastSample = 0;
            std::uint8_t lastVolume = 0;
            unsigned int starts = 0;
            bool failStart = false;
            std::uint16_t GetSampleCount() const override { return sampleCount; }
            void StopAllSamples() override { playing.fill(false); }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                Check(!playing.at(channel), "Audio admission attempted to interrupt an occupied channel.");
                if (failStart) { throw std::runtime_error("Test audio backend failure."); }
                lastChannel = channel;
                lastSample = sample;
                lastVolume = volume;
                ++starts;
                playing[channel] = true;
            }
        } audio;
        GameContext state;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 2, 1) == SoundPlaybackStatus::AudioUnavailable,
            "Unconfigured audio must report unavailable, not successful playback.");
        state.audioHandler = &audio;
        bool rejected = false;
        try { state.PlaySoundEffect(SoundEffect::Drop, 255, 2, 1); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected, "An attached audio backend requires its sound-effect table.");

        std::ifstream file(tablePath, std::ios::binary);
        Check(static_cast<bool>(file), "Cannot open original S_TAB.PRB.");
        const std::vector<std::uint8_t> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        state.LoadSoundEffects(bytes.data(), bytes.size());
        Check(state.soundEffectsLoaded && state.soundEffects[4].sampleCount == 2 &&
            state.soundEffects[4].samples[0] == 5 && state.soundEffects[4].samples[1] == 38,
            "Author-confirmed Drop effect must use the original two sample references.");
        const auto loaded = state.soundEffects;
        for (int invalid = 0; invalid < 6; ++invalid)
        {
            auto damaged = bytes;
            if (invalid == 2) { damaged[0] = 128; }
            if (invalid == 3) { damaged[2] = 4; }
            if (invalid == 4) { damaged[4 + 128 * 5] = 5; }
            if (invalid == 5) { damaged.push_back(0); }
            rejected = false;
            try
            {
                state.LoadSoundEffects(invalid == 0 ? nullptr : damaged.data(),
                    invalid == 1 ? damaged.size() - 1 : damaged.size());
            }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected && std::memcmp(loaded.data(), state.soundEffects.data(), sizeof(loaded)) == 0,
                "Invalid sound table was accepted or partially overwrote the loaded definitions.");
        }
        for (const auto id : { 0, 130, 65535 })
        {
            rejected = false;
            try { state.PlaySoundEffect(static_cast<SoundEffect>(id), 255, 2, 1); }
            catch (const std::out_of_range&) { rejected = true; }
            Check(rejected, "Invalid effect enum must be an explicit input error.");
        }

        auto& definition = state.soundEffects[4];
        for (std::uint8_t count = 1; count <= 4; ++count)
        {
            definition = { count, { 1, 2, 3, 4 } };
            for (std::uint32_t seed = 0; seed <= 65535; ++seed)
            {
                audio.playing.fill(false);
                state.randomSeed = seed;
                const auto next = static_cast<std::uint32_t>(
                    (static_cast<std::uint64_t>(seed) * 134775813u + 1u) & 0xffffffffu);
                Check(state.PlaySoundEffect(SoundEffect::Drop, 201, 2, 1) == SoundPlaybackStatus::Started &&
                    state.randomSeed == next && audio.lastSample == 1 + (next >> 16) % count &&
                    audio.lastChannel == 3 && audio.lastVolume == 201,
                    "Sound random selection, native highest-free channel, or sample/volume forwarding differs.");
            }
        }
        for (const auto seed : { 0x80000000u, 0xffffffffu, 0x12345678u })
        {
            audio.playing.fill(false);
            state.randomSeed = seed;
            const auto next = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(seed) * 134775813u + 1u) & 0xffffffffu);
            state.PlaySoundEffect(SoundEffect::Drop, 255, 2, 1);
            Check(state.randomSeed == next && audio.lastSample == 1 + (next >> 16) % 4,
                "Sound RNG must retain high seed bits and 32-bit overflow.");
        }
        state.LoadSoundEffects(bytes.data(), bytes.size());
        state.randomSeed = 0;
        for (std::int16_t source = 1; source <= 4; ++source)
        {
            audio.playing.fill(false);
            Check(state.PlaySoundEffect(SoundEffect::Drop, 255, source, 1) == SoundPlaybackStatus::Started &&
                (audio.lastSample == 5 || audio.lastSample == 38), "Original Drop variants were not forwarded.");
        }

        for (unsigned int occupancy = 0; occupancy < 16; ++occupancy)
        {
            for (const auto limit : { -3, -1, 0, 1, 2, 4, -32768 })
            {
                int freeCount = 0;
                int sameCount = 0;
                std::size_t lastFree = SoundChannelCount;
                for (std::size_t channel = 0; channel < SoundChannelCount; ++channel)
                {
                    audio.playing[channel] = (occupancy & (1u << channel)) != 0;
                    state.soundChannelSources[channel] = channel % 2 == 0 ? 7 : 8;
                    if (!audio.playing[channel]) { ++freeCount; lastFree = channel; }
                    else if (state.soundChannelSources[channel] == 7) { ++sameCount; }
                }
                const auto expected = limit < 0 && freeCount < 2 ? SoundPlaybackStatus::ReservedChannel :
                    freeCount == 0 ? SoundPlaybackStatus::NoFreeChannel :
                    ReferenceSignedWord(limit < 0 ? -limit : limit) <= sameCount ?
                    SoundPlaybackStatus::SourceLimitReached : SoundPlaybackStatus::Started;
                const auto starts = audio.starts;
                Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, static_cast<std::int16_t>(limit)) == expected,
                    "Four-channel occupancy, signed source limit, or reserved-channel rule differs.");
                Check(audio.starts == starts + (expected == SoundPlaybackStatus::Started ? 1u : 0u),
                    "Rejected playback touched the audio backend.");
                if (expected == SoundPlaybackStatus::Started)
                {
                    Check(audio.lastChannel == lastFree && state.soundChannelSources[lastFree] == 7,
                        "Playback did not select and tag the last free channel.");
                }
            }
        }
        audio.playing.fill(false);
        state.soundEffects[4] = { 1, { 5, 0, 0, 0 } };
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::Started,
            "Initial per-source sound was rejected.");
        state.soundEffects[4] = { 1, { 38, 0, 0, 0 } };
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::SourceLimitReached,
            "Source concurrency must count other samples, not just duplicates of the selected sample.");
        audio.playing[3] = false;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::Started,
            "Finished audio channel was not reusable.");

        audio.playing.fill(false);
        const auto starts = audio.starts;
        state.randomSeed = 0;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 0, 7, 1) == SoundPlaybackStatus::Inaudible &&
            state.randomSeed == 1, "Inaudible requests must still consume native sample selection.");
        state.soundEnabled = false;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::Disabled,
            "Disabled sound must be reported explicitly.");
        state.soundEnabled = true;
        audio.sampleCount = 4;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::NoSample,
            "Missing backend sample must not be passed to PlaySample.");
        audio.sampleCount = 129;
        state.soundEffects[4].sampleCount = 0;
        const auto seedBefore = state.randomSeed;
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::NoSample &&
            state.randomSeed == seedBefore && audio.starts == starts,
            "Empty sample lists must not draw random values or start audio.");
        state.soundEffects[4] = { 1, { 0, 0, 0, 0 } };
        Check(state.PlaySoundEffect(SoundEffect::Drop, 255, 7, 1) == SoundPlaybackStatus::NoSample,
            "Zero sample reference must follow the original no-sample gate.");
        state.LoadSoundEffects(bytes.data(), bytes.size());
        audio.failStart = true;
        const auto sources = state.soundChannelSources;
        rejected = false;
        try { state.PlaySoundEffect(SoundEffect::Drop, 255, 9, 1); }
        catch (const std::runtime_error&) { rejected = true; }
        Check(rejected && sources == state.soundChannelSources, "Backend errors must propagate without publishing a source.");
        audio.failStart = false;
        RawGameObject foreign;
        rejected = false;
        try { state.PlaySoundEffect(SoundEffect::Drop, 255, foreign, 1); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "An object outside worldObjects cannot supply an original source slot.");

        const auto referenceVolume = [](int x, int z, int scale, int listenerX, int listenerZ)
        {
            const auto absolute = [](int value)
            {
                const auto word = ReferenceSignedWord(value);
                return word < 0 ? ReferenceSignedWord(-word) : word;
            };
            const auto distance = ReferenceSignedWord(absolute(listenerX - x) + absolute(listenerZ - z));
            const auto scaled = ReferenceSignedWord(distance * scale);
            const auto magnitude = static_cast<std::uint16_t>(absolute(scaled));
            return std::clamp(355 - static_cast<int>(magnitude / 2), 0, 255);
        };
        state.soundListenerX = -1234;
        state.soundListenerZ = 2700;
        for (int coordinate = -32768; coordinate <= 32767; ++coordinate)
        {
            for (const auto scale : { 1, 3 })
            {
                const auto value = static_cast<std::int16_t>(coordinate);
                Check(state.GetSoundVolumeAt(value, -300, static_cast<std::int16_t>(scale)) ==
                    referenceVolume(value, -300, scale, -1234, 2700) &&
                    state.GetSoundVolumeAt(300, value, static_cast<std::int16_t>(scale)) ==
                    referenceVolume(300, value, scale, -1234, 2700),
                    "Positional volume differs in native absolute value, Manhattan distance, scaling, or word wrapping.");
            }
        }
        state.soundListenerX = 0;
        state.soundListenerZ = 0;
        Check(state.GetSoundVolumeAt(200, 0) == 255 && state.GetSoundVolumeAt(201, 0) == 255 &&
            state.GetSoundVolumeAt(202, 0) == 254 && state.GetSoundVolumeAt(708, 0) == 1 &&
            state.GetSoundVolumeAt(710, 0) == 0, "Native sound-volume thresholds changed.");
        state.positionalSoundsSuppressed = true;
        Check(state.GetSoundVolumeAt(0, 0) == 0 && state.GetSoundVolumeAt(0, 0, 3) == 0,
            "DS:D4FA positional suppression must force distance 10000.");
        state.positionalSoundsSuppressed = false;

        auto level = CreateDummyLevel();
        state.level = &level;
        auto& drop = state.worldObjects[2];
        drop.behavior = BehaviorType::WaterDrop;
        drop.health = 1000;
        drop.posX = 150;
        drop.posZ = 4050;
        drop.posY = -1;
        drop.generalPurpose2 = 4;
        drop.winX = 1;
        state.soundListenerX = drop.posX;
        state.soundListenerZ = drop.posZ;
        audio.playing.fill(false);
        const auto beforeImpact = audio.starts;
        DispatchObjectCase(drop, state);
        Check(audio.starts == beforeImpact + 1 && state.soundChannelSources[3] == 2 &&
            audio.lastVolume == 255 && drop.winX == 0,
            "WaterDrop impact must play the named Drop effect using its original object slot.");
        for (int i = 0; i < 3; ++i) { DispatchObjectCase(drop, state); }
        Check(audio.starts == beforeImpact + 1 && drop.winX == 1 && drop.posY == 96,
            "Splash updates must not repeat audio; ceiling reset must rearm it.");
        audio.playing.fill(false);
        drop.posY = -1;
        DispatchObjectCase(drop, state);
        Check(audio.starts == beforeImpact + 2, "Next WaterDrop cycle did not play its splash sound.");
    }

    void TestAgainstOriginal(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        Check(static_cast<bool>(file), "Cannot open the original executable for differential routing tests.");
        const std::vector<std::uint8_t> bytes{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        Check(bytes.size() == 203760 && bytes[0] == 'M' && bytes[1] == 'Z',
            "Unexpected original executable.");
        MessageContext promptContext;
        auto door = MakeTestDoor();
        door.doorLockOrGroupBitmask = 7;
        auto& key = promptContext.inventory[0];
        key.health = 1000;
        key.collisionType = CollisionType::Key;
        key.itemValue = 8;
        HandlePlayerObjectInteraction(door, promptContext);
        const std::size_t wrongKeyMessageOffset = 0x175bb;
        const std::string originalWrongKeyMessage(
            bytes.begin() + wrongKeyMessageOffset + 1,
            bytes.begin() + wrongKeyMessageOffset + 1 + bytes[wrongKeyMessageOffset]);
        Check(promptContext.userMessage == originalWrongKeyMessage,
            "Wrong-key prompt bytes must match the original executable's Pascal string exactly.");
        const auto word = [&](std::size_t at) -> int
        {
            Check(at < bytes.size() - 1, "Word is outside the executable.");
            return bytes[at] | (static_cast<int>(bytes[at + 1]) << 8);
        };
        const auto header = static_cast<std::size_t>(word(8) * 16);
        const auto originalEntry = [&](int behavior)
        {
            int ip = 0x1151;
            int compared = -1;
            for (int step = 0; step < 1000; ++step)
            {
                if (ip == 0x5110) { return ip; }
                const auto at = header + static_cast<std::size_t>(ip);
                Check(at < bytes.size() - 3, "Selector escaped executable bounds.");
                const int op = bytes[at];
                if (op == 0x3d)
                {
                    compared = word(at + 1);
                    ip += 3;
                }
                else if (op == 0x83 && bytes[at + 1] == 0xf8)
                {
                    const int immediate = bytes[at + 2];
                    compared = (immediate < 128 ? immediate : immediate - 256) & 0xffff;
                    ip += 3;
                }
                else if (op == 0xe9)
                {
                    const int delta = word(at + 1);
                    ip = (ip + 3 + (delta < 32768 ? delta : delta - 65536)) & 0xffff;
                }
                else if (op == 0xeb || op == 0x74 || op == 0x75)
                {
                    const int delta = bytes[at + 1];
                    Check(op == 0xeb || compared >= 0, "Selector branch lacks a comparison.");
                    const bool take = op == 0xeb || (op == 0x74 ? behavior == compared : behavior != compared);
                    ip = (ip + 2 + (take ? (delta < 128 ? delta : delta - 256) : 0)) & 0xffff;
                }
                else
                {
                    return ip;
                }
            }
            throw std::runtime_error("Selector interpreter exceeded its instruction bound.");
        };
        GameContext context;
        auto level = CreateDummyLevel();
        context.collisionMap = level.collisionMap.get();
        context.level = &level;
        for (int behavior = 0; behavior <= 65535; ++behavior)
        {
            // Each route needs a fresh active object; a previous handler can legitimately expire it.
            RawGameObject object;
            object.health = 1;
            object.posX = 150;
            object.posZ = 4050;
            object.behavior = static_cast<BehaviorType>(behavior);
            if (object.behavior == BehaviorType::MummieGuard) { object.soundEffectId = 15; }
            level.heightSurfaces[0] = Level::HeightSurface{ 0, 0 };
            const auto result = DispatchObjectCase(object, context);
            Check(result.moduleOffset == originalEntry(behavior) && result.behavior == static_cast<BehaviorType>(behavior),
                "C++ routing differs from the original executable's selector instructions.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        TestObjectLayout();
        TestCases();
        TestInputAbstraction();
        TestLightingCache();
        TestLightSource();
        TestPulsatingLight();
        TestFlies();
        TestUserMessages();
        TestDoorCollisionMap();
        TestDoorUnlocking();
        TestDoorUpdates();
        TestDoorSounds();
        TestObjectAllocation();
        TestObjectUpdates();
        TestSharedBurning();
        TestGatesAndWords();
        TestCollisionMap();
        TestCollisionMapReader();
        TestIdleAnimation();
        TestGameMath();
        TestRotationTables();
        TestSmokeFading();
        TestSwitches();
        TestObjectInteractions();
        TestPlayerInteractionLoop();
        TestHeightfields();
        TestWaterDrop();
        Check(argc == 3, "Expected original executable and sound-table paths.");
        TestSoundEffects(argv[2]);
        TestAgainstOriginal(argv[1]);
        std::cout << "PASS: 190 catalog routes and all 65536 selector words match the original; "
            "signed gates, object state, collision-map reads/writes, Pillar, LightSource, PulsatingLight, Flies, doors/keys/prompts, IdleAnimation, SmokeFading, switches, "
            "inventory/pickup interactions, collision/input routing, geometry streams, height lookups, WaterDrop, sound "
            "variants/attenuation/channel admission, both rotation tables and all word-angle lookups, "
            "and exhaustive arithmetic checks verified.\n";
        return 0;
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
