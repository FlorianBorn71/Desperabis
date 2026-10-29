#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace Desperabis
{
    enum class SoundEffect : std::uint16_t
    {
        Drop = 5,
        DoorClosed = 8,
        RatAndSnake = 17,
        Burning = 62,
        Flies = 74
    };

    struct SoundEffectDefinition
    {
        std::uint8_t sampleCount = 0;
        std::array<std::uint8_t, 4> samples{};
    };

    static_assert(sizeof(SoundEffectDefinition) == 5);
    inline constexpr std::size_t SoundEffectCount = 129;
    inline constexpr std::size_t SoundChannelCount = 4;
    inline constexpr std::size_t SoundEffectTableSize = 4 + SoundEffectCount * 5;

    class AudioHandler
    {
    public:
        virtual ~AudioHandler() = default;
        virtual std::uint16_t GetSampleCount() const = 0;
        // Host channels are zero-based; sample references retain their original one-based IDs.
        virtual bool IsChannelPlaying(std::size_t channel) const = 0;
        virtual void PlaySample(std::size_t channel, std::uint8_t sample, std::uint8_t volume) = 0;
        virtual void StopAllSamples() = 0;
        // Frames are mono PCM samples, not sound-asset counts. Legacy effect-only hosts reject these calls.
        virtual std::size_t GetSampleLength(std::uint8_t) const { throw std::logic_error("Ambient audio is unsupported."); }
        virtual void BeginAmbientLoopCreation(std::size_t) { throw std::logic_error("Ambient audio is unsupported."); }
        virtual void CopyAmbient(std::uint8_t, std::size_t) { throw std::logic_error("Ambient audio is unsupported."); }
        virtual void MixAmbient(std::uint8_t, std::size_t, std::uint8_t) { throw std::logic_error("Ambient audio is unsupported."); }
        virtual void EndAmbientLoopCreation(std::size_t) { throw std::logic_error("Ambient audio is unsupported."); }
        virtual void StartAmbientPlayback() { throw std::logic_error("Ambient audio is unsupported."); }
        virtual void StopAmbientPlayback() { throw std::logic_error("Ambient audio is unsupported."); }
    };

    enum class SoundPlaybackStatus
    {
        Started,
        AudioUnavailable,
        Disabled,
        NoSample,
        Inaudible,
        NoFreeChannel,
        ReservedChannel,
        SourceLimitReached
    };
}
