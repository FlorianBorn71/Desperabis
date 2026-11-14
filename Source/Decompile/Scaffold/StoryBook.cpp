#include "StoryBook.h"
#include "GameContext.h"

#include <algorithm>
#include <stdexcept>

namespace Desperabis
{
    void ReadStoryFontAdvances(const std::vector<std::uint8_t>& bytes, std::string_view characters,
        std::array<int, 256>& advances)
    {
        if (bytes.size() < 2 || bytes[0] * bytes[1] < characters.size())
        {
            throw std::invalid_argument("Incomplete native story font.");
        }
        std::size_t offset = 2;
        const auto word = [&]()
        {
            if (offset + 2 > bytes.size()) { throw std::invalid_argument("Truncated story font header."); }
            const int value = bytes[offset] | (bytes[offset + 1] << 8);
            offset += 2;
            return value;
        };
        for (int frame = 0; frame < bytes[0] * bytes[1]; ++frame)
        {
            const int x = word();
            word();
            const int width = word() + 1, height = word() + 1;
            if (width > 1024 || height > 1024 || offset + width * height > bytes.size())
            {
                throw std::invalid_argument("Invalid native story font dimensions.");
            }
            if (frame < static_cast<int>(characters.size()))
            {
                advances[static_cast<std::uint8_t>(characters[frame])] =
                    width + (x < 32768 ? x : x - 65536) + 1;
            }
            offset += width * height;
        }
        if (offset != bytes.size()) { throw std::invalid_argument("Trailing native story font data."); }
        advances[' '] = 8;
    }

    StoryPages LayoutStorySection(std::string_view text, std::int16_t section,
        const std::array<int, 256>& advances)
    {
        if (section <= 0) { throw std::invalid_argument("Story sections must be positive."); }
        const auto marker = "@" + std::to_string(section);
        std::size_t start = 0;
        for (;;)
        {
            start = text.find(marker, start);
            if (start == std::string_view::npos) { throw std::runtime_error("Missing story section " + std::to_string(section) + "."); }
            start += marker.size();
            if (start == text.size() || text[start] < '0' || text[start] > '9') { break; }
        }
        const auto headingEnd = text.find_first_of("\r\n", start);
        if (headingEnd == std::string_view::npos) { throw std::runtime_error("Story section has no body."); }
        start = headingEnd + 1;
        const auto end = text.find('@', start);
        const auto body = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        StoryPages pages(1);
        std::string line;
        std::size_t separator = 0;
        int y = 5;
        const auto width = [&](std::string_view value)
        {
            int result = 0;
            for (const unsigned char c : value)
            {
                if (c == '=') { continue; }
                if (advances[c] <= 0) { throw std::runtime_error("Unavailable story glyph " + std::to_string(c) + "."); }
                result += advances[c];
            }
            return result;
        };
        const auto nextPage = [&]
        {
            if (!pages.back().empty()) { pages.emplace_back(); }
            y = 5;
        };
        const auto flush = [&](bool forced)
        {
            const auto count = forced ? line.size() : separator;
            if (!forced && count == 0) { throw std::runtime_error("Story word has no usable hyphenation point."); }
            auto rendered = line.substr(0, count);
            line.erase(0, count);
            for (std::size_t i = 0; i < rendered.size();)
            {
                if (rendered[i] != '=') { ++i; continue; }
                if (!forced && i + 1 == rendered.size()) { rendered[i++] = '-'; }
                else { rendered.erase(i, 1); }
            }
            y += 16;
            if (!rendered.empty())
            {
                pages.back().emplace_back(UiText{20, static_cast<float>(y), std::move(rendered),
                    1, {}, UiTextAlignment::Left, UiTextLayout::Native});
            }
            if (y > 210) { nextPage(); }
            separator = 0;
        };
        for (std::size_t i = 0; i < body.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(body[i]);
            if ((i == 0 || body[i - 1] == '\n') && body.substr(i, 2) == "//")
            {
                const auto newline = body.find('\n', i);
                if (newline == std::string_view::npos) { break; }
                i = newline;
                continue;
            }
            if (c == '\r') { c = ' '; }
            else if (c < 32 || c == '^' || c == '~' || c == '\\' || c == 252) { continue; }
            if (c == '*')
            {
                if (++i == body.size() || body[i] < '1' || body[i] > '9')
                {
                    throw std::runtime_error("Invalid story chapter annotation.");
                }
                continue;
            }
            if (c == '_') { if (!line.empty()) { flush(true); } nextPage(); continue; }
            if (c == '+') { y += 6; continue; }
            if (c == 254)
            {
                if (++i == body.size() || body[i] < 'A' || body[i] > '^')
                {
                    throw std::runtime_error("Invalid story illustration selector.");
                }
                pages.back().emplace_back(UiBitmap{{static_cast<float>(20 + width(line)),
                    static_cast<float>(y + 20), 0, 0}, UiBitmapSource::TextIllustration,
                    0, 0, static_cast<std::uint8_t>(body[i] - 'A'), {}, UiBitmapLayout::TopLeft});
                continue;
            }
            if (c == '#') { flush(true); continue; }
            line.push_back(static_cast<char>(c));
            if (c == ' ' || c == '=') { separator = line.size(); }
            if (width(line) > 300)
            {
                flush(false);
                for (std::size_t j = 0; j < line.size(); ++j)
                {
                    if (line[j] == ' ' || line[j] == '=') { separator = j + 1; }
                }
            }
        }
        if (!line.empty()) { flush(true); }
        if (pages.back().empty()) { pages.pop_back(); }
        return pages;
    }

    void GameContext::PrepareStoryIntroductions(const std::vector<std::uint8_t>& order, std::uint8_t destination)
    {
        pendingStorySections.clear();
        const auto info = LoadFile("LEVEL\\LEVELS.INF");
        for (const auto number : order)
        {
            if (number == destination) { break; }
            const auto offset = static_cast<std::size_t>(number) * 124;
            if (offset + 124 > info.size()) { throw std::runtime_error("Story introduction references missing level metadata."); }
            if (info[offset + 100] >= 100 && info[offset + 103] != 0)
            {
                pendingStorySections.push_back(info[offset + 103]);
            }
        }
    }

    void GameContext::BeginLevelStory()
    {
        if (level->nativeInfo[103] != 0) { pendingStorySections.push_back(level->nativeInfo[103]); }
        if (pendingStorySections.empty()) { return; }
        std::array<int, 256> advances{};
        for (std::size_t bank = 0; bank < NativeFontCharacters.size(); ++bank)
        {
            ReadStoryFontAdvances(LoadFile("ANIMS\\SCHRIFT" + std::to_string(bank) + ".AN2"),
                NativeFontCharacters[bank], advances);
        }
        const auto bytes = LoadFile("LEVEL\\TEXTE.TXT");
        const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        StoryPages pages;
        for (const auto section : pendingStorySections)
        {
            auto chapter = LayoutStorySection(text, section, advances);
            pages.insert(pages.end(), std::make_move_iterator(chapter.begin()), std::make_move_iterator(chapter.end()));
        }
        pendingStorySections.clear();
        storyPages = std::move(pages);
    }
}
