#include "StringResources.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace Desperabis
{
    namespace
    {
        std::map<std::string, std::string, std::less<>> messages;

        std::string NativeText(std::string_view text)
        {
            std::string result;
            bool highlighted = false;
            for (std::size_t i = 0; i < text.size();)
            {
                if (text.substr(i, 11) == "<highlight>")
                {
                    if (highlighted) { throw std::invalid_argument("Highlight tags must not be nested."); }
                    highlighted = true;
                    result += '\xfb'; i += 11;
                }
                else if (text.substr(i, 12) == "</highlight>")
                {
                    if (!highlighted) { throw std::invalid_argument("Closing highlight tag has no opening tag."); }
                    highlighted = false;
                    result += '\xfa'; i += 12;
                }
                else if (static_cast<unsigned char>(text[i]) < 128)
                {
                    if (text[i] < ' ' && text[i] != '\n')
                    {
                        throw std::invalid_argument("String resources only support newline as a control character.");
                    }
                    result += text[i] == '|' ? '\xb3' : text[i];
                    ++i;
                }
                else
                {
                    // The original font only supplies these seven non-ASCII letters.
                    constexpr std::string_view utf8[]{ "\xc3\xa4", "\xc3\xb6", "\xc3\xbc",
                        "\xc3\x84", "\xc3\x96", "\xc3\x9c", "\xc3\x9f" };
                    constexpr unsigned char native[]{ 0x84, 0x94, 0x81, 0x8e, 0x99, 0x9a, 0xe1 };
                    std::size_t letter = 0;
                    while (letter < std::size(utf8) && text.substr(i, 2) != utf8[letter]) { ++letter; }
                    if (letter == std::size(utf8))
                    {
                        throw std::invalid_argument("String resource contains a character absent from the native font.");
                    }
                    result += static_cast<char>(native[letter]);
                    i += 2;
                }
            }
            if (highlighted) { throw std::invalid_argument("Highlight tag is not closed."); }
            return result;
        }
    }

    void LoadStringResourcesJson(std::string_view json, std::string_view locale)
    {
        std::vector<std::set<std::string>> objectKeys;
        const auto catalog = nlohmann::json::parse(json, [&](int, nlohmann::json::parse_event_t event,
            nlohmann::json& value)
        {
            using Event = nlohmann::json::parse_event_t;
            if (event == Event::object_start) { objectKeys.emplace_back(); }
            else if (event == Event::object_end) { objectKeys.pop_back(); }
            else if (event == Event::key && !objectKeys.back().insert(value.get<std::string>()).second)
            {
                throw std::invalid_argument("Duplicate string resource field: " + value.get<std::string>());
            }
            return true;
        });
        if (!catalog.is_object() || catalog.empty())
        {
            throw std::invalid_argument("String resources require a nonempty object keyed by locale.");
        }
        if (!catalog.contains(locale))
        {
            throw std::out_of_range("Missing string resource locale: " + std::string(locale));
        }
        std::map<std::string, std::string, std::less<>> loaded;
        std::set<std::string> expectedIds;
        for (const auto& language : catalog.items())
        {
            if (language.key().empty() || !language.value().is_object() || language.value().empty())
            {
                throw std::invalid_argument("String resource locale must contain a nonempty message catalog.");
            }
            std::set<std::string> ids;
            for (const auto& entry : language.value().items())
            {
                const auto& value = entry.value();
                if (entry.key().empty() || !value.is_object() || value.size() != 2 ||
                    !value.contains("message") || !value["message"].is_string() ||
                    !value.contains("description") || !value["description"].is_string() ||
                    value["message"].get_ref<const std::string&>().empty() ||
                    value["description"].get_ref<const std::string&>().empty())
                {
                    throw std::invalid_argument("String resource requires message and description: " +
                        language.key() + "/" + entry.key());
                }
                ids.insert(entry.key());
                try
                {
                    auto text = NativeText(value["message"].get_ref<const std::string&>());
                    if (language.key() == locale) { loaded.emplace(entry.key(), std::move(text)); }
                }
                catch (const std::invalid_argument& error)
                {
                    throw std::invalid_argument(language.key() + "/" + entry.key() + ": " + error.what());
                }
            }
            if (expectedIds.empty()) { expectedIds = std::move(ids); }
            else if (ids != expectedIds)
            {
                throw std::invalid_argument("String resource locales must contain identical IDs: " + language.key());
            }
        }
        messages = std::move(loaded);
    }

    void LoadStringResources(const std::filesystem::path& filename, std::string_view locale)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file) { throw std::runtime_error("Cannot open string resources: " + filename.string()); }
        const std::string contents{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        if (file.bad()) { throw std::runtime_error("Cannot read string resources: " + filename.string()); }
        try { LoadStringResourcesJson(contents, locale); }
        catch (const std::exception& error)
        {
            throw std::runtime_error(filename.string() + ": " + error.what());
        }
    }

    const std::string& GetString(std::string_view id)
    {
        if (messages.empty()) { LoadStringResources(std::filesystem::path("Localization") / "strings.json"); }
        const auto entry = messages.find(id);
        if (entry == messages.end()) { throw std::out_of_range("Missing string resource: " + std::string(id)); }
        return entry->second;
    }

    std::string FormatString(std::string_view id,
        std::initializer_list<std::pair<std::string_view, std::string>> arguments)
    {
        const auto& pattern = GetString(id);
        std::string result;
        for (std::size_t i = 0; i < pattern.size();)
        {
            if (pattern[i] != '{') { result += pattern[i++]; continue; }
            const auto end = pattern.find('}', i + 1);
            if (end == std::string::npos) { throw std::invalid_argument("Unclosed placeholder in " + std::string(id)); }
            const auto name = std::string_view(pattern).substr(i + 1, end - i - 1);
            auto argument = arguments.begin();
            while (argument != arguments.end() && argument->first != name) { ++argument; }
            if (argument == arguments.end())
            {
                throw std::invalid_argument("Missing placeholder " + std::string(name) + " in " + std::string(id));
            }
            result += argument->second;
            i = end + 1;
        }
        return result;
    }

    std::string IndexedStringId(std::string_view prefix, int index)
    {
        if (index < 0) { throw std::out_of_range("String resource indices must be nonnegative."); }
        return std::string(prefix) + (index < 10 ? "0" : "") + std::to_string(index);
    }
}
