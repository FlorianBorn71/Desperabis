#include "pch.h"
#include "AudioPlayer.h"
#include <Asset/AssetManager.h>
#include <IO/FileManagerBuiltIn.h>
#include <GameContext.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <xaudio2fx.h>

// This fixture registers only SAMP; no graphics resources or device are needed.
void RegisterFiles_BuiltInShaders() {}
void RegisterFiles_BuiltInANIMS() {}
void RegisterFiles_BuiltInGRAFIK() {}
void RegisterFiles_BuiltInLEVEL() {}
void RegisterFiles_BuiltInVEKT() {}
void RegisterFiles_BuiltInMapIcons() {}

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
        throw std::runtime_error("Expected audio input rejection.");
    }

    class TestRenderer final : public Renderer
    {
    public:
        TestRenderer() { g_Instance = this; }
        ~TestRenderer() { g_Instance = nullptr; }
        void RenderSceneElements(const VisibilityResult&, const RenderingSettings&, ProfilingStats&) override {}
        void CreateTexture(Texture&, TextureLoadingContext&) override { throw std::runtime_error("Unexpected GPU asset."); }
        void CloneTextureRuntime(Texture&, Texture&) override { throw std::runtime_error("Unexpected GPU asset."); }
        std::shared_ptr<IGraphicsRuntimeObject> CreateModelBuffers(ModelConversionContext&) override
        {
            throw std::runtime_error("Unexpected GPU asset.");
        }
        void ConfigureVisibility(IVisibilityContext&) override {}
    };

    std::vector<byte> ReadFile(const std::filesystem::path& filename)
    {
        std::ifstream input(filename, std::ios::binary);
        Check(static_cast<bool>(input), "Cannot read reference sample file.");
        return {std::istreambuf_iterator<char>(input), {}};
    }

    FUTURE(int) AwaitValue()
    {
        CO_RETURN 42;
    }

    FUTURE(void) AwaitFailure()
    {
        throw std::runtime_error("Expected await test failure.");
        CO_RETURN;
    }

    FUTURE(void) TestAwaitMacros()
    {
        Check((CO_AWAIT AwaitValue()) == 42, "CO_AWAIT lost its return value.");
        CO_RETURN;
    }

    void TestSampleFadeIn()
    {
        auto& settings = AssetSettings::Instance();
        struct RestoreDuration
        {
            float value;
            ~RestoreDuration() { AssetSettings::Instance().soundFadeInMilliseconds = value; }
        } restore{settings.soundFadeInMilliseconds};
        const std::vector<byte> raw{0, 0, 255, 255, 42, 128};
        SoundSample sample;
        const auto load = [&]
        {
            AssetStream stream(raw);
            sample.Read(stream);
        };
        settings.soundFadeInMilliseconds = 0.3f; // Four samples at 11025 Hz.
        load();
        Check(sample.GetPcm() == raw &&
            sample.GetPlaybackPcm() == std::vector<byte>({128, 85, 213, 255, 42, 128}),
            "Fade-in must scale around unsigned PCM silence and leave the remaining bytes unchanged.");
        settings.soundFadeInMilliseconds = 0.f;
        load();
        Check(sample.GetPlaybackPcm() == raw, "Zero fade duration must preserve bit-exact playback.");
        settings.soundFadeInMilliseconds = 1000.f;
        load();
        Check(sample.GetPlaybackPcm().size() == raw.size() && sample.GetPlaybackPcm().front() == 128 &&
            sample.GetPlaybackPcm().back() == raw.back(), "Long fades must be clamped to the sample length.");
        const std::vector<byte> oneByte{255};
        AssetStream oneByteStream(oneByte);
        sample.Read(oneByteStream);
        Check(sample.GetPlaybackPcm() == std::vector<byte>({128}), "A single-byte fade must be safe.");
        for (const float invalid : {-1.f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()})
        {
            settings.soundFadeInMilliseconds = invalid;
            Reject<std::invalid_argument>(load);
        }
        sample.Reset();
        Check(sample.GetPcm().empty() && sample.GetPlaybackPcm().empty(),
            "Reset must release both raw and prepared PCM.");
    }
}

