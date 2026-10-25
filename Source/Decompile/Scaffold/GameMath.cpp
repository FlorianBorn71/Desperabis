#include "GameMath.h"

namespace Desperabis
{
    std::int16_t WrapSignedWord(std::int32_t value)
    {
        const auto bits = static_cast<std::uint16_t>(value);
        return static_cast<std::int16_t>(bits >= 0x8000 ? static_cast<std::int32_t>(bits) - 0x10000 : bits);
    }

    void AddFixedPoint8(std::int16_t& whole, std::int16_t& fraction, std::int16_t increment)
    {
        fraction = WrapSignedWord(static_cast<std::int32_t>(fraction) + increment);
        const auto highByte = static_cast<std::uint16_t>(fraction) >> 8;
        const auto carry = highByte >= 128 ? static_cast<std::int32_t>(highByte) - 256 : highByte;
        whole = WrapSignedWord(static_cast<std::int32_t>(whole) + carry);
        fraction = static_cast<std::int16_t>(static_cast<std::uint16_t>(fraction) & 0xff);
    }

    std::int16_t SignWord(std::int16_t value)
    {
        return static_cast<std::int16_t>((value > 0) - (value < 0));
    }

    std::int16_t GetStepToward(std::int16_t current, std::int16_t target, std::int16_t step)
    {
        const auto difference = WrapSignedWord(static_cast<std::int32_t>(target) - current);
        const auto magnitude = WrapSignedWord(difference < 0 ? -static_cast<std::int32_t>(difference) : difference);
        if (magnitude <= step)
        {
            return difference;
        }
        return WrapSignedWord(static_cast<std::int32_t>(SignWord(difference)) * step);
    }
}
