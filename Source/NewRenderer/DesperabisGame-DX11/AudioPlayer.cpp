#include "pch.h"
#include "AudioPlayer.h"

#include <stdexcept>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    void CheckAudioResult(HRESULT result, const char* operation)
    {
        if (FAILED(result))
        {
            std::ostringstream message;
            message << operation << " failed (HRESULT 0x" << std::hex << static_cast<unsigned long>(result) << ").";
            throw std::runtime_error(message.str());
        }
    }
}

AudioPlayer::ComApartment::ComApartment()
{
    CheckAudioResult(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Audio COM initialization");
}

AudioPlayer::ComApartment::~ComApartment()
{
    CoUninitialize();
}

AudioPlayer::AudioPlayer(std::vector<std::shared_ptr<const SoundSample>> samples)
    : m_samples(std::move(samples))
{
    if (m_samples.empty() || m_samples.size() > 255)
    {
        throw std::invalid_argument("Audio sample bank must contain 1..255 one-based sample IDs.");
    }
    for (const auto& sample : m_samples)
    {
        if (!sample || !sample->IsLoaded() || sample->GetPlaybackPcm().empty())
        {
            throw std::invalid_argument("Audio sample bank contains an unloaded or empty sample.");
        }
    }
    CheckAudioResult(XAudio2Create(m_engine.GetAddressOf()), "XAudio2 initialization");
    IXAudio2MasteringVoice* masteringVoice = nullptr;
    CheckAudioResult(m_engine->CreateMasteringVoice(&masteringVoice), "Audio output creation");
    m_masteringVoice.reset(masteringVoice);
    for (auto& channel : m_channels) { channel = CreateChannel(); }
}

std::unique_ptr<IXAudio2SourceVoice, AudioPlayer::DestroyVoice> AudioPlayer::CreateChannel()
{
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = SoundSample::Channels;
    format.nSamplesPerSec = SoundSample::SampleRate;
    format.wBitsPerSample = SoundSample::BitsPerSample;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    IXAudio2SourceVoice* voice = nullptr;
    CheckAudioResult(m_engine->CreateSourceVoice(&voice, &format), "Audio channel creation");
    return std::unique_ptr<IXAudio2SourceVoice, DestroyVoice>(voice);
}

AudioPlayer::~AudioPlayer() = default;

std::uint16_t AudioPlayer::GetSampleCount() const
{
    return static_cast<std::uint16_t>(m_samples.size());
}

IXAudio2SourceVoice* AudioPlayer::GetChannel(std::size_t channel) const
{
    if (channel >= m_channels.size()) { throw std::out_of_range("Audio channel must be in 0..3."); }
    return m_channels[channel].get();
}

bool AudioPlayer::IsChannelPlaying(std::size_t channel) const
{
    XAUDIO2_VOICE_STATE state{};
    auto* voice = GetChannel(channel);
    if (!voice) { return false; }
    voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return state.BuffersQueued != 0;
}

void AudioPlayer::PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume)
{
    const auto& pcm = GetSamplePcm(sample);
    auto* voice = GetChannel(channel);
    if (IsChannelPlaying(channel)) { throw std::logic_error("Cannot overwrite a playing audio channel."); }
    if (!voice)
    {
        m_channels[channel] = CreateChannel();
        voice = m_channels[channel].get();
    }
    XAUDIO2_BUFFER buffer{};
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(pcm.size());
    buffer.pAudioData = pcm.data();
    CheckAudioResult(voice->SetVolume(static_cast<float>(volume) / 255.f), "Audio volume update");
    CheckAudioResult(voice->SubmitSourceBuffer(&buffer), "Audio sample submission");
    CheckAudioResult(voice->Start(), "Audio playback");
}

void AudioPlayer::StopAllSamples()
{
    // Stop/Flush are asynchronous; destruction guarantees immediate channel reuse with no stale buffer.
    for (auto& voice : m_channels) { voice.reset(); }
}

const std::vector<byte>& AudioPlayer::GetSamplePcm(std::uint8_t sample) const
{
    if (sample == 0 || sample > m_samples.size())
    {
        throw std::out_of_range("Audio sample ID is outside the one-based sample bank.");
    }
    return m_samples[sample - 1]->GetPlaybackPcm();
}

std::size_t AudioPlayer::GetSampleLength(std::uint8_t sample) const
{
    return GetSamplePcm(sample).size();
}

void AudioPlayer::BeginAmbientLoopCreation(std::size_t capacityFrames)
{
    if (capacityFrames == 0 || capacityFrames > XAUDIO2_MAX_BUFFER_BYTES)
    {
        throw std::invalid_argument("Ambient capacity must fit a nonempty XAudio2 buffer.");
    }
    std::vector<byte> pcm(capacityFrames, 128);
    StopAmbientPlayback();
    m_ambientPcm.swap(pcm);
    m_ambientLoopLength = 0;
    m_buildingAmbient = true;
}

void AudioPlayer::ValidateAmbientWrite(std::size_t offset, std::size_t length) const
{
    if (!m_buildingAmbient) { throw std::logic_error("Ambient writes require an active creation pass."); }
    if (offset > m_ambientPcm.size() || length > m_ambientPcm.size() - offset)
    {
        throw std::out_of_range("Ambient sample placement exceeds the creation buffer.");
    }
}

void AudioPlayer::CopyAmbient(std::uint8_t sample, std::size_t offsetFrames)
{
    const auto& pcm = GetSamplePcm(sample);
    ValidateAmbientWrite(offsetFrames, pcm.size());
    std::copy(pcm.begin(), pcm.end(), m_ambientPcm.begin() + offsetFrames);
}

void AudioPlayer::MixAmbient(std::uint8_t sample, std::size_t offsetFrames, std::uint8_t existingWeight)
{
    const auto& pcm = GetSamplePcm(sample);
    ValidateAmbientWrite(offsetFrames, pcm.size());
    for (std::size_t i = 0; i < pcm.size(); ++i)
    {
        auto& destination = m_ambientPcm[offsetFrames + i];
        destination = static_cast<byte>(
            (destination * existingWeight + pcm[i] * (255 - existingWeight)) >> 8);
    }
}

void AudioPlayer::EndAmbientLoopCreation(std::size_t loopLengthFrames)
{
    if (!m_buildingAmbient) { throw std::logic_error("No ambient creation pass is active."); }
    if (loopLengthFrames == 0 || loopLengthFrames > m_ambientPcm.size())
    {
        throw std::out_of_range("Ambient loop length must fit the creation buffer.");
    }
    m_ambientLoopLength = loopLengthFrames;
    m_buildingAmbient = false;
}

void AudioPlayer::StartAmbientPlayback()
{
    if (m_buildingAmbient || m_ambientLoopLength == 0)
    {
        throw std::logic_error("Ambient playback requires a completed creation pass.");
    }
    StopAmbientPlayback();
    auto voice = CreateChannel();
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(m_ambientLoopLength);
    buffer.pAudioData = m_ambientPcm.data();
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    CheckAudioResult(voice->SubmitSourceBuffer(&buffer), "Ambient loop submission");
    CheckAudioResult(voice->Start(), "Ambient playback");
    m_ambientVoice = std::move(voice);
}

void AudioPlayer::StopAmbientPlayback()
{
    m_ambientVoice.reset();
}
