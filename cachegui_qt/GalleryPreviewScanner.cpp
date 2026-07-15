#include "GalleryPreviewScanner.h"

#include "CacheEntryTableModel.h"
#include "PreviewCache.h"

#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <vector>

#include <QListView>
#include <QModelIndex>
#include <QPoint>
#include <QRect>
#include <QSortFilterProxyModel>
#include <QWidget>

namespace
{
    int FirstVisibleGalleryProxyRow(
        const QListView& galleryView,
        int rowCount)
    {
        const int width = galleryView.viewport()->width();
        const int height = galleryView.viewport()->height();

        if (width > 0 && height > 0)
        {
            const int xPoints[] = {0, width / 4, width / 2, (width * 3) / 4, width - 1};
            const int yPoints[] = {0, height / 4, height / 2, (height * 3) / 4, height - 1};
            int bestRow = rowCount;

            for (const int y : yPoints)
            {
                for (const int x : xPoints)
                {
                    const QModelIndex index = galleryView.indexAt(QPoint(x, y));

                    if (index.isValid())
                    {
                        bestRow = std::min(bestRow, index.row());
                    }
                }
            }

            if (bestRow != rowCount)
            {
                return bestRow;
            }
        }

        if (galleryView.currentIndex().isValid())
        {
            return galleryView.currentIndex().row();
        }

        return 0;
    }

    struct GalleryCandidate
    {
        int proxyRow = 0;
        int distanceFromCenter = 0;
    };

    int DistanceFromViewportCenter(
        const QRect& itemRect,
        const QRect& viewportRect)
    {
        return std::abs(itemRect.center().y() - viewportRect.center().y());
    }

    void AddAttemptableEntry(
        std::deque<CacheEntry>& entries,
        int proxyRow,
        const QSortFilterProxyModel& proxyModel,
        const CacheEntryTableModel& tableModel,
        const PreviewCache& previewCache)
    {
        const QModelIndex proxyIndex = proxyModel.index(proxyRow, 0);

        if (!proxyIndex.isValid())
        {
            return;
        }

        const QModelIndex sourceIndex = proxyModel.mapToSource(proxyIndex);
        const CacheEntry* entry = tableModel.EntryAt(sourceIndex.row());

        if (entry != nullptr && previewCache.ShouldAttemptPreview(*entry))
        {
            entries.push_back(*entry);
        }
    }
}

std::deque<CacheEntry> BuildVisibleGalleryPreviewQueue(
    const QListView& galleryView,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel,
    const PreviewCache& previewCache)
{
    std::deque<CacheEntry> entries;

    const int rowCount = proxyModel.rowCount();

    if (rowCount <= 0)
    {
        return entries;
    }

    const int firstVisibleRow = FirstVisibleGalleryProxyRow(galleryView, rowCount);

    constexpr int MaximumRowsToScan = 260;
    constexpr std::size_t MaximumQueueSize = 48;
    constexpr std::size_t MaximumCandidates = MaximumQueueSize * 3;
    const int finalRow =
        std::min(rowCount, firstVisibleRow + MaximumRowsToScan);
    const QRect viewportRect = galleryView.viewport()->rect();
    const QRect nearbyArea = viewportRect.adjusted(0, -220, 0, 220);
    std::vector<GalleryCandidate> visibleCandidates;
    std::vector<int> lookaheadRows;

    for (int proxyRow = firstVisibleRow; proxyRow < finalRow; ++proxyRow)
    {
        const QModelIndex proxyIndex = proxyModel.index(proxyRow, 0);

        if (!proxyIndex.isValid())
        {
            continue;
        }

        const QRect itemRect = galleryView.visualRect(proxyIndex);

        if (!itemRect.intersects(nearbyArea))
        {
            continue;
        }

        if (itemRect.intersects(viewportRect))
        {
            visibleCandidates.push_back(
                GalleryCandidate{
                    proxyRow,
                    DistanceFromViewportCenter(itemRect, viewportRect)});
            if (visibleCandidates.size() >= MaximumCandidates)
            {
                break;
            }
            continue;
        }

        lookaheadRows.push_back(proxyRow);
        if (visibleCandidates.size() + lookaheadRows.size() >= MaximumCandidates)
        {
            break;
        }
    }

    std::sort(
        visibleCandidates.begin(),
        visibleCandidates.end(),
        [](const GalleryCandidate& left, const GalleryCandidate& right)
        {
            if (left.distanceFromCenter != right.distanceFromCenter)
            {
                return left.distanceFromCenter < right.distanceFromCenter;
            }

            return left.proxyRow < right.proxyRow;
        });

    for (const GalleryCandidate& candidate : visibleCandidates)
    {
        AddAttemptableEntry(
            entries,
            candidate.proxyRow,
            proxyModel,
            tableModel,
            previewCache);

        if (entries.size() >= MaximumQueueSize)
        {
            return entries;
        }
    }

    for (const int proxyRow : lookaheadRows)
    {
        AddAttemptableEntry(
            entries,
            proxyRow,
            proxyModel,
            tableModel,
            previewCache);

        if (entries.size() >= MaximumQueueSize)
        {
            return entries;
        }
    }

    return entries;
}
