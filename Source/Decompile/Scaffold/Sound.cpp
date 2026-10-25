#include "BehaviorDispatch.h"
#include "GameMath.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Desperabis
{
    namespace
    {
        std::int16_t AbsoluteWord(std::int16_t value)
        {
            return value < 0 ? WrapSignedWord(-static_cast<std::int32_t>(value)) : value;
        }
    }

    void GameState::LoadSoundEffects(const std::uint8_t* data, std::size_t size)
    {
        if (!data || size != SoundEffectTableSize)
        {
            throw std::invalid_argument("S_TAB.PRB must contain its four-byte header and 129 five-byte records.");
        }
        if (data[0] != SoundEffectCount || data[1] != 0 || data[2] != 5 || data[3] != 0)
        {
            throw std::invalid_argument("Unsupported sound-effect table dimensions.");
        }
        for (std::size_t i = 0; i < SoundEffectCount; ++i)
        {
            if (data[4 + i * 5] > 4)
            {
                throw std::invalid_argument("Sound effects support at most four sample references.");
            }
        }
        std::memcpy(soundEffects.data(), data + 4, SoundEffectCount * sizeof(SoundEffectDefinition));
        soundEffectsLoaded = true;
    }

    std::uint8_t GameState::GetSoundVolumeAt(
        std::int16_t x, std::int16_t z, std::int16_t distanceScale) const
    {
        const auto distance = positionalSoundsSuppressed ? static_cast<std::int16_t>(10000) :
            WrapSignedWord(AbsoluteWord(WrapSignedWord(soundListenerX - x)) +
                AbsoluteWord(WrapSignedWord(soundListenerZ - z)));
        const auto scaled = WrapSignedWord(static_cast<std::int32_t>(distance) * distanceScale);
        const auto magnitude = static_cast<std::uint16_t>(AbsoluteWord(scaled));
        return static_cast<std::uint8_t>(std::clamp(355 - static_cast<int>(magnitude >> 1), 0, 255));
    }

    SoundPlaybackStatus GameState::PlaySoundEffect(
        SoundEffect effect, std::uint8_t volume, std::int16_t sourceId, std::int16_t instanceLimit)
    {
        const auto effectId = static_cast<std::uint16_t>(effect);
        if (effectId < 1 || effectId > SoundEffectCount)
        {
            throw std::out_of_range("Sound-effect ID must be in the native range 1..129.");
        }
        if (!soundEffectsLoaded)
        {
            if (!audioHandler) { return SoundPlaybackStatus::AudioUnavailable; }
            throw std::runtime_error("PlaySoundEffect requires the S_TAB.PRB effect table.");
        }
        const auto& definition = soundEffects[effectId - 1];
        if (definition.sampleCount == 0) { return SoundPlaybackStatus::NoSample; }
        if (definition.sampleCount > definition.samples.size())
        {
            throw std::invalid_argument("Sound-effect sample count exceeds its four-reference record.");
        }
        // 2CE4:170A uses the high seed word modulo the range, not multiply-high scaling.
        randomSeed = randomSeed * 0x08088405u + 1u;
        const auto sample = definition.samples[(randomSeed >> 16) % definition.sampleCount];
        if (!audioHandler) { return SoundPlaybackStatus::AudioUnavailable; }
        if (sample == 0 || sample > audioHandler->GetSampleCount()) { return SoundPlaybackStatus::NoSample; }
        if (volume == 0) { return SoundPlaybackStatus::Inaudible; }
        if (!soundEnabled) { return SoundPlaybackStatus::Disabled; }

        std::size_t freeChannel = SoundChannelCount;
        int freeCount = 0;
        int sameSourceCount = 0;
        for (std::size_t channel = 0; channel < SoundChannelCount; ++channel)
        {
            if (!audioHandler->IsChannelPlaying(channel))
            {
                freeChannel = channel;
                ++freeCount;
            }
            else if (soundChannelSources[channel] == sourceId)
            {
                ++sameSourceCount;
            }
        }
        if (instanceLimit < 0 && freeCount < 2) { return SoundPlaybackStatus::ReservedChannel; }
        if (freeCount == 0) { return SoundPlaybackStatus::NoFreeChannel; }
        if (AbsoluteWord(instanceLimit) <= sameSourceCount) { return SoundPlaybackStatus::SourceLimitReached; }
        audioHandler->PlaySample(freeChannel, sample, volume);
        soundChannelSources[freeChannel] = sourceId;
        return SoundPlaybackStatus::Started;
    }

    SoundPlaybackStatus GameState::PlaySoundEffect(
        SoundEffect effect, std::uint8_t volume, const RawGameObject& source, std::int16_t instanceLimit)
    {
        for (std::size_t slot = 0; slot < worldObjects.size(); ++slot)
        {
            if (&source == &worldObjects[slot])
            {
                return PlaySoundEffect(effect, volume, static_cast<std::int16_t>(slot), instanceLimit);
            }
        }
        if (!audioHandler) { return PlaySoundEffect(effect, volume, 0, instanceLimit); }
        throw std::invalid_argument("Object sound sources must belong to GameState::worldObjects.");
    }
}
