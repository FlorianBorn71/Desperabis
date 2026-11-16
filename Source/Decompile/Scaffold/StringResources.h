#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace Desperabis
{
    // UTF-8 catalogs are converted to the original font's byte encoding at load time.
    // Loading is transactional; malformed resources never replace the active catalog.
    void LoadStringResources(const std::filesystem::path& filename, std::string_view locale = "en-us");
    void LoadStringResourcesJson(std::string_view json, std::string_view locale = "en-us");
    const std::string& GetString(std::string_view id);
    std::string FormatString(std::string_view id,
        std::initializer_list<std::pair<std::string_view, std::string>> arguments);
    std::string IndexedStringId(std::string_view prefix, int index);
}
