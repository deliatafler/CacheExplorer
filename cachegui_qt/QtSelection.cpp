#include "QtSelection.h"

#include "CacheEntryTableModel.h"
#include "PreviewCache.h"
#include "QtHelpers.h"

#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QTableView>

QModelIndex SelectedProxyIndex(
    bool galleryMode,
    const QTableView& table,
    const QListView& galleryView)
{
    if (galleryMode)
    {
        const QModelIndex galleryIndex = galleryView.currentIndex();

        if (galleryIndex.isValid())
        {
            return galleryIndex.sibling(galleryIndex.row(), 0);
        }
    }

    const QModelIndexList selectedRows =
        table.selectionModel()->selectedRows();

    if (!selectedRows.empty())
    {
        return selectedRows.front().sibling(selectedRows.front().row(), 0);
    }

    const QModelIndex galleryIndex = galleryView.currentIndex();

    if (galleryIndex.isValid())
    {
        return galleryIndex.sibling(galleryIndex.row(), 0);
    }

    return {};
}

const CacheEntry* SelectedEntry(
    bool galleryMode,
    const QTableView& table,
    const QListView& galleryView,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel)
{
    const QModelIndex selectedIndex =
        SelectedProxyIndex(galleryMode, table, galleryView);

    const QModelIndex sourceIndex =
        proxyModel.mapToSource(selectedIndex);

    if (!sourceIndex.isValid())
    {
        return nullptr;
    }

    return tableModel.EntryAt(sourceIndex.row());
}

void SelectEntry(
    const CacheEntry& entry,
    const CacheEntryTableModel& tableModel,
    const QSortFilterProxyModel& proxyModel,
    QTableView& table,
    QListView& galleryView)
{
    const int sourceRow = tableModel.RowForEntry(entry);

    if (sourceRow < 0)
    {
        return;
    }

    const QModelIndex sourceIndex = tableModel.index(sourceRow, 0);
    const QModelIndex proxyIndex = proxyModel.mapFromSource(sourceIndex);

    if (!proxyIndex.isValid())
    {
        return;
    }

    table.selectRow(proxyIndex.row());
    table.scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    galleryView.setCurrentIndex(proxyIndex);
    galleryView.scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
}

void SyncActiveViewSelection(
    bool galleryMode,
    const QModelIndex& selectedIndex,
    QTableView& table,
    QListView& galleryView)
{
    if (!selectedIndex.isValid())
    {
        return;
    }

    if (galleryMode)
    {
        galleryView.setCurrentIndex(selectedIndex);
        galleryView.scrollTo(selectedIndex, QAbstractItemView::PositionAtCenter);
        return;
    }

    table.selectRow(selectedIndex.row());
    table.scrollTo(selectedIndex, QAbstractItemView::PositionAtCenter);
}

CachedSelectionPreview CachedPreviewForSelection(
    const CacheEntry& entry,
    const PreviewCache& previewCache)
{
    const PreviewRecord* record = previewCache.Find(entry);

    if (record == nullptr ||
        record->state == PreviewState::Unknown)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Preview not loaded yet: ")
            + ToQString(entry.uuid.ToString());
        preview.panelMessage =
            QStringLiteral("Preview not loaded yet.");
        return preview;
    }

    if (record->state == PreviewState::Checking)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Checking preview: ")
            + ToQString(entry.uuid.ToString());
        preview.panelMessage =
            QStringLiteral("Checking preview...");
        return preview;
    }

    if (record->state == PreviewState::Unavailable)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("No preview available: ")
            + ToQString(entry.uuid.ToString());
        preview.panelMessage =
            QStringLiteral("No preview available.");
        return preview;
    }

    if (record->state == PreviewState::LoadFailed)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Preview file could not be loaded: ")
            + ToQString(entry.uuid.ToString());
        preview.panelMessage =
            QStringLiteral("Preview could not be loaded.");
        return preview;
    }

    if (record->pixmap.isNull())
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Preview not loaded yet: ")
            + ToQString(entry.uuid.ToString());
        preview.panelMessage =
            QStringLiteral("Preview not loaded yet.");
        return preview;
    }

    CachedSelectionPreview preview;
    preview.available = true;
    preview.pixmap = record->pixmap;
    preview.statusText =
        PreviewReadyStatus(
            entry.uuid.ToString(),
            record->width,
            record->height);
    return preview;
}
