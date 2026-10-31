#include "BehaviorDispatch.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace Desperabis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename Exception, typename Action>
    void Reject(Action action)
    {
        try { action(); }
        catch (const Exception&) { return; }
        throw std::runtime_error("Expected ambient input rejection.");
    }

    struct Placement
    {
        std::uint8_t sample;
        std::size_t offset;
        std::uint8_t weight;
    };

    struct Audio final : AudioHandler
    {
        std::array<std::size_t, 130> lengths{};
        std::vector<Placement> copies;
        std::vector<Placement> mixes;
        std::size_t capacity = 0;
        std::size_t loopLength = 0;
        int starts = 0;
        int stops = 0;
        bool failMix = false;

        std::uint16_t GetSampleCount() const override { return 129; }
        bool IsChannelPlaying(std::size_t) const override { return false; }
        void PlaySample(std::size_t, std::uint8_t, std::uint8_t) override {}
        void StopAllSamples() override {}
        std::size_t GetSampleLength(std::uint8_t sample) const override { return lengths.at(sample); }
        void BeginAmbientLoopCreation(std::size_t size) override
        {
            capacity = size;
            loopLength = 0;
            copies.clear();
            mixes.clear();
        }
        void CopyAmbient(std::uint8_t sample, std::size_t offset) override
        {
            Check(offset <= capacity && lengths[sample] <= capacity - offset, "Base exceeds ambient allocation.");
            copies.push_back({sample, offset, 0});
        }
        void MixAmbient(std::uint8_t sample, std::size_t offset, std::uint8_t weight) override
        {
            if (failMix) { throw std::runtime_error("Injected ambient mix failure."); }
            mixes.push_back({sample, offset, weight});
        }
        void EndAmbientLoopCreation(std::size_t length) override { loopLength = length; }
        void StartAmbientPlayback() override
        {
            Check(loopLength != 0, "Ambient started before finalization.");
            ++starts;
        }
        void StopAmbientPlayback() override { ++stops; }
    };

    std::uint32_t Next(std::uint32_t& seed)
    {
        seed = seed * 134775813u + 1u;
        return seed;
    }

    std::size_t ExpectedOffset(std::uint32_t& seed, std::size_t maximum)
    {
        const auto bits = Next(seed);
        const auto magnitude = bits <= 0x7fffffffu ? static_cast<std::uint64_t>(bits) : 0x100000000ull - bits;
        const auto product = magnitude * maximum;
        auto rounded = product >> 31;
        const auto remainder = product & 0x7fffffffu;
        if (remainder > 0x40000000u || (remainder == 0x40000000u && (rounded & 1u) != 0)) { ++rounded; }
        return static_cast<std::size_t>(rounded) & ~std::size_t{1};
    }

    void TestRecipes(Audio& audio)
    {
        GameContext realRandom;
        realRandom.randomSeed = 0x26b05733u;
        Check(realRandom.RandomReal() == 0.0 && realRandom.randomSeed == 0,
            "Real RNG must handle a zero next seed.");
        realRandom.randomSeed = 0xa6b05733u;
        Check(realRandom.RandomReal() == 1.0 && realRandom.randomSeed == 0x80000000u,
            "Real RNG must safely handle ABS of the most negative signed seed.");
        realRandom.randomSeed = 0x4d60ae66u;
        Check(realRandom.RandomReal() == 1.0 / 2147483648.0 && realRandom.randomSeed == 0xffffffffu,
            "Real RNG must use signed absolute value, not unsigned division by 2^32.");
        const std::array<std::uint8_t, 5> bases{46, 30, 28, 28, 62};
        const std::array<std::uint16_t, 5> gaps{0, 0, 13, 4, 0};
        struct Overlay { std::uint8_t sample; int count; };
        const std::array<std::vector<Overlay>, 5> overlays{{
            {{5, 50}, {38, 50}}, {{31, 20}}, {{82, 100}, {29, 10}, {40, 4}},
            {{82, 100}, {44, 4}, {73, 8}}, {{62, 140}, {63, 140}, {64, 140}}
        }};
        for (std::uint8_t environment = 0; environment < 5; ++environment)
        {
            for (const std::uint32_t initialSeed : {0u, 123u, 0xffffffffu})
            {
                GameContext core;
                core.audioHandler = &audio;
                core.randomSeed = initialSeed;
                core.CreateAmbientLoop(environment);
                Check(audio.capacity == 1800000 && audio.loopLength > 1400000 &&
                    audio.loopLength < 1540000, "Unexpected ambient capacity/endpoint.");
                auto seed = initialSeed;
                std::size_t cursor = 0;
                for (const auto& copy : audio.copies)
                {
                    if (gaps[environment] != 0 && (Next(seed) >> 16) % gaps[environment] == 0)
                    {
                        cursor += 4u * ((Next(seed) >> 16) % 10000);
                    }
                    Check(copy.sample == bases[environment] && copy.offset == cursor, "Base/gap sequence differs.");
                    cursor += audio.lengths[copy.sample];
                }
                Check(cursor >= 1499904 && audio.loopLength == cursor - audio.lengths[bases[environment]],
                    "Final copied sample must be excluded from logical loop length.");
                std::size_t mixIndex = 0;
                for (const auto& [sample, count] : overlays[environment])
                {
                    for (int i = 0; i < count; ++i)
                    {
                        const auto offset = ExpectedOffset(seed, audio.loopLength - audio.lengths[sample] - 100);
                        const auto weight = 200 - (Next(seed) >> 16) % 150;
                        const auto& mix = audio.mixes.at(mixIndex++);
                        Check(mix.sample == sample && mix.offset == offset && mix.weight == weight &&
                            mix.weight >= 51 && mix.weight <= 200 && (mix.offset & 1) == 0 &&
                            mix.offset + audio.lengths[sample] + 100 <= audio.loopLength,
                            "Overlay order, weight, alignment or offset differs from native arithmetic.");
                    }
                }
                Check(audio.mixes.size() == mixIndex && core.randomSeed == seed, "Ambient RNG consumption differs.");
            }
        }
        GameContext core;
        core.audioHandler = &audio;
        core.CreateAmbientLoop(99);
        Check(audio.copies.front().sample == 46, "Non-special selectors must use the indoor recipe.");
        for (const auto selector : {100, 101, 102, 103, 150})
        {
            Reject<std::runtime_error>([&] { core.CreateAmbientLoop(static_cast<std::uint8_t>(selector)); });
        }
        const auto original = audio.lengths[46];
        for (const std::size_t invalid : {std::size_t{0}, std::size_t{64001}, (std::numeric_limits<std::size_t>::max)()})
        {
            audio.lengths[46] = invalid;
            Reject<std::invalid_argument>([&] { core.CreateAmbientLoop(0); });
        }
        audio.lengths[46] = original;
        core.audioHandler = nullptr;
        Reject<std::logic_error>([&] { core.CreateAmbientLoop(0); });
    }

    class Host final : public GameContext
    {
    public:
        std::uint8_t selector = 0;
        std::filesystem::path original;
        bool deferLoad = false;
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream input(original / std::filesystem::path(filename), std::ios::binary);
            if (!input) { throw std::runtime_error("Missing ambient level fixture."); }
            return {std::istreambuf_iterator<char>(input), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
            if (deferLoad) { return; }
            LevelLoadData data;
            LoadLevelGeometry(request.levelNumber, data);
            Check(data.ambientSound == selector, "Geometry loader lost level-info ambient selector.");
            CompleteLevelLoad(request.id, std::move(data));
        }
    };

    void TestLevelLoading(Audio& audio, const std::filesystem::path& original)
    {
        Host core;
        core.original = original;
        core.audioHandler = &audio;
        int count = 0;
        struct LevelCase { int level; std::uint8_t selector; };
        for (const auto [level, selector] : std::array<LevelCase, 5>{{
            {50, 0}, {10, 1}, {16, 2}, {30, 3}, {25, 4}}})
        {
            core.selector = selector;
            core.StartLevel(static_cast<std::int16_t>(level));
            ++count;
            Check(audio.starts == count && audio.stops == count - 1 &&
                core.GetRuntimeState() == GameRuntimeState::Ready, "Level transition must replace/start ambient.");
        }
        core.soundEnabled = false;
        core.StartLevel(25);
        Check(audio.starts == count && audio.stops == count, "Disabled sound must stop, not generate/play ambient.");
        core.soundEnabled = true;
        audio.failMix = true;
        Reject<std::runtime_error>([&] { core.StartLevel(25); });
        Check(core.GetRuntimeState() != GameRuntimeState::Ready && !core.GetLevelLoadError().empty() &&
            audio.starts == count, "Ambient creation failure must fail the level load without starting playback.");
        audio.failMix = false;
        core.StartLevel(25);
        Check(audio.starts == count + 1, "Ambient level load must recover after a failed creation pass.");
        core.deferLoad = true;
        const auto request = core.StartLevel(25);
        Check(audio.starts == count + 1 && audio.stops == count + 1 &&
            core.GetRuntimeState() == GameRuntimeState::Loading,
            "The core must stop old ambient at request time, before an asynchronous provider completes.");
        core.FailLevelLoad(request.id, "Deferred fixture load failed.");
        Check(audio.starts == count + 1, "Failed asynchronous loading must not restart stale ambient.");
    }

    void TestPositionalMarkers(const std::filesystem::path& original)
    {
        struct MarkerAudio final : AudioHandler
        {
            struct Playback { std::size_t channel; std::uint8_t sample; std::uint8_t volume; };
            std::vector<Playback> plays;
            std::array<bool, SoundChannelCount> playing{};
            std::uint16_t GetSampleCount() const override { return 129; }
            bool IsChannelPlaying(std::size_t channel) const override { return playing.at(channel); }
            void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override
            {
                plays.push_back({ channel, sample, volume });
                playing.at(channel) = true;
            }
            void StopAllSamples() override { playing.fill(false); }
        } audio;
        std::ifstream objects(original / "LEVEL" / "OBJPOS50.DAT", std::ios::binary);
        Check(static_cast<bool>(objects), "Cannot open tutorial ambient object fixtures.");
        const std::vector<std::uint8_t> objectBytes((std::istreambuf_iterator<char>(objects)), {});
        Check(objectBytes.size() >= 133 * 54, "Tutorial marker object table is truncated.");
        std::ifstream table(original / "SAMP" / "S_TAB.PRB", std::ios::binary);
        Check(static_cast<bool>(table), "Cannot open original ambient sound definitions.");
        const std::vector<std::uint8_t> definitions((std::istreambuf_iterator<char>(table)), {});
        GameContext core;
        core.LoadSoundEffects(definitions.data(), definitions.size());
        core.audioHandler = &audio;
        for (const int slot : { 100, 101 })
        {
            const auto originalMarker = DecodeGameObject(objectBytes.data() + (slot - 2) * 54, 54);
            Check(originalMarker.behavior == BehaviorType::AmbientSoundPosition &&
                originalMarker.soundEffectId == slot - 13 && originalMarker.generalPurpose3 == 9 &&
                originalMarker.doorLockOrGroupBitmask == 2,
                "Tutorial positional marker fields differ from native fixture.");
            core.soundListenerX = originalMarker.posX;
            core.soundListenerZ = originalMarker.posZ;
            const auto& definition = core.soundEffects[originalMarker.soundEffectId - 1];
            Check(definition.sampleCount > 0, "Original positional marker must reference playable samples.");
            for (const int threshold : { -32768, -1, 0, 9, 999, 1000, 32767 })
            {
                for (const int ticks : { 0, 1, 77 })
                {
                    for (std::uint32_t initialSeed = 0; initialSeed < 256; ++initialSeed)
                    {
                        auto& marker = core.worldObjects[slot];
                        marker = originalMarker;
                        marker.generalPurpose3 = static_cast<std::uint16_t>(threshold);
                        const auto before = marker;
                        core.tickCount = static_cast<std::int16_t>(ticks);
                        core.randomSeed = initialSeed;
                        audio.plays.clear();
                        audio.playing.fill(false);
                        auto seed = initialSeed;
                        const bool attempt = threshold > static_cast<int>((Next(seed) >> 16) % 1000);
                        const auto sample = attempt ? definition.samples[(Next(seed) >> 16) % definition.sampleCount] : 0;
                        const auto result = DispatchObjectCase(marker, core);
                        Check(result.status == DispatchStatus::ExecutedCase && result.moduleOffset == 0x4EAF &&
                            core.randomSeed == seed && std::memcmp(&marker, &before, sizeof(marker)) == 0,
                            "Ambient marker must use one signed-threshold RNG attempt per frame, without mutating its record.");
                        Check(audio.plays.size() == (attempt ? 1u : 0u),
                            "Ambient marker probability/zero-tick behavior differs from native reference.");
                        if (attempt)
                        {
                            Check(audio.plays[0].sample == sample && audio.plays[0].volume == 255 &&
                                audio.plays[0].channel == SoundChannelCount - 1 &&
                                core.soundChannelSources[audio.plays[0].channel] == slot,
                                "Ambient marker must preserve sample RNG, volume, source slot and channel admission.");
                        }
                    }
                }
            }
            auto& marker = core.worldObjects[slot];
            marker = originalMarker;
            marker.generalPurpose3 = 1000;
            core.tickCount = 0;
            core.randomSeed = 17;
            core.soundEnabled = false;
            audio.plays.clear();
            const auto seedBeforeMute = core.randomSeed;
            DispatchObjectCase(marker, core);
            Check(core.randomSeed == seedBeforeMute && audio.plays.empty(),
                "Native sound-enable gate must suppress marker RNG as well as playback.");
            core.soundEnabled = true;
            core.behaviorDispatchEnabled = false;
            DispatchObjectCase(marker, core);
            Check(core.randomSeed == seedBeforeMute && audio.plays.empty(), "Disabled dispatch must not update ambient markers.");
            core.behaviorDispatchEnabled = true;
            marker.health = 0;
            DispatchObjectCase(marker, core);
            Check(core.randomSeed == seedBeforeMute && audio.plays.empty(), "Inactive marker must not consume RNG.");
            marker.health = 1000;
            audio.playing.fill(false);
            core.soundListenerX = static_cast<std::int16_t>(marker.posX + 708);
            DispatchObjectCase(marker, core);
            Check(audio.plays.size() == 1 && audio.plays[0].volume == 1, "Marker volume must use native positional attenuation.");
            audio.plays.clear();
            core.soundListenerX = static_cast<std::int16_t>(marker.posX + 710);
            auto inaudibleSeed = core.randomSeed;
            Next(inaudibleSeed);
            Next(inaudibleSeed);
            DispatchObjectCase(marker, core);
            Check(audio.plays.empty() && core.randomSeed == inaudibleSeed,
                "Inaudible marker must retain probability and sample RNG draws.");
            core.soundListenerX = marker.posX;
            audio.playing.fill(false);
            audio.plays.clear();
            DispatchObjectCase(marker, core);
            DispatchObjectCase(marker, core);
            DispatchObjectCase(marker, core);
            Check(audio.plays.size() == 2, "Marker field 46 must enforce its native per-source instance limit.");
            audio.plays.clear();
            core.audioHandler = nullptr;
            auto unavailableSeed = core.randomSeed;
            Next(unavailableSeed);
            Next(unavailableSeed);
            DispatchObjectCase(marker, core);
            Check(core.randomSeed == unavailableSeed && audio.plays.empty(),
                "Enabled markers must preserve RNG with an unavailable modern audio backend.");
            core.audioHandler = &audio;
            marker.soundEffectId = 0;
            Reject<std::out_of_range>([&] { DispatchObjectCase(marker, core); });
        }
        for (const int slot : { 132, 133, 134 })
        {
            auto& marker = core.worldObjects[slot];
            marker = DecodeGameObject(objectBytes.data() + (slot - 2) * 54, 54);
            Check(marker.behavior == BehaviorType::UnderwaterColorAnim, "Expected original underwater marker.");
            const auto before = marker;
            const auto seed = core.randomSeed;
            core.tickCount = 77;
            core.playerUnderwater = true;
            const auto lighting = core.lightingCache.contributions;
            const auto result = DispatchObjectCase(marker, core);
            Check(result.status == DispatchStatus::UnimplementedHandler &&
                std::memcmp(&marker, &before, sizeof(marker)) == 0 &&
                core.randomSeed == seed && core.lightingCache.contributions == lighting,
                "Underwater marker must remain entirely unimplemented pending renderer integration.");
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original asset directory.");
        const std::filesystem::path original(argv[1]);
        Audio audio;
        for (std::size_t id = 1; id < audio.lengths.size(); ++id)
        {
            audio.lengths[id] = static_cast<std::size_t>(std::filesystem::file_size(
                original / "SAMP" / ("SOUND" + std::to_string(id) + ".SMP")));
        }
        TestRecipes(audio);
        TestLevelLoading(audio, original);
        TestPositionalMarkers(original);
        std::cout << "Ambient tests passed: recipes, positional markers, RNG, bounds, original metadata and level lifecycle.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
