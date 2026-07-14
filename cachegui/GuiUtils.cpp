#include "GuiUtils.h"

#include <iomanip>
#include <sstream>

#define NOMINMAX
#define UUID WindowsSdkUUID
#include <windows.h>
#include <shlobj.h>
#undef UUID

namespace fs = std::filesystem;

namespace cachegui
{
    std::wstring ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int length = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

        std::wstring converted(static_cast<std::size_t>(length), L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            converted.data(),
            length);

        return converted;
    }

    std::wstring FormatTime(std::time_t timestamp)
    {
        std::tm localTime{};

        if (localtime_s(&localTime, &timestamp) != 0)
        {
            return {};
        }

        std::wostringstream stream;
        stream << std::put_time(&localTime, L"%Y-%m-%d %H:%M:%S");
        return stream.str();
    }

    std::wstring DefaultCachePath()
    {
        wchar_t* localAppData = nullptr;

        if (SHGetKnownFolderPath(
                FOLDERID_LocalAppData,
                0,
                nullptr,
                &localAppData) != S_OK)
        {
            return {};
        }

        fs::path path(localAppData);
        CoTaskMemFree(localAppData);

        path /= L"FirestormOS_x64";
        path /= L"texturecache";
        return path.wstring();
    }

    std::wstring GetText(HWND window)
    {
        const int length = GetWindowTextLengthW(window);
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');

        if (length > 0)
        {
            GetWindowTextW(window, text.data(), length + 1);
        }

        text.resize(static_cast<std::size_t>(length));
        return text;
    }

    fs::path ResolveTextureCacheDirectory(const fs::path& suppliedPath)
    {
        if (fs::exists(suppliedPath / L"texture.entries"))
        {
            return suppliedPath;
        }

        const fs::path childTextureCache = suppliedPath / L"texturecache";

        if (fs::exists(childTextureCache / L"texture.entries"))
        {
            return childTextureCache;
        }

        return suppliedPath;
    }
}
