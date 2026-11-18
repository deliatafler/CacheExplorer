#include "CacheEntryTableModel.h"
#include "GalleryActivityIndicator.h"
#include "GalleryFilterProxyModel.h"
#include "GalleryPreviewMetrics.h"
#include "GalleryPreviewQueue.h"
#include "PreviewCache.h"
#include "QtGalleryStatus.h"
#include "QtGalleryDiagnostics.h"
#include "QtHelpers.h"
#include "QtTextureDetails.h"
#include "QtWindowState.h"
#include "TryNextPreviewState.h"

#include <QStandardItemModel>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

    class TestTextureCacheSource final : public TextureCacheSource
    {
    public:
        TextureCacheOpenError Open(
            const std::filesystem::path& cacheDirectory) override
        {
            cacheDirectory_ = cacheDirectory;
            open_ = true;
            return TextureCacheOpenError::None;
        }

        void Close() override
        {
            open_ = false;
            entries.clear();
        }

        bool IsOpen() const override
        {
            return open_;
        }

        TextureCacheFormat Format() const override
        {
            return TextureCacheFormat::LegacyViewer;
        }

        const std::filesystem::path& CacheDirectory() const override
        {
            return cacheDirectory_;
        }

        const std::vector<TextureCatalogEntry>& Entries() const override
        {
            return entries;
        }

        const TextureCatalogEntry* FindByUuid(
            const UUID& uuid) const override
        {
            for (const TextureCatalogEntry& entry : entries)
            {
                if (entry.uuid.has_value() && *entry.uuid == uuid)
                {
                    return &entry;
                }
            }

            return nullptr;
        }

        TexturePayloadError ReadPayload(
            const std::string&,
            TexturePayload&) const override
        {
            return TexturePayloadError::UnsupportedFormat;
        }

        std::unique_ptr<TexturePayloadRequest> CreatePayloadRequest(
            const std::string&) const override
        {
            return {};
        }

        bool open_ = true;
        std::filesystem::path cacheDirectory_;
        std::vector<TextureCatalogEntry> entries;
    };

    TextureCatalogEntry MakeEntry(std::uint32_t sourceIndex)
    {
        TextureCatalogEntry entry{};
        entry.sourceKey = std::to_string(sourceIndex);
        entry.sourceIndex = sourceIndex;
        return entry;
    }

    void TestGalleryPreviewQueue()
    {
        GalleryPreviewQueue queue;
        queue.Replace({MakeEntry(4), MakeEntry(7)});

        Expect(queue.HasEntries(), "queue has replaced entries");
        Expect(queue.Total() == 2, "queue records total entries");
        Expect(queue.Completed() == 0, "queue starts with no completed entries");
        Expect(
            queue.TakeNext().sourceIndex == 4,
            "queue preserves entry order");

        queue.MarkCompleted();
        Expect(queue.Completed() == 1, "queue increments completed entries");
        Expect(
            queue.TakeNext().sourceIndex == 7,
            "queue returns second entry");
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

    void TestCacheEntryTableModelUsesNeutralCatalog()
    {
        const std::optional<UUID> uuid = UUID::FromString(
            "11111111-2222-3333-4444-555555555555");
        Expect(uuid.has_value(), "table model test UUID parses");

        TextureCatalogEntry legacyEntry;
        legacyEntry.sourceKey = uuid.has_value()
            ? uuid->ToString()
            : std::string("legacy-entry");
        legacyEntry.uuid = uuid;
        legacyEntry.expectedBytes = 700;
        legacyEntry.cachedBytes = 700;
        legacyEntry.payloadBytes = 100;
        legacyEntry.sourceIndex = 12;
        legacyEntry.timestamp = 1000;

        TextureCatalogEntry anonymousEntry;
        anonymousEntry.sourceKey = "0123456789abcdef0123456789abcdef";
        anonymousEntry.cachedBytes = 4096;
        anonymousEntry.payloadBytes = 4096;

        TestTextureCacheSource source;
        source.entries = {legacyEntry, anonymousEntry};

        CacheEntryTableModel model;
        model.SetSource(&source);

        Expect(model.rowCount() == 2, "table model reads neutral entry count");
        Expect(
            model.index(0, 0).data(Qt::DisplayRole).toString() ==
                QStringLiteral("11111111-2222-3333-4444-555555555555"),
            "table model displays a known UUID");
        Expect(
            model.index(0, 1).data(Qt::DisplayRole).toULongLong() == 700,
            "table model preserves legacy image size");
        Expect(
            model.index(0, 2).data(Qt::DisplayRole).toULongLong() == 100,
            "table model preserves legacy body size");
        Expect(
            model.index(0, 3).data(CacheEntryTableModel::CacheIndexRole)
                .toULongLong() == 12,
            "table model preserves legacy cache index role");
        Expect(
            model.index(0, 0).data(
                CacheEntryTableModel::CachedCompleteRole).toBool(),
            "table model reports complete neutral payload metadata");
        Expect(
            model.index(1, 0).data(Qt::DisplayRole).toString() ==
                QStringLiteral("0123456789abcdef0123456789abcdef"),
            "table model displays an opaque source identifier");
        Expect(
            !model.index(1, 3).data(
                CacheEntryTableModel::CacheIndexRole).isValid(),
            "table model leaves unavailable source sequence blank");
        Expect(
            model.RowForEntry(anonymousEntry) == 1,
            "table model finds entries by stable source key");
    }

    void TestGalleryPreviewMetrics()
    {
        GalleryPreviewMetrics metrics;
        metrics.Record(std::chrono::milliseconds{100}, true);
        metrics.Record(std::chrono::milliseconds{300}, false);

        const GalleryPreviewMetricsSnapshot snapshot = metrics.Snapshot();
        Expect(snapshot.completed == 2, "metrics count completed previews");
        Expect(snapshot.succeeded == 1, "metrics count successful previews");
        Expect(snapshot.unavailable == 1, "metrics count unavailable previews");
        Expect(
            snapshot.previewsPerSecond == 5.0,
            "metrics calculate single-worker decode throughput");

        GalleryActivityState activity;
        activity.workerActive = true;
        activity.queueTotal = 8;
        activity.queueCompleted = 2;
        activity.measuredCompleted = snapshot.completed;
        activity.measuredSucceeded = snapshot.succeeded;
        activity.measuredUnavailable = snapshot.unavailable;
        activity.previewsPerSecond = snapshot.previewsPerSecond;
        Expect(
            GalleryActivityText(activity) ==
                QStringLiteral("Loading thumbnails 3 / 8 (5.0/s)"),
            "activity text includes measured thumbnail throughput");
        Expect(
            GalleryActivityTooltip(activity) ==
                QStringLiteral(
                    "This cache: 2 checked, 1 previews, 1 unavailable, 5.0 per second"),
            "activity tooltip includes measured thumbnail outcomes");
        Expect(
            GalleryPerformanceDiagnosticText(snapshot) ==
                QStringLiteral(
                    "Gallery thumbnails checked: 2\n"
                    "Gallery previews: 1\n"
                    "Gallery unavailable: 1\n"
                    "Gallery decode rate: 5.0 per second (single worker)"),
            "About diagnostics retain the complete Gallery measurement");

        metrics.Reset();
        Expect(
            metrics.Snapshot().completed == 0,
            "metrics reset between opened caches");
        Expect(
            GalleryPerformanceDiagnosticText(metrics.Snapshot()) ==
                QStringLiteral(
                    "Gallery thumbnail sample: not measured this session"),
            "About diagnostics explain an empty Gallery sample");
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
        Expect(
            RefreshedCacheStatus(48, 64, 1.71f, 1).contains(
                QStringLiteral("1 entry added or updated")),
            "refresh status uses a singular recent-change count");
        Expect(
            RefreshedCacheStatus(48, 64, 1.71f, 12).contains(
                QStringLiteral("12 entries added or updated")),
            "refresh status reports multiple recent changes");
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
            sourceModel.setData(
                sourceModel.index(row, 0),
                row + 100,
                CacheEntryTableModel::CacheIndexRole);
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

        proxyModel.SetGalleryMode(true);
        proxyModel.SetRecentCacheIndices({101, 104});
        proxyModel.SetPreviewFilter(GalleryPreviewFilter::RecentChanges);
        Expect(
            proxyModel.rowCount() == 2,
            "recent-changes filter keeps only changed cache indices");
        Expect(
            proxyModel.index(0, 0).data(
                CacheEntryTableModel::CacheIndexRole).toInt() == 101 &&
                proxyModel.index(1, 0).data(
                    CacheEntryTableModel::CacheIndexRole).toInt() == 104,
            "recent-changes filter preserves source ordering");

        proxyModel.SetRecentCacheIndices({103});
        Expect(
            proxyModel.rowCount() == 1 &&
                proxyModel.index(0, 0).data(
                    CacheEntryTableModel::CacheIndexRole).toInt() == 103,
            "recent-changes filter refreshes when a new delta is supplied");

        proxyModel.SetGalleryMode(false);
        Expect(
            proxyModel.rowCount() == 5,
            "table mode ignores the recent-changes filter");
    }

    void TestCachePathNormalization()
    {
#ifdef _WIN32
        const QString localAppData = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation);
        Expect(
            !localAppData.isEmpty(),
            "Windows local application-data path is available");
        if (localAppData.isEmpty())
        {
            return;
        }

        const QString nativePath = PathToQString(
            PathFromQString(localAppData) / "SecondLife" / "texturecache");
        QString alternatePath = nativePath;
        alternatePath.replace('\\', '/');

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
        const std::filesystem::path homePath = PathFromQString(
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        const QString unnormalizedPath = PathToQString(
            homePath / "cache" / ".." / "texturecache");
        const QString normalizedPath = PathToQString(
            (homePath / "texturecache").lexically_normal());

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

        const QStandardPaths::StandardLocation startLocation =
#ifdef __APPLE__
            QStandardPaths::GenericCacheLocation;
#else
            QStandardPaths::GenericDataLocation;
#endif
        const QString platformStartPath =
            QStandardPaths::writableLocation(
                startLocation);
        const QString expectedFallback = platformStartPath.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
            : platformStartPath;
        Expect(
            CacheFolderDialogStartPath({}) == expectedFallback,
            "first cache folder picker starts in the platform cache root");
    }

    void TestAuroraCachePathResolution()
    {
        QTemporaryDir directory;
        Expect(
            directory.isValid(),
            "temporary Aurora path directory is available");
        if (!directory.isValid())
        {
            return;
        }

        const std::filesystem::path selectedParent =
            PathFromQString(directory.path());
        const std::filesystem::path cacheRoot =
            selectedParent / "AuroraViewerCache";
        const std::filesystem::path store = cacheRoot / "store";
        const std::filesystem::path textureStore = store / "tex";

        std::error_code error;
        std::filesystem::create_directories(textureStore, error);
        Expect(!error, "synthetic Aurora texture store is created");

        Expect(
            ResolveTextureCacheDirectory(cacheRoot) ==
                std::filesystem::weakly_canonical(cacheRoot, error),
            "Aurora cache root resolves to itself");
        Expect(
            ResolveTextureCacheDirectory(store) ==
                std::filesystem::weakly_canonical(cacheRoot, error),
            "Aurora store directory resolves to its cache root");
        Expect(
            ResolveTextureCacheDirectory(textureStore) ==
                std::filesystem::weakly_canonical(cacheRoot, error),
            "Aurora texture directory resolves to its cache root");
        Expect(
            ResolveTextureCacheDirectory(selectedParent) ==
                std::filesystem::weakly_canonical(cacheRoot, error),
            "parent containing AuroraViewerCache resolves to the cache root");
    }

    void TestWindowStatePersistence()
    {
        QTemporaryDir settingsDirectory;
        Expect(
            settingsDirectory.isValid(),
            "temporary window-state settings directory is available");
        if (!settingsDirectory.isValid())
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
            QStringLiteral("WindowStatePersistence"));

        QSettings settings;
        settings.clear();
        settings.sync();

        const QtWindowState emptyState = LoadQtWindowState();
        Expect(
            emptyState.windowGeometry.isEmpty() &&
                emptyState.contentSplitterState.isEmpty(),
            "window state is empty before it is first saved");

        const QtWindowState expectedState{
            QByteArray("geometry-state"),
            QByteArray("splitter-state")};
        SaveQtWindowState(expectedState);
        settings.sync();

        const QtWindowState restoredState = LoadQtWindowState();
        Expect(
            restoredState.windowGeometry == expectedState.windowGeometry,
            "window geometry round-trips through Qt settings");
        Expect(
            restoredState.contentSplitterState ==
                expectedState.contentSplitterState,
            "content splitter state round-trips through Qt settings");
    }

    void TestPngExportDirectoryPersistence()
    {
        QTemporaryDir settingsDirectory;
        QTemporaryDir exportDirectory;
        Expect(
            settingsDirectory.isValid() && exportDirectory.isValid(),
            "temporary export settings directories are available");
        if (!settingsDirectory.isValid() || !exportDirectory.isValid())
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
            QStringLiteral("PngExportDirectoryPersistence"));

        QSettings settings;
        settings.clear();
        settings.sync();

        const QString fallbackDirectory = PreferredPngExportDirectory();
        Expect(
            !fallbackDirectory.isEmpty(),
            "PNG export has a platform user-directory fallback");

        const QString expectedDirectory = PathToQString(
            PathFromQString(exportDirectory.path()).lexically_normal());
        RememberPngExportDirectory(exportDirectory.path());
        settings.sync();

        Expect(
            PreferredPngExportDirectory() == expectedDirectory,
            "PNG export remembers the last selected directory");
        Expect(
            settings.value(QStringLiteral("export/lastDirectory")).toString()
                == expectedDirectory,
            "remembered PNG export directory is stored in native form");

        settings.setValue(
            QStringLiteral("export/lastDirectory"),
            exportDirectory.path() + QStringLiteral("/missing"));
        settings.sync();
        Expect(
            PreferredPngExportDirectory() == fallbackDirectory,
            "missing PNG export directories fall back to a user location");
    }

    void TestTextureDetailsText()
    {
        TextureCatalogEntry entry{};
        entry.expectedBytes = 700;
        entry.cachedBytes = 700;
        entry.payloadBytes = 100;
        entry.timestamp = 0;
        entry.hasCompleteJpeg2000 = true;

        Expect(
            TextureDetailsText(entry) ==
                QStringLiteral(
                    "Cache data: Complete JPEG2000\n"
                    "Cached 700 B of 700 B | Complete\n"
                    "Time unavailable"),
            "texture details describe complete cached data");

        entry.expectedBytes = 2048;
        entry.cachedBytes = 1000;
        entry.payloadBytes = 400;
        entry.hasCompleteJpeg2000 = false;
        entry.hasPartialJpeg2000 = true;
        entry.hasBc7 = true;
        const QString partialDetails = TextureDetailsText(entry, 512, 256);
        Expect(
            partialDetails.startsWith(QStringLiteral("512 x 256 pixels\n")),
            "texture details include decoded dimensions when available");
        Expect(
            partialDetails.contains(
                QStringLiteral("Cached 1000 B of 2.0 KiB | Partial")),
            "texture details describe partial cached data");
        Expect(
            partialDetails.contains(
                QStringLiteral(
                    "Cache data: partial JPEG2000 + BC7")),
            "texture details describe mixed Aurora cache data");

        entry.hasPartialJpeg2000 = false;
        Expect(
            TextureDetailsText(entry).startsWith(
                QStringLiteral("Cache data: BC7 only\n")),
            "texture details identify BC7-only entries");
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    TestGalleryPreviewQueue();
    TestCacheEntryTableModelUsesNeutralCatalog();
    TestGalleryPreviewMetrics();
    TestTryNextPreviewState();
    TestGalleryStatusText();
    TestInitialGalleryFilter();
    TestCachePathNormalization();
    TestRecentCachePathMigration();
    TestCacheFolderDialogStartPath();
    TestAuroraCachePathResolution();
    TestWindowStatePersistence();
    TestPngExportDirectoryPersistence();
    TestTextureDetailsText();

    if (gFailures != 0)
    {
        std::cerr << gFailures << " test assertion(s) failed.\n";
        return 1;
    }

    std::cout << "All cachegui helper tests passed.\n";
    return 0;
}
