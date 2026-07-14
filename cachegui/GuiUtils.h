#pragma once

#include <ctime>
#include <filesystem>
#include <string>

struct HWND__;
using HWND = HWND__*;

namespace cachegui
{
    std::wstring ToWide(const std::string& value);
    std::wstring FormatTime(std::time_t timestamp);
    std::wstring DefaultCachePath();
    std::wstring GetText(HWND window);
    std::filesystem::path ResolveTextureCacheDirectory(
        const std::filesystem::path& suppliedPath);
}
