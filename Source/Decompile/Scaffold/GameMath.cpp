#include "GameMath.h"

namespace Desperabis
{
    namespace
    {
        // Verified against the original x87 generation path at all 513 indices.
        constexpr std::array<std::int32_t, 129> SineQuarter = {
            0, 804, 1608, 2412, 3216, 4019, 4821, 5623, 6424, 7224, 8022, 8820,
            9616, 10411, 11204, 11996, 12785, 13573, 14359, 15143, 15924, 16703, 17479, 18253,
            19024, 19792, 20557, 21320, 22078, 22834, 23586, 24335, 25080, 25821, 26558, 27291,
            28020, 28745, 29466, 30182, 30893, 31600, 32303, 33000, 33692, 34380, 35062, 35738,
            36410, 37076, 37736, 38391, 39040, 39683, 40320, 40951, 41576, 42194, 42806, 43412,
            44011, 44604, 45190, 45769, 46341, 46906, 47464, 48015, 48559, 49095, 49624, 50146,
            50660, 51166, 51665, 52156, 52639, 53114, 53581, 54040, 54491, 54934, 55368, 55794,
            56212, 56621, 57022, 57414, 57798, 58172, 58538, 58896, 59244, 59583, 59914, 60235,
            60547, 60851, 61145, 61429, 61705, 61971, 62228, 62476, 62714, 62943, 63162, 63372,
            63572, 63763, 63944, 64115, 64277, 64429, 64571, 64704, 64827, 64940, 65043, 65137,
            65220, 65294, 65358, 65413, 65457, 65492, 65516, 65531, 65536
        };

        constexpr std::array<std::int32_t, 129> ClampedSineQuarter = {
            0, 822, 1644, 2466, 3288, 4108, 4929, 5748, 6567, 7385, 8202, 9017,
            9831, 10643, 11454, 12264, 13071, 13876, 14680, 15481, 16280, 17076, 17870, 18661,
            19449, 20234, 21017, 21796, 22572, 23344, 24113, 24878, 25640, 26397, 27151, 27901,
            28646, 29387, 30124, 30856, 31584, 32306, 33024, 33737, 34445, 35148, 35845, 36537,
            37223, 37904, 38579, 39248, 39912, 40569, 41221, 41866, 42504, 43137, 43763, 44382,
            44994, 45600, 46199, 46791, 47376, 47954, 48525, 49088, 49644, 50192, 50733, 51266,
            51792, 52309, 52819, 53321, 53815, 54301, 54778, 55247, 55708, 56161, 56605, 57041,
            57468, 57886, 58296, 58697, 59089, 59472, 59846, 60211, 60567, 60914, 61252, 61581,
            61900, 62210, 62511, 62802, 63083, 63356, 63618, 63872, 64115, 64349, 64573, 64787,
            64992, 65187, 65372, 65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536,
            65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536
        };

        constexpr std::array<std::int32_t, 513> MakeSineTable(const std::array<std::int32_t, 129>& quarter)
        {
            std::array<std::int32_t, 513> table{};
            for (std::size_t i = 0; i < table.size(); ++i)
            {
                const auto phase = i & 511u;
                const auto folded = phase <= 128 ? phase : phase <= 256 ? 256 - phase :
                    phase <= 384 ? phase - 256 : 512 - phase;
                table[i] = phase >= 256 ? -quarter[folded] : quarter[folded];
            }
            return table;
        }
    }

    const std::array<std::int32_t, 513> SineTable = MakeSineTable(SineQuarter);
    const std::array<std::int32_t, 513> ClampedSineTable = MakeSineTable(ClampedSineQuarter);

    std::int32_t Sine(std::int16_t angle)
    {
        return ClampedSineTable[static_cast<std::uint16_t>(angle) & 511u];
    }

    std::int32_t Cosine(std::int16_t angle)
    {
        return ClampedSineTable[(static_cast<std::uint16_t>(angle) + 128u) & 511u];
    }

    std::int16_t WrapSignedWord(std::int32_t value)
    {
        const auto bits = static_cast<std::uint16_t>(value);
        return static_cast<std::int16_t>(bits >= 0x8000 ? static_cast<std::int32_t>(bits) - 0x10000 : bits);
    }

    void GetDirectionDisplacement(std::int16_t& x, std::int16_t& z,
        std::int16_t angle, std::int16_t speed)
    {
        const auto phase = static_cast<std::uint16_t>(angle) & 511u;
        const auto xProduct = static_cast<std::uint32_t>(static_cast<std::int64_t>(SineTable[phase]) * speed);
        const auto zProduct = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(SineTable[(phase + 128u) & 511u]) * speed);
        x = WrapSignedWord(static_cast<std::int32_t>(xProduct >> 16));
        z = WrapSignedWord(-static_cast<std::int32_t>(zProduct >> 16));
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
