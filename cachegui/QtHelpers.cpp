#include "QtHelpers.h"

#include "AuroraCacheProbe.h"

#include <system_error>

#include <QByteArray>
#include <QFile>
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

    bool RecognizedCacheDirectoryExists(
        const std::filesystem::path& directory)
    {
        if (TextureEntriesFileExists(directory))
        {
            return true;
        }

        AuroraTextureStoreProbe probe;
        return ProbeAuroraTextureStore(directory, probe) ==
            TextureCacheOpenError::None;
    }

    Qt::CaseSensitivity CachePathCaseSensitivity()
    {
#ifdef _WIN32
        return Qt::CaseInsensitive;
#else
        return Qt::CaseSensitive;
#endif
    }

    QString ComparableCachePath(const std::filesystem::path& path)
    {
        const std::string normalized =
            path.lexically_normal().generic_u8string();
        return QString::fromUtf8(
            normalized.data(),
            static_cast<int>(normalized.size()));
    }

    QString NormalizeCachePath(const QString& path)
    {
        if (path.isEmpty())
        {
            return {};
        }

        return PathToQString(PathFromQString(path).lexically_normal());
    }

    bool ContainsCachePath(
        const QStringList& paths,
        const std::filesystem::path& candidate)
    {
        for (const QString& path : paths)
        {
            if (IsSameCachePath(PathFromQString(path), candidate))
            {
                return true;
            }
        }

        return false;
    }

    std::filesystem::path AuroraDefaultCachePath()
    {
        const QString cacheRoot = QStandardPaths::writableLocation(
#ifdef _WIN32
            QStandardPaths::GenericDataLocation);
#else
            QStandardPaths::GenericCacheLocation);
#endif
        if (cacheRoot.isEmpty())
        {
            return {};
        }

        const std::filesystem::path root = PathFromQString(cacheRoot);

#ifdef _WIN32
        return root / "Aurora" / "AuroraViewer" / "cache";
#elif defined(__APPLE__)
        return root / "org.Aurora.AuroraViewer";
#else
        return root / "auroraviewer";
#endif
    }

    std::filesystem::path AuroraCacheLocationMarkerPath()
    {
        QString configRoot;

#ifdef _WIN32
        configRoot = qEnvironmentVariable("APPDATA");
#elif defined(__APPLE__)
        configRoot = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation);
#else
        configRoot = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation);
#endif

        if (configRoot.isEmpty())
        {
            return {};
        }

        const std::filesystem::path root = PathFromQString(configRoot);

#ifdef _WIN32
        return root / "Aurora" / "AuroraViewer" / "config" /
            "cache_location.txt";
#elif defined(__APPLE__)
        return root / "org.Aurora.AuroraViewer" /
            "cache_location.txt";
#else
        return root / "auroraviewer" / "cache_location.txt";
#endif
    }
}

QString ToQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString PathToQString(const std::filesystem::path& path)
{
    std::filesystem::path preferredPath = path;
    preferredPath.make_preferred();

#ifdef _WIN32
    return QString::fromStdWString(preferredPath.wstring());
#else
    return QString::fromUtf8(preferredPath.u8string().c_str());
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

    AuroraTextureStoreProbe auroraProbe;
    if (ProbeAuroraTextureStore(suppliedPath, auroraProbe) ==
        TextureCacheOpenError::None)
    {
        return auroraProbe.cacheRoot;
    }

    return suppliedPath;
}

std::filesystem::path AuroraCachePathFromLocationMarker(
    const std::filesystem::path& markerPath)
{
    if (markerPath.empty())
    {
        return {};
    }

    QFile markerFile(PathToQString(markerPath));
    if (!markerFile.open(QIODevice::ReadOnly))
    {
        return {};
    }

    constexpr qint64 MaximumMarkerBytes = 32 * 1024;
    const QByteArray markerBytes = markerFile.read(MaximumMarkerBytes + 1);
    if (markerBytes.size() > MaximumMarkerBytes)
    {
        return {};
    }

    const QString markedPath = QString::fromUtf8(markerBytes).trimmed();
    if (markedPath.isEmpty())
    {
        return {};
    }

    AuroraTextureStoreProbe probe;
    if (ProbeAuroraTextureStore(PathFromQString(markedPath), probe) !=
        TextureCacheOpenError::None)
    {
        return {};
    }

    return probe.cacheRoot;
}

bool IsSameCachePath(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
    if (left.empty() || right.empty())
    {
        return false;
    }

    const QString normalizedLeft = ComparableCachePath(left);
    const QString normalizedRight = ComparableCachePath(right);

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
        const std::filesystem::path localAppDataPath =
            PathFromQString(localAppData);
        const std::filesystem::path candidatePaths[] = {
            localAppDataPath / "SecondLife" / "texturecache",
            localAppDataPath / "Firestorm_x64" / "texturecache",
            localAppDataPath / "FirestormOS_x64" / "texturecache"};

        for (const std::filesystem::path& candidatePath : candidatePaths)
        {
            if (RecognizedCacheDirectoryExists(candidatePath))
            {
                return PathToQString(candidatePath);
            }
        }
    }
