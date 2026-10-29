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

    void TestSampleFades()
    {
        auto& settings = AssetSettings::Instance();
        struct RestoreDuration
        {
            float fadeIn;
            float fadeOut;
            ~RestoreDuration()
            {
                auto& settings = AssetSettings::Instance();
                settings.soundFadeInMilliseconds = fadeIn;
                settings.soundFadeOutMilliseconds = fadeOut;
            }
        } restore{settings.soundFadeInMilliseconds, settings.soundFadeOutMilliseconds};
        const std::vector<byte> raw{0, 0, 255, 255, 42, 255};
        SoundSample sample;
        const auto load = [&]
        {
            AssetStream stream(raw);
            sample.Read(stream);
        };
        settings.soundFadeInMilliseconds = 0.3f; // Four samples at 11025 Hz.
        settings.soundFadeOutMilliseconds = 0.f;
        load();
        Check(sample.GetPcm() == raw &&
            sample.GetPlaybackPcm() == std::vector<byte>({128, 85, 213, 255, 42, 255}),
            "Fade-in must scale around unsigned PCM silence and leave the remaining bytes unchanged.");
        settings.soundFadeInMilliseconds = 0.f;
        load();
        Check(sample.GetPlaybackPcm() == raw, "Zero fade duration must preserve bit-exact playback.");
        settings.soundFadeOutMilliseconds = 0.3f;
        load();
        Check(sample.GetPcm() == raw &&
            sample.GetPlaybackPcm() == std::vector<byte>({0, 0, 255, 213, 99, 128}),
            "Fade-out must preserve the prefix and end exactly at unsigned PCM silence.");
        settings.soundFadeInMilliseconds = 0.3f;
        load();
        Check(sample.GetPlaybackPcm() == std::vector<byte>({128, 85, 213, 213, 99, 128}),
            "Overlapping fades must use the lower gain, applied once to raw PCM.");
        settings.soundFadeOutMilliseconds = 0.f;
        settings.soundFadeInMilliseconds = 1000.f;
        load();
        Check(sample.GetPlaybackPcm().size() == raw.size() && sample.GetPlaybackPcm().front() == 128 &&
            sample.GetPlaybackPcm().back() == raw.back(), "Long fades must be clamped to the sample length.");
        settings.soundFadeOutMilliseconds = 1000.f;
        load();
        Check(sample.GetPlaybackPcm() == std::vector<byte>({128, 102, 179, 179, 111, 128}),
            "Long overlapping fades must be clamped and retain both silent endpoints.");
        settings.soundFadeInMilliseconds = 0.f;
        const std::vector<byte> oneByte{255};
        AssetStream oneByteStream(oneByte);
        sample.Read(oneByteStream);
        Check(sample.GetPlaybackPcm() == std::vector<byte>({128}), "A single-byte fade-out must be safe.");
        settings.soundFadeInMilliseconds = 0.001f;
        settings.soundFadeOutMilliseconds = 0.001f;
        load();
        Check(sample.GetPlaybackPcm() == std::vector<byte>({128, 0, 255, 255, 42, 128}),
            "Single-frame fades must change only the two endpoints.");
        const auto prepared = sample.GetPlaybackPcm();
        for (const float invalid : {-1.f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()})
        {
            settings.soundFadeInMilliseconds = invalid;
            Reject<std::invalid_argument>(load);
            settings.soundFadeInMilliseconds = 0.f;
            settings.soundFadeOutMilliseconds = invalid;
            Reject<std::invalid_argument>(load);
            settings.soundFadeOutMilliseconds = 0.f;
            Check(sample.GetPcm() == raw && sample.GetPlaybackPcm() == prepared,
                "Invalid fade settings must not mutate a previously loaded sample.");
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

class AudioPlayerAmbientTest
{
public:
    static void Run()
    {
        struct RestoreDuration
        {
            float fadeIn = AssetSettings::Instance().soundFadeInMilliseconds;
            float fadeOut = AssetSettings::Instance().soundFadeOutMilliseconds;
            ~RestoreDuration()
            {
                auto& settings = AssetSettings::Instance();
                settings.soundFadeInMilliseconds = fadeIn;
                settings.soundFadeOutMilliseconds = fadeOut;
            }
        } restore;
        AssetSettings::Instance().soundFadeInMilliseconds = 0.f;
        AssetSettings::Instance().soundFadeOutMilliseconds = 0.f;
        class Sample final : public SoundSample
        {
        public:
            explicit Sample(const std::vector<byte>& pcm)
            {
                AssetStream stream(pcm);
                Read(stream);
                m_loaded = true;
            }
        };
        AudioPlayer audio({
            std::make_shared<Sample>(std::vector<byte>{0, 64, 128, 255}),
            std::make_shared<Sample>(std::vector<byte>{255, 128, 64, 0}),
            std::make_shared<Sample>(std::vector<byte>(4, 128)),
            std::make_shared<Sample>(std::vector<byte>(SoundSample::SampleRate, 128))
        });
        Check(SUCCEEDED(audio.m_masteringVoice->SetVolume(0.f)), "Cannot mute ambient test output.");
        Check(audio.GetSampleLength(1) == 4, "Ambient lengths must count PCM frames.");
        Reject<std::out_of_range>([&] { audio.GetSampleLength(0); });
        Reject<std::out_of_range>([&] { audio.GetSampleLength(5); });
        Reject<std::logic_error>([&] { audio.StartAmbientPlayback(); });
        Reject<std::logic_error>([&] { audio.EndAmbientLoopCreation(1); });
        Reject<std::logic_error>([&] { audio.CopyAmbient(1, 0); });
        Reject<std::logic_error>([&] { audio.MixAmbient(1, 0, 100); });
        Reject<std::invalid_argument>([&] { audio.BeginAmbientLoopCreation(0); });
        Reject<std::invalid_argument>([&]
        {
            audio.BeginAmbientLoopCreation(static_cast<std::size_t>(XAUDIO2_MAX_BUFFER_BYTES) + 1);
        });

        audio.BeginAmbientLoopCreation(2048);
        Check(std::all_of(audio.m_ambientPcm.begin(), audio.m_ambientPcm.end(),
            [](byte value) { return value == 128; }), "Ambient gaps must start as unsigned PCM silence.");
        Reject<std::logic_error>([&] { audio.StartAmbientPlayback(); });
        audio.CopyAmbient(1, 8);
        Check(std::equal(audio.m_ambientPcm.begin() + 8, audio.m_ambientPcm.begin() + 12,
            std::vector<byte>{0, 64, 128, 255}.begin()), "Ambient base copy must be byte-exact.");
        audio.MixAmbient(2, 8, 100);
        const std::vector<byte> expected{154, 102, 88, 99};
        Check(std::equal(expected.begin(), expected.end(), audio.m_ambientPcm.begin() + 8),
            "Ambient must use unsigned weighted blending with denominator 256.");
        audio.MixAmbient(2, 8, 200);
        const std::vector<byte> source{255, 128, 64, 0};
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            Check(audio.m_ambientPcm[8 + i] == (expected[i] * 200 + source[i] * 55) >> 8,
                "Overlapping ambient placements must mix into the previous output.");
        }
        audio.MixAmbient(3, 0, 100);
        Check(audio.m_ambientPcm[0] == 127, "Ambient blending must preserve the native silence bias.");
        const auto snapshot = audio.m_ambientPcm;
        Reject<std::out_of_range>([&] { audio.CopyAmbient(1, 2047); });
        Reject<std::out_of_range>([&]
        {
            audio.MixAmbient(1, (std::numeric_limits<std::size_t>::max)(), 100);
        });
        Reject<std::out_of_range>([&] { audio.CopyAmbient(0, 0); });
        Reject<std::out_of_range>([&] { audio.EndAmbientLoopCreation(0); });
        Reject<std::out_of_range>([&] { audio.EndAmbientLoopCreation(2049); });
        Check(audio.m_ambientPcm == snapshot, "Rejected ambient writes must not mutate PCM.");
        audio.EndAmbientLoopCreation(1024);
        Check(audio.m_ambientLoopLength == 1024 && audio.m_ambientPcm.size() == 2048,
            "Ambient logical length must be independent of allocation capacity.");
        Reject<std::logic_error>([&] { audio.MixAmbient(1, 0, 100); });
        Reject<std::logic_error>([&] { audio.EndAmbientLoopCreation(1024); });
        audio.StartAmbientPlayback();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        XAUDIO2_VOICE_STATE state{};
        audio.m_ambientVoice->GetState(&state);
        Check(state.BuffersQueued == 1 && state.SamplesPlayed > 1024,
            "Ambient must keep looping beyond its first logical-length buffer.");
        audio.StopAllSamples();
        Check(audio.m_ambientVoice != nullptr, "Effect cleanup must not consume the dedicated ambient voice.");
        audio.PlaySample(0, 4, 0);
        audio.StopAmbientPlayback();
        audio.StopAmbientPlayback();
        Check(audio.IsChannelPlaying(0), "Ambient stop must not stop live effects.");
        audio.StartAmbientPlayback();
        audio.BeginAmbientLoopCreation(4096);
        Check(!audio.m_ambientVoice && audio.IsChannelPlaying(0) && audio.m_ambientLoopLength == 0,
            "Rebuilding must synchronously release old ambient playback without touching effects.");
        audio.CopyAmbient(1, 0);
        audio.EndAmbientLoopCreation(2048);
        audio.StartAmbientPlayback();
    }

    static void RunRecipes(AudioPlayer& audio)
    {
        Check(SUCCEEDED(audio.m_masteringVoice->SetVolume(0.f)), "Cannot mute ambient recipe test output.");
        Desperabis::GameContext core;
        core.audioHandler = &audio;
        for (std::uint8_t environment = 0; environment < 5; ++environment)
        {
            core.CreateAmbientLoop(environment);
            Check(audio.m_ambientPcm.size() == 1800000 && audio.m_ambientLoopLength > 1400000 &&
                !audio.m_buildingAmbient, "Original recipe must produce a finalized ambient buffer.");
            core.audioHandler->StartAmbientPlayback();
            XAUDIO2_VOICE_STATE state{};
            audio.m_ambientVoice->GetState(&state);
            Check(state.BuffersQueued == 1, "Original recipe must reach the dedicated looping voice.");
        }
        audio.StopAmbientPlayback();
        Check(SUCCEEDED(audio.m_masteringVoice->SetVolume(1.f)), "Cannot restore audio output volume.");
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
        TestSampleFades();
        AudioPlayerAmbientTest::Run();

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
            const auto fadeInSamples = static_cast<std::size_t>(std::min<double>(static_cast<double>(raw.size()),
                std::ceil(AssetSettings::Instance().soundFadeInMilliseconds * SoundSample::SampleRate / 1000.0)));
            const auto fadeOutSamples = static_cast<std::size_t>(std::min<double>(static_cast<double>(raw.size()),
                std::ceil(AssetSettings::Instance().soundFadeOutMilliseconds * SoundSample::SampleRate / 1000.0)));
            Check(playback.size() == raw.size() && playback.front() == 128 && playback.back() == 128,
                "Prepared samples must retain their length and start/end at silence.");
            if (fadeInSamples + fadeOutSamples <= raw.size())
            {
                Check(std::equal(raw.begin() + fadeInSamples, raw.end() - fadeOutSamples,
                    playback.begin() + fadeInSamples), "Samples must remain byte-exact outside both fades.");
            }
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
            AudioPlayerAmbientTest::RunRecipes(audio);

            // A queued sample must outlive removal from the asset-manager cache.
            audio.PlaySample(0, 1, 0);
            samples.clear();
            assets.Deinitialize();
            Check(audio.IsChannelPlaying(0), "The backend must retain its sample resources until voice destruction.");
        }
        files.Deinitialize();
        std::cout << "Audio tests passed: 129 byte-exact built-in samples, asset caching, four XAudio2 channels, "
            "load-time fade-in/out and water-drop transient reduction, sustained mixed-output volume, "
            "ambient PCM blending/looping/rebuild and original recipes, "
            "natural completion, stop/reuse, ownership, core RNG and await/error propagation.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
