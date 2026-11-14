#pragma once

#include "ApplicationFlow.h"
#include <array>
#include <string_view>

namespace Desperabis
{
    inline constexpr std::array<std::string_view, 3> NativeFontCharacters{
        "abcdefghijklmnopqrstuvwxyz\x94\x81\x84\xe1",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\x99\x9a\x8e?",
        "0123456789.:>/-\xb3~\"!$<,+\xef^#*%" };

    using StoryPages = std::vector<std::vector<UiDrawCommand>>;
    void ReadStoryFontAdvances(const std::vector<std::uint8_t>& bytes, std::string_view characters,
        std::array<int, 256>& advances);
    StoryPages LayoutStorySection(std::string_view text, std::int16_t section,
        const std::array<int, 256>& advances);
}
