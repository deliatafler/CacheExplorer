#include "GalleryPreviewScanner.h"

#include "CacheEntryTableModel.h"
#include "PreviewCache.h"

#include <algorithm>
#include <cstddef>

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

    const int firstRow = FirstVisibleGalleryProxyRow(galleryView, rowCount);

    constexpr int MaximumRowsToScan = 240;
    constexpr std::size_t MaximumQueueSize = 48;
    const int finalRow = std::min(rowCount, firstRow + MaximumRowsToScan);
    const QRect visibleArea = galleryView.viewport()->rect().adjusted(0, -170, 0, 170);

    for (int proxyRow = firstRow; proxyRow < finalRow; ++proxyRow)
    {
        const QModelIndex proxyIndex = proxyModel.index(proxyRow, 0);

        if (!proxyIndex.isValid() ||
            !galleryView.visualRect(proxyIndex).intersects(visibleArea))
        {
            continue;
        }

        const QModelIndex sourceIndex = proxyModel.mapToSource(proxyIndex);
        const CacheEntry* entry = tableModel.EntryAt(sourceIndex.row());

        if (entry != nullptr && previewCache.ShouldAttemptPreview(*entry))
        {
            entries.push_back(*entry);

            if (entries.size() >= MaximumQueueSize)
            {
                break;
            }
        }
    }

    return entries;
}
