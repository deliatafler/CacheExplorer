#include "QtHelpers.h"

#include <QByteArray>
#include <QStandardPaths>

QString ToQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString PathToQString(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromUtf8(path.u8string().c_str());
#endif
}

std::filesystem::path PathFromQString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::filesystem::u8path(utf8.constData());
}

std::filesystem::path ResolveTextureCacheDirectory(
    const std::filesystem::path& suppliedPath)
{
    if (std::filesystem::exists(suppliedPath / "texture.entries"))
    {
        return suppliedPath;
    }

    const std::filesystem::path childTextureCache =
        suppliedPath / "texturecache";

    if (std::filesystem::exists(childTextureCache / "texture.entries"))
    {
        return childTextureCache;
    }

    return suppliedPath;
}

QString DefaultCachePath()
{
#ifdef _WIN32
    const QString localAppData =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    if (!localAppData.isEmpty())
    {
        return localAppData + "/FirestormOS_x64/texturecache";
    }
#endif

    return {};
}

const char* CacheErrorMessage(CacheError error)
{
    switch (error)
    {
        case CacheError::None:
            return "No error.";

        case CacheError::FileNotFound:
            return "texture.entries could not be found.";

        case CacheError::ReadError:
            return "The cache entry table could not be read.";

        case CacheError::InvalidHeader:
            return "The texture.entries header is invalid.";

        case CacheError::UnsupportedVersion:
            return "The cache version is unsupported.";
    }

    return "Unknown cache error.";
}

QString LoadedCacheStatus(
    std::size_t validEntryCount,
    std::uint32_t slotCount,
    float cacheVersion)
{
    return QStringLiteral("Loaded %1 valid texture entries from %2 slots. Cache version %3.")
        .arg(validEntryCount)
        .arg(slotCount)
        .arg(cacheVersion);
}

QString PreviewReadyStatus(
    const std::string& uuidText,
    std::uint32_t width,
    std::uint32_t height)
{
    return QStringLiteral("Preview ready: %1 (%2 x %3)")
        .arg(ToQString(uuidText))
        .arg(width)
        .arg(height);
}
