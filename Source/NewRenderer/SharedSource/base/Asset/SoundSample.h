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
        const auto fadeMilliseconds = AssetSettings::Instance().soundFadeInMilliseconds;
        if (!std::isfinite(fadeMilliseconds) || fadeMilliseconds < 0.f)
        {
            throw std::invalid_argument("Sound fade-in duration must be finite and nonnegative.");
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
        const auto fadeSamples = static_cast<size_t>(std::min<double>(static_cast<double>(size),
            std::ceil(static_cast<double>(fadeMilliseconds) * SampleRate / 1000.0)));
        for (size_t i = 0; i < fadeSamples; ++i)
        {
            const double gain = static_cast<double>(i) / std::max<size_t>(fadeSamples - 1, 1);
            playbackPcm[i] = static_cast<byte>(128 + std::lround((static_cast<int>(pcm[i]) - 128) * gain));
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
