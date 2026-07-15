#include "QtHelpers.h"

#include <system_error>

#include <QByteArray>
#include <QSettings>
#include <QStandardPaths>

namespace
{
    constexpr int RecentCacheLimit = 8;

    bool TextureEntriesFileExists(const std::filesystem::path& directory)
    {
        std::error_code error;
        const bool exists =
            std::filesystem::exists(
                directory / "texture.entries",
                error);

        return exists && !error;
    }

    Qt::CaseSensitivity CachePathCaseSensitivity()
    {
#ifdef _WIN32
        return Qt::CaseInsensitive;
#else
        return Qt::CaseSensitive;
#endif
    }
}

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
    if (TextureEntriesFileExists(suppliedPath))
    {
        return suppliedPath;
    }

    const std::filesystem::path childTextureCache =
        suppliedPath / "texturecache";

    if (TextureEntriesFileExists(childTextureCache))
    {
        return childTextureCache;
    }

    return suppliedPath;
}

bool IsSameCachePath(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
    if (left.empty() || right.empty())
    {
        return false;
    }

    const QString normalizedLeft =
        PathToQString(left.lexically_normal());
    const QString normalizedRight =
        PathToQString(right.lexically_normal());

    return normalizedLeft.compare(
        normalizedRight,
        CachePathCaseSensitivity()) == 0;
}

QString DefaultCachePath()
{
#ifdef _WIN32
    const QString localAppData =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    if (!localAppData.isEmpty())
    {
        const QString candidatePaths[] = {
            localAppData + "/SecondLife/texturecache",
            localAppData + "/Firestorm_x64/texturecache",
            localAppData + "/FirestormOS_x64/texturecache"};

        for (const QString& candidatePath : candidatePaths)
        {
            if (TextureEntriesFileExists(PathFromQString(candidatePath)))
            {
                return candidatePath;
            }
        }
    }
#endif

    return {};
}

QString PreferredCachePath()
{
    QSettings settings;
    const QString lastOpenedPath =
        settings.value(QStringLiteral("cache/lastOpenedPath")).toString();

    if (!lastOpenedPath.isEmpty() &&
        TextureEntriesFileExists(PathFromQString(lastOpenedPath)))
    {
        return lastOpenedPath;
    }

    return DefaultCachePath();
}

bool DefaultCachePathExists()
{
    return !DefaultCachePath().isEmpty();
}

void RememberOpenedCachePath(const QString& cachePath)
{
    QSettings settings;
    settings.setValue(QStringLiteral("cache/lastOpenedPath"), cachePath);

    QStringList recentPaths;
    recentPaths.append(cachePath);

    for (const QString& recentPath : RecentCachePaths())
    {
        if (recentPath.compare(cachePath, CachePathCaseSensitivity()) != 0)
        {
            recentPaths.append(recentPath);
        }
    }

    while (recentPaths.size() > RecentCacheLimit)
    {
        recentPaths.removeLast();
    }

    settings.setValue(QStringLiteral("cache/recentPaths"), recentPaths);
}

QStringList RecentCachePaths()
{
    QSettings settings;
    const QStringList storedPaths =
        settings.value(QStringLiteral("cache/recentPaths")).toStringList();
    QStringList validPaths;

    for (const QString& storedPath : storedPaths)
    {
        if (!storedPath.isEmpty() &&
            TextureEntriesFileExists(PathFromQString(storedPath)) &&
            !validPaths.contains(storedPath, CachePathCaseSensitivity()))
        {
            validPaths.append(storedPath);
        }
    }

    return validPaths;
}

void ClearRecentCachePaths()
{
    QSettings settings;
    settings.remove(QStringLiteral("cache/recentPaths"));
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

QString PreviewUnavailablePanelMessage(PreviewDecodeStatus status)
{
    if (status == PreviewDecodeStatus::Incomplete)
    {
        return QStringLiteral("Incomplete cached texture.");
    }

    return QStringLiteral("Preview unavailable.");
}

QString PreviewUnavailableStatus(
    PreviewDecodeStatus status,
    const std::string& detailMessage)
{
    if (status == PreviewDecodeStatus::Incomplete)
    {
        return QStringLiteral(
            "Preview unavailable: cached texture is incomplete or undecodable.");
    }

    return QStringLiteral("Preview unavailable: ")
        + ToQString(detailMessage);
}
