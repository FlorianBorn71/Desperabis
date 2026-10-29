#pragma once

#include <Sound.h>
#include <Asset/SoundSample.h>
#include <wrl/client.h>
#include <xaudio2.h>

class AudioPlayer final : public Desperabis::AudioHandler
{
public:
    explicit AudioPlayer(std::vector<std::shared_ptr<const SoundSample>> samples);
    ~AudioPlayer() override;
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    std::uint16_t GetSampleCount() const override;
    bool IsChannelPlaying(std::size_t channel) const override;
    void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) override;
    void StopAllSamples() override;
    std::size_t GetSampleLength(std::uint8_t sample) const override;
    void BeginAmbientLoopCreation(std::size_t capacityFrames) override;
    void CopyAmbient(std::uint8_t sample, std::size_t offsetFrames) override;
    void MixAmbient(std::uint8_t sample, std::size_t offsetFrames, std::uint8_t existingWeight) override;
    void EndAmbientLoopCreation(std::size_t loopLengthFrames) override;
    void StartAmbientPlayback() override;
    void StopAmbientPlayback() override;

private:
    friend class AudioPlayerVolumeTest;
    friend class AudioPlayerAmbientTest;

    struct ComApartment
    {
        ComApartment();
        ~ComApartment();
        ComApartment(const ComApartment&) = delete;
        ComApartment& operator=(const ComApartment&) = delete;
    };

    struct DestroyVoice
    {
        void operator()(IXAudio2Voice* voice) const { voice->DestroyVoice(); }
    };

    std::unique_ptr<IXAudio2SourceVoice, DestroyVoice> CreateChannel();
    IXAudio2SourceVoice* GetChannel(std::size_t channel) const;
    const std::vector<byte>& GetSamplePcm(std::uint8_t sample) const;
    void ValidateAmbientWrite(std::size_t offset, std::size_t length) const;

    ComApartment m_com;
    std::vector<std::shared_ptr<const SoundSample>> m_samples;
    Microsoft::WRL::ComPtr<IXAudio2> m_engine;
    std::unique_ptr<IXAudio2MasteringVoice, DestroyVoice> m_masteringVoice;
    std::vector<byte> m_ambientPcm;
    std::size_t m_ambientLoopLength = 0;
    bool m_buildingAmbient = false;
    std::unique_ptr<IXAudio2SourceVoice, DestroyVoice> m_ambientVoice;
    std::array<std::unique_ptr<IXAudio2SourceVoice, DestroyVoice>, Desperabis::SoundChannelCount> m_channels;
};
