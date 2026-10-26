#pragma once

#include <array>
#include <cstdint>

namespace Desperabis
{
    // Original DS:1B00 and DS:2304; both include the endpoint at angle 512.
    extern const std::array<std::int32_t, 513> SineTable;
    extern const std::array<std::int32_t, 513> ClampedSineTable;

    // Original 1E48:0051 / 1E48:0078 use the 67000-scale, +/-65536-clamped table.
    std::int32_t Sine(std::int16_t angle);
    std::int32_t Cosine(std::int16_t angle);

    std::int16_t WrapSignedWord(std::int32_t value);

    // Original 10AC:384E: whole and fraction by far reference, then one word; RETF 10.
    void AddFixedPoint8(std::int16_t& whole, std::int16_t& fraction, std::int16_t increment);

    // Original 1E48:0A25: one signed word, RETF 2.
    std::int16_t SignWord(std::int16_t value);

    // Original 1E48:0A6C: current, target, step word arguments, RETF 6; returns a delta.
    std::int16_t GetStepToward(std::int16_t current, std::int16_t target, std::int16_t step);
}
