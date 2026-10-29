#pragma once

#include <Asset/AssetBase.h>
#include <Asset/AssetSettings.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

class D_IMPEXP SoundSample : public AssetBase
{
public:
    static constexpr unsigned int SampleRate = 11025;
    static constexpr unsigned int BitsPerSample = 8;
    static constexpr unsigned int Channels = 1;

    void Read(AssetStream& stream) override
    {
        const auto fadeInMilliseconds = AssetSettings::Instance().soundFadeInMilliseconds;
        const auto fadeOutMilliseconds = AssetSettings::Instance().soundFadeOutMilliseconds;
        if (!std::isfinite(fadeInMilliseconds) || fadeInMilliseconds < 0.f)
        {
            throw std::invalid_argument("Sound fade-in duration must be finite and nonnegative.");
        }
        if (!std::isfinite(fadeOutMilliseconds) || fadeOutMilliseconds < 0.f)
        {
            throw std::invalid_argument("Sound fade-out duration must be finite and nonnegative.");
        }
        const auto size = stream.GetRemainingSize();
        if (size == 0 || size > static_cast<size_t>((std::numeric_limits<int>::max)()))
        {
            throw std::runtime_error("Sound sample must contain nonempty raw 8-bit mono PCM.");
        }
        std::vector<byte> pcm(size);
        if (stream.Read(reinterpret_cast<char*>(pcm.data()), static_cast<int>(size)) != static_cast<int>(size))
        {
            throw std::runtime_error("Sound sample PCM read was incomplete.");
        }
        auto playbackPcm = pcm;
        const auto fadeSamples = [size](float milliseconds)
        {
            return static_cast<size_t>(std::min<double>(static_cast<double>(size),
                std::ceil(static_cast<double>(milliseconds) * SampleRate / 1000.0)));
        };
        const auto fadeInSamples = fadeSamples(fadeInMilliseconds);
        const auto fadeOutSamples = fadeSamples(fadeOutMilliseconds);
        for (size_t i = 0; i < size; ++i)
        {
            double gain = 1.0;
            if (i < fadeInSamples)
            {
                gain = static_cast<double>(i) / std::max<size_t>(fadeInSamples - 1, 1);
            }
            const auto remaining = size - 1 - i;
            if (remaining < fadeOutSamples)
            {
                gain = std::min<double>(gain,
                    static_cast<double>(remaining) / std::max<size_t>(fadeOutSamples - 1, 1));
            }
            if (gain < 1.0)
            {
                playbackPcm[i] = static_cast<byte>(128 + std::lround((static_cast<int>(pcm[i]) - 128) * gain));
            }
        }
        m_pcm = std::move(pcm);
        m_playbackPcm = std::move(playbackPcm);
    }

    void Reset() override { m_pcm.clear(); m_playbackPcm.clear(); m_loaded = false; }
    const std::vector<byte>& GetPcm() const { return m_pcm; }
    const std::vector<byte>& GetPlaybackPcm() const { return m_playbackPcm; }

private:
    std::vector<byte> m_pcm;
    std::vector<byte> m_playbackPcm;
};