#elif defined(__APPLE__)
    const QString userCaches =
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);

    if (!userCaches.isEmpty())
    {
        const std::filesystem::path userCachesPath =
            PathFromQString(userCaches);
        const std::filesystem::path candidatePaths[] = {
            userCachesPath / "SecondLife" / "texturecache",
            userCachesPath / "Firestorm" / "texturecache",
            userCachesPath / "Firestorm_x64" / "texturecache",
            userCachesPath / "FirestormOS" / "texturecache",
            userCachesPath / "FirestormOS_x64" / "texturecache"};

        for (const std::filesystem::path& candidatePath : candidatePaths)
        {
            if (RecognizedCacheDirectoryExists(candidatePath))
            {
                return PathToQString(candidatePath);
            }
        }
    }
#endif

    const std::filesystem::path markedAuroraCache =
        AuroraCachePathFromLocationMarker(
            AuroraCacheLocationMarkerPath());
    if (!markedAuroraCache.empty())
    {
        return PathToQString(markedAuroraCache);
    }

    const std::filesystem::path defaultAuroraCache =
        AuroraDefaultCachePath();
    if (!defaultAuroraCache.empty() &&
        RecognizedCacheDirectoryExists(defaultAuroraCache))
    {
        return PathToQString(defaultAuroraCache);
    }

    return {};
}

QString PreferredCachePath()
{
    QSettings settings;
    const QString storedPath =
        settings.value(QStringLiteral("cache/lastOpenedPath")).toString();
    const QString lastOpenedPath = NormalizeCachePath(storedPath);

    if (!lastOpenedPath.isEmpty() && lastOpenedPath != storedPath)
    {
        settings.setValue(
            QStringLiteral("cache/lastOpenedPath"),
            lastOpenedPath);
    }

    if (!lastOpenedPath.isEmpty() &&
        RecognizedCacheDirectoryExists(PathFromQString(lastOpenedPath)))
    {
        return lastOpenedPath;
    }

    return DefaultCachePath();
}

QString CacheFolderDialogStartPath(const QString& currentPath)
{
    if (!currentPath.isEmpty())
    {
        return currentPath;
    }

    const QStandardPaths::StandardLocation startLocation =
#ifdef _WIN32
        QStandardPaths::GenericDataLocation;
#else
        QStandardPaths::GenericCacheLocation;
#endif
    const QString platformStartPath =
        QStandardPaths::writableLocation(startLocation);
    if (!platformStartPath.isEmpty())
    {
        return platformStartPath;
    }

    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

bool DefaultCachePathExists()
{
    return !DefaultCachePath().isEmpty();
}

void RememberOpenedCachePath(const QString& cachePath)
{
    const QString normalizedCachePath = NormalizeCachePath(cachePath);
    if (normalizedCachePath.isEmpty())
    {
        return;
    }

    QSettings settings;
    settings.setValue(
        QStringLiteral("cache/lastOpenedPath"),
        normalizedCachePath);

    QStringList recentPaths;
    recentPaths.append(normalizedCachePath);

    for (const QString& recentPath : RecentCachePaths())
    {
        if (!IsSameCachePath(
                PathFromQString(recentPath),
                PathFromQString(normalizedCachePath)))
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
        const QString normalizedPath = NormalizeCachePath(storedPath);
        const std::filesystem::path filesystemPath =
            PathFromQString(normalizedPath);

        if (!normalizedPath.isEmpty() &&
            RecognizedCacheDirectoryExists(filesystemPath) &&
            !ContainsCachePath(validPaths, filesystemPath))
        {
            validPaths.append(normalizedPath);
        }
    }

    while (validPaths.size() > RecentCacheLimit)
    {
        validPaths.removeLast();
    }

    if (validPaths != storedPaths)
    {
        settings.setValue(QStringLiteral("cache/recentPaths"), validPaths);
    }

    return validPaths;
}

void ClearRecentCachePaths()
{
    QSettings settings;
    settings.remove(QStringLiteral("cache/recentPaths"));
}

QString PreferredPngExportDirectory()
{
    QSettings settings;
    const QString storedDirectory = NormalizeCachePath(
        settings.value(QStringLiteral("export/lastDirectory")).toString());

    if (!storedDirectory.isEmpty())
    {
        std::error_code error;
        if (std::filesystem::is_directory(
                PathFromQString(storedDirectory),
                error) &&
            !error)
        {
            return storedDirectory;
        }
    }

    const QStandardPaths::StandardLocation fallbackLocations[] = {
        QStandardPaths::PicturesLocation,
        QStandardPaths::DocumentsLocation,
        QStandardPaths::HomeLocation};

    for (const QStandardPaths::StandardLocation location : fallbackLocations)
    {
        const QString directory = QStandardPaths::writableLocation(location);
        if (!directory.isEmpty())
        {
            return NormalizeCachePath(directory);
        }
    }

    return {};
}

void RememberPngExportDirectory(const QString& directory)
{
    const QString normalizedDirectory = NormalizeCachePath(directory);
    if (normalizedDirectory.isEmpty())
    {
        return;
    }

    QSettings settings;
    settings.setValue(
        QStringLiteral("export/lastDirectory"),
        normalizedDirectory);
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

QString RefreshedCacheStatus(
    std::size_t validEntryCount,
    std::uint32_t slotCount,
    float cacheVersion,
    std::size_t changedEntryCount)
{
    const QString changeLabel = changedEntryCount == 1
        ? QStringLiteral("1 entry added or updated")
        : QStringLiteral("%1 entries added or updated")
            .arg(changedEntryCount);

    return QStringLiteral(
        "Refreshed %1 valid texture entries from %2 slots. "
        "Cache version %3. %4.")
        .arg(validEntryCount)
        .arg(slotCount)
        .arg(cacheVersion)
        .arg(changeLabel);
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
