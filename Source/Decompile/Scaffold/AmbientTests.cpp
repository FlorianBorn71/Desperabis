#include "GameContext.h"

#include <array>
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
    protected:
        std::vector<std::uint8_t> LoadFile(std::string_view filename) override
        {
            std::ifstream input(original / std::filesystem::path(filename), std::ios::binary);
            if (!input) { throw std::runtime_error("Missing ambient level fixture."); }
            return {std::istreambuf_iterator<char>(input), {}};
        }
        void LoadLevel(const LevelLoadRequest& request) override
        {
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
            Check(audio.starts == count && audio.stops == count &&
                core.GetRuntimeState() == GameRuntimeState::Ready, "Level transition must replace/start ambient.");
        }
        core.soundEnabled = false;
        core.StartLevel(25);
        Check(audio.starts == count && audio.stops == count + 1, "Disabled sound must stop, not generate/play ambient.");
        core.soundEnabled = true;
        audio.failMix = true;
        Reject<std::runtime_error>([&] { core.StartLevel(25); });
        Check(core.GetRuntimeState() != GameRuntimeState::Ready && !core.GetLevelLoadError().empty() &&
            audio.starts == count, "Ambient creation failure must fail the level load without starting playback.");
        audio.failMix = false;
        core.StartLevel(25);
        Check(audio.starts == count + 1, "Ambient level load must recover after a failed creation pass.");
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
        std::cout << "Ambient tests passed: all recipes, RNG, bounds, original metadata and level lifecycle.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