class AudioPlayerVolumeTest
{
public:
    static void Run()
    {
        class ToneSample final : public SoundSample
        {
        public:
            ToneSample()
            {
                std::vector<byte> pcm(SampleRate);
                for (std::size_t i = 0; i < pcm.size(); ++i)
                {
                    pcm[i] = (i / 25) % 2 == 0 ? 192 : 64;
                }
                AssetStream stream(pcm);
                Read(stream);
                m_loaded = true;
            }
        };

        AudioPlayer audio({std::make_shared<ToneSample>()});
        auto* output = audio.m_masteringVoice.get();
        XAUDIO2_VOICE_DETAILS details{};
        output->GetVoiceDetails(&details);
        Microsoft::WRL::ComPtr<IUnknown> meter;
        Check(SUCCEEDED(XAudio2CreateVolumeMeter(meter.GetAddressOf())), "Cannot create output volume meter.");
        XAUDIO2_EFFECT_DESCRIPTOR effect{meter.Get(), TRUE, details.InputChannels};
        XAUDIO2_EFFECT_CHAIN chain{1, &effect};
        Check(SUCCEEDED(output->SetEffectChain(&chain)), "Cannot attach output volume meter.");
        // Keep the test tone quiet; compare relative gains with a fixed mastering level.
        Check(SUCCEEDED(output->SetVolume(0.01f)), "Cannot lower volume test output.");
        std::vector<float> rms(details.InputChannels);
        XAUDIO2FX_VOLUMEMETER_LEVELS levels{nullptr, rms.data(), details.InputChannels};
        float fullVolumeRms = 0.f;
        for (const std::uint8_t volume : {255, 128, 64, 0})
        {
            audio.PlaySample(0, 1, volume);
            const auto start = std::chrono::steady_clock::now();
            for (const int milliseconds : {200, 450, 700})
            {
                std::this_thread::sleep_until(start + std::chrono::milliseconds(milliseconds));
                Check(audio.IsChannelPlaying(0), "Volume test sample ended too early.");
                Check(SUCCEEDED(output->GetEffectParameters(0, &levels, sizeof(levels))),
                    "Cannot read mixed output volume.");
                const float measured = *std::max_element(rms.begin(), rms.end());
                if (volume == 255 && milliseconds == 200)
                {
                    fullVolumeRms = measured;
                    Check(fullVolumeRms > 0.001f, "Volume meter did not receive the full-volume test signal.");
                }
                const float expected = fullVolumeRms * static_cast<float>(volume) / 255.f;
                Check(std::abs(measured - expected) < fullVolumeRms * 0.03f,
                    "Mixed output must retain the requested volume throughout sample playback.");
            }
            const auto deadline = start + std::chrono::seconds(3);
            while (audio.IsChannelPlaying(0) && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            Check(!audio.IsChannelPlaying(0), "Volume test sample did not finish.");
        }
    }
};

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected original SAMP directory.");
        TestRenderer renderer;
        FileManagerBuiltIn files;
        files.Initialize();
        FORCE_WAIT(TestAwaitMacros());
        Check(FORCE_GET(AwaitValue()) == 42, "FORCE_GET lost its return value.");
        Reject<std::runtime_error>([] { FORCE_WAIT(AwaitFailure()); });
        TestSampleFadeIn();

