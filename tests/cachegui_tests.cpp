#include "CacheEntryTableModel.h"
#include "GalleryFilterProxyModel.h"
#include "GalleryPreviewQueue.h"
#include "QtGalleryStatus.h"
#include "TryNextPreviewState.h"

#include <QStandardItemModel>
#include <QString>

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
        QStandardItemModel sourceModel(2, 1);
        sourceModel.setData(
            sourceModel.index(0, 0),
            true,
            CacheEntryTableModel::CachedCompleteRole);
        sourceModel.setData(
            sourceModel.index(1, 0),
            false,
            CacheEntryTableModel::CachedCompleteRole);

        GalleryFilterProxyModel proxyModel;
        proxyModel.setSourceModel(&sourceModel);
        proxyModel.SetPreviewFilter(GalleryPreviewFilter::CachedComplete);

        Expect(
            proxyModel.rowCount() == 1,
            "gallery filter applies before the first view toggle");

        proxyModel.SetGalleryMode(false);
        Expect(
            proxyModel.rowCount() == 2,
            "table mode ignores the gallery-only filter");
    }
}

int main()
{
    TestGalleryPreviewQueue();
    TestTryNextPreviewState();
    TestGalleryStatusText();
    TestInitialGalleryFilter();

    if (gFailures != 0)
    {
        std::cerr << gFailures << " test assertion(s) failed.\n";
        return 1;
    }

    std::cout << "All cachegui helper tests passed.\n";
    return 0;
}
