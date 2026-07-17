#include "CacheEntryTableModel.h"
#include "GalleryFilterProxyModel.h"
#include "GalleryPreviewQueue.h"
#include "PreviewCache.h"
#include "QtGalleryStatus.h"
#include "QtHelpers.h"
#include "QtTextureDetails.h"
#include "TryNextPreviewState.h"

#include <QStandardItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

#include <iostream>
#include <string>

namespace
{
    int gFailures = 0;

    void Expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++gFailures;
        }
    }

    CacheEntry MakeEntry(std::uint32_t cacheIndex)
    {
        CacheEntry entry{};
        entry.cacheIndex = cacheIndex;
        return entry;
    }

    void TestGalleryPreviewQueue()
    {
        GalleryPreviewQueue queue;
        queue.Replace({MakeEntry(4), MakeEntry(7)});

        Expect(queue.HasEntries(), "queue has replaced entries");
        Expect(queue.Total() == 2, "queue records total entries");
        Expect(queue.Completed() == 0, "queue starts with no completed entries");
        Expect(queue.TakeNext().cacheIndex == 4, "queue preserves entry order");

        queue.MarkCompleted();
        Expect(queue.Completed() == 1, "queue increments completed entries");
        Expect(queue.TakeNext().cacheIndex == 7, "queue returns second entry");
        Expect(!queue.HasEntries(), "queue becomes empty after taking entries");

        queue.MarkCompleted();
        queue.MarkCompleted();
        Expect(queue.Completed() == 2, "completed count does not exceed total");

        queue.RequestRefresh();
        Expect(queue.RefreshPending(), "queue records a refresh request");
        Expect(queue.ConsumeRefreshRequest(), "queue consumes a refresh request");
        Expect(!queue.ConsumeRefreshRequest(), "queue consumes refresh only once");

        queue.Clear();
        Expect(queue.Total() == 0, "clear resets total entries");
        Expect(queue.Completed() == 0, "clear resets completed entries");
        Expect(!queue.RefreshPending(), "clear removes pending refresh");
    }

    void TestTryNextPreviewState()
    {
        TryNextPreviewState state;
        state.Start(12);

        Expect(state.IsActive(), "try-next starts active");
        Expect(state.TakeNextProxyRow() == 12, "try-next starts at selected row");
        Expect(state.TakeNextProxyRow() == 13, "try-next advances proxy rows");
        Expect(state.Attempts() == 2, "try-next counts attempts");
        Expect(!state.IsExhausted(20), "try-next remains active before limit");

        state.Stop();
        Expect(!state.IsActive(), "try-next stops explicitly");

        state.Start(0);
        for (int attempt = 0; attempt < 200; ++attempt)
        {
            state.TakeNextProxyRow();
        }

        Expect(
            state.IsExhausted(1000),
            "try-next stops after its bounded attempt count");
    }

    void TestGalleryStatusText()
    {
        Expect(
            GalleryEntryCountText(0, 0) == QStringLiteral("0 entries"),
            "empty gallery count is clear");
        Expect(
            GalleryEntryCountText(12, 12) == QStringLiteral("12 entries"),
            "unfiltered gallery count omits duplicate total");
        Expect(
            GalleryEntryCountText(12, 48) == QStringLiteral("12 of 48 entries"),
            "filtered gallery count includes total");
        Expect(
            GalleryFilterUpdatedStatus(0, 48) ==
                QStringLiteral("Gallery filter updated: no matching entries."),
            "empty filter result is clear");
        Expect(
            GalleryFilterUpdatedStatus(12, 48) ==
                QStringLiteral("Gallery filter updated: showing 12 of 48 entries."),
            "filter result uses gallery count text");
    }

    void TestInitialGalleryFilter()
    {
        QStandardItemModel sourceModel(5, 1);
        const PreviewState states[] = {
            PreviewState::Unknown,
            PreviewState::Checking,
            PreviewState::Previewable,
            PreviewState::Unavailable,
            PreviewState::LoadFailed};

        for (int row = 0; row < sourceModel.rowCount(); ++row)
        {
            sourceModel.setData(
                sourceModel.index(row, 0),
                static_cast<int>(states[row]),
                CacheEntryTableModel::PreviewStateRole);
        }

        GalleryFilterProxyModel proxyModel;
        proxyModel.setSourceModel(&sourceModel);
        proxyModel.SetPreviewFilter(GalleryPreviewFilter::ImagesOnly);

        Expect(
            proxyModel.rowCount() == 3,
            "images-only filter keeps pending and previewable rows");

        sourceModel.setData(
            sourceModel.index(0, 0),
            static_cast<int>(PreviewState::Unavailable),
            CacheEntryTableModel::PreviewStateRole);
        Expect(
            proxyModel.RefreshForPreviewStateChange(),
            "images-only filter refreshes after a preview state change");
        Expect(
            proxyModel.rowCount() == 2,
            "images-only filter removes a newly unavailable row");

        proxyModel.SetGalleryMode(false);
        Expect(
            proxyModel.rowCount() == 5,
            "table mode ignores the gallery-only filter");
        Expect(
            !proxyModel.RefreshForPreviewStateChange(),
            "table mode does not refilter for Gallery preview changes");
    }

    void TestCachePathNormalization()
    {
#ifdef _WIN32
        const QString alternatePath = QStringLiteral(
            "C:/Users/test/AppData/Local/SecondLife/texturecache");
        const QString nativePath = QStringLiteral(
            "C:\\Users\\test\\AppData\\Local\\SecondLife\\texturecache");

        Expect(
            PathToQString(PathFromQString(alternatePath)) == nativePath,
            "cache paths use native Windows separators for display");
        Expect(
            IsSameCachePath(
                PathFromQString(alternatePath),
                PathFromQString(nativePath)),
            "cache path comparison ignores Windows separator spelling");
        Expect(
            IsSameCachePath(
                PathFromQString(nativePath.toUpper()),
                PathFromQString(nativePath.toLower())),
            "cache path comparison remains case-insensitive on Windows");
#else
        const QString unnormalizedPath =
            QStringLiteral("/home/test/cache/../texturecache");
        const QString normalizedPath = QStringLiteral("/home/test/texturecache");

        Expect(
            PathToQString(
                PathFromQString(unnormalizedPath).lexically_normal()) ==
                normalizedPath,
            "cache paths use normalized native separators for display");
#endif
    }

    void TestRecentCachePathMigration()
    {
        QTemporaryDir settingsDirectory;
        QTemporaryDir cacheParent;
        Expect(
            settingsDirectory.isValid() && cacheParent.isValid(),
            "temporary path-migration directories are available");
        if (!settingsDirectory.isValid() || !cacheParent.isValid())
        {
            return;
        }

        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(
            QSettings::IniFormat,
            QSettings::UserScope,
            settingsDirectory.path());
        QCoreApplication::setOrganizationName(
            QStringLiteral("CacheExplorerTests"));
        QCoreApplication::setApplicationName(
            QStringLiteral("CachePathMigration"));

        const QString cacheDirectory = cacheParent.path() + "/texturecache";
        Expect(
            QDir().mkpath(cacheDirectory),
            "synthetic cache directory is created");
        QFile entriesFile(cacheDirectory + "/texture.entries");
        Expect(
            entriesFile.open(QIODevice::WriteOnly),
            "synthetic texture.entries is created");
        entriesFile.close();

        const QString nativePath = PathToQString(
            PathFromQString(cacheDirectory).lexically_normal());
        QString alternatePath = nativePath;
#ifdef _WIN32
        alternatePath.replace('\\', '/');
#else
        alternatePath += QStringLiteral("/../texturecache");
#endif

        QSettings settings;
        settings.clear();
        settings.setValue(
            QStringLiteral("cache/lastOpenedPath"),
            alternatePath);
        settings.setValue(
            QStringLiteral("cache/recentPaths"),
            QStringList{alternatePath, nativePath});
        settings.sync();

        const QStringList recentPaths = RecentCachePaths();
        Expect(
            recentPaths == QStringList{nativePath},
            "legacy recent path variants migrate to one native path");
        Expect(
            PreferredCachePath() == nativePath,
            "legacy last-opened path migrates to native form");

        settings.sync();
        Expect(
            settings.value(QStringLiteral("cache/recentPaths")).toStringList()
                == QStringList{nativePath},
            "migrated recent paths are persisted");
        Expect(
            settings.value(QStringLiteral("cache/lastOpenedPath")).toString()
                == nativePath,
            "migrated last-opened path is persisted");
    }

    void TestCacheFolderDialogStartPath()
    {
        const QString explicitPath = QStringLiteral("/chosen/cache/path");
        Expect(
            CacheFolderDialogStartPath(explicitPath) == explicitPath,
            "cache folder picker preserves a current path");

        const QString localDataPath =
            QStandardPaths::writableLocation(
                QStandardPaths::GenericDataLocation);
        const QString expectedFallback = localDataPath.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
            : localDataPath;
        Expect(
            CacheFolderDialogStartPath({}) == expectedFallback,
            "first cache folder picker starts in platform-local app data");
    }

    void TestTextureDetailsText()
    {
        CacheEntry entry{};
        entry.imageSize = 700;
        entry.bodySize = 100;
        entry.timestamp = 0;

        Expect(
            TextureDetailsText(entry) ==
                QStringLiteral(
                    "Cached 700 B of 700 B | Complete\nTime unavailable"),
            "texture details describe complete cached data");

        entry.imageSize = 2048;
        entry.bodySize = 400;
        const QString partialDetails = TextureDetailsText(entry, 512, 256);
        Expect(
            partialDetails.startsWith(QStringLiteral("512 x 256 pixels\n")),
            "texture details include decoded dimensions when available");
        Expect(
            partialDetails.contains(
                QStringLiteral("Cached 1000 B of 2.0 KiB | Partial")),
            "texture details describe partial cached data");
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    TestGalleryPreviewQueue();
    TestTryNextPreviewState();
    TestGalleryStatusText();
    TestInitialGalleryFilter();
    TestCachePathNormalization();
    TestRecentCachePathMigration();
    TestCacheFolderDialogStartPath();
    TestTextureDetailsText();

    if (gFailures != 0)
    {
        std::cerr << gFailures << " test assertion(s) failed.\n";
        return 1;
    }

    std::cout << "All cachegui helper tests passed.\n";
    return 0;
}