        auto& assets = AssetManager::Instance();
        std::vector<std::shared_ptr<const SoundSample>> samples;
        for (int id = 1; id <= 129; ++id)
        {
            const auto filename = L"SAMP\\SOUND" + std::to_wstring(id) + L".SMP";
            auto sample = FORCE_GET(assets.LoadAsset<SoundSample>(filename));
            Check(sample->IsLoaded() && sample->GetPcm() == ReadFile(
                std::filesystem::path(argv[1]) / ("SOUND" + std::to_string(id) + ".SMP")),
                "A raw sample must retain every original byte, with no inferred header.");
            Check(sample == FORCE_GET(assets.LoadAsset<SoundSample>(filename)),
                "Repeated sample requests must reuse the asset-manager resource.");
            const auto& raw = sample->GetPcm();
            const auto& playback = sample->GetPlaybackPcm();
            const auto fadeSamples = static_cast<std::size_t>(
                std::ceil(AssetSettings::Instance().soundFadeInMilliseconds * SoundSample::SampleRate / 1000.0));
            Check(playback.size() == raw.size() && playback.front() == 128 &&
                std::equal(raw.begin() + fadeSamples, raw.end(), playback.begin() + fadeSamples),
                "Prepared samples must start at silence without altering length or bytes after the fade.");
            if (id == 5 || id == 38)
            {
                for (std::size_t i = 0; i < 6; ++i)
                {
                    Check(std::abs(static_cast<int>(playback[i]) - 128) <= 12,
                        "Water-drop leading transients must be reduced to at most 12 PCM units.");
                }
            }
            samples.push_back(std::move(sample));
        }
        Check(SoundSample::SampleRate == 11025 && SoundSample::Channels == 1 && SoundSample::BitsPerSample == 8,
            "Samples must use fixed 11025 Hz unsigned 8-bit mono PCM.");
        const auto soundTable = FORCE_GET(files.ReadDataFuture(files.FilenameToNative(L"SAMP\\S_TAB.PRB")));
        Check(soundTable == ReadFile(std::filesystem::path(argv[1]) / "S_TAB.PRB"),
            "The built-in sound effect table differs from the original.");
        SoundSample empty;
        const std::vector<byte> noBytes;
        AssetStream emptyStream(noBytes);
        Reject<std::runtime_error>([&] { empty.Read(emptyStream); });
        Reject<std::runtime_error>([&] { FORCE_GET(assets.LoadAsset<SoundSample>(L"SAMP\\MISSING.SMP")); });
        Reject<std::invalid_argument>([] { AudioPlayer invalid({}); });
        Reject<std::invalid_argument>([] { AudioPlayer invalid({nullptr}); });
        AudioPlayerVolumeTest::Run();

        {
            AudioPlayer audio(samples);
            Check(audio.GetSampleCount() == 129, "The backend must expose the complete one-based sample bank.");
            Reject<std::out_of_range>([&] { audio.IsChannelPlaying(4); });
            Reject<std::out_of_range>([&] { audio.PlaySample(0, 0, 0); });
            Reject<std::out_of_range>([&] { audio.PlaySample(0, 130, 0); });
            Reject<std::out_of_range>([&] { audio.PlaySample(4, 1, 0); });
            for (std::size_t channel = 0; channel < Desperabis::SoundChannelCount; ++channel)
            {
                Check(!audio.IsChannelPlaying(channel), "A new channel must be idle.");
                audio.PlaySample(channel, static_cast<std::uint8_t>(channel + 1), 0);
                Check(audio.IsChannelPlaying(channel), "Submitted samples must occupy their assigned channels.");
                Reject<std::logic_error>([&] { audio.PlaySample(channel, 1, 0); });
            }
            audio.StopAllSamples();
            audio.StopAllSamples();
            for (std::size_t channel = 0; channel < Desperabis::SoundChannelCount; ++channel)
            {
                Check(!audio.IsChannelPlaying(channel), "StopAllSamples must stop and flush all channels.");
            }
            audio.PlaySample(0, 1, 0);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (audio.IsChannelPlaying(0) && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            Check(!audio.IsChannelPlaying(0), "Natural completion must release the channel.");
            audio.PlaySample(0, 129, 0);
            audio.StopAllSamples();

            Desperabis::GameContext core;
            core.LoadSoundEffects(soundTable.data(), soundTable.size());
            core.audioHandler = &audio;
            core.soundEffects[0] = {2, {1, 2, 0, 0}};
            core.randomSeed = 123;
            const auto expectedSeed = core.randomSeed * 134775813u + 1u;
            Check(core.PlaySoundEffect(static_cast<Desperabis::SoundEffect>(1), 1, 17, 1) ==
                Desperabis::SoundPlaybackStatus::Started && core.randomSeed == expectedSeed &&
                audio.IsChannelPlaying(3) && core.soundChannelSources[3] == 17,
                "Core selection must consume exactly one RNG draw and pass the chosen sample to the backend.");
            core.StopAllSoundEffects();
            Check(!audio.IsChannelPlaying(3) && core.soundChannelSources[3] == 0,
                "Core stop must flush playback and clear channel-source ownership.");

            // A queued sample must outlive removal from the asset-manager cache.
            audio.PlaySample(0, 1, 0);
            samples.clear();
            assets.Deinitialize();
            Check(audio.IsChannelPlaying(0), "The backend must retain its sample resources until voice destruction.");
        }
        files.Deinitialize();
        std::cout << "Audio tests passed: 129 byte-exact built-in samples, asset caching, four XAudio2 channels, "
            "load-time fade-in and water-drop transient reduction, sustained mixed-output volume, "
            "natural completion, stop/reuse, ownership, core RNG and await/error propagation.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
