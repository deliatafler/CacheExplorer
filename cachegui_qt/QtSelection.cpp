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
    bool,
    const QTableView& table,
    const QListView&)
{
    const QItemSelectionModel* selectionModel = table.selectionModel();

    if (selectionModel == nullptr)
    {
        return {};
    }

    const QModelIndex currentIndex = selectionModel->currentIndex();

    if (currentIndex.isValid())
    {
        return currentIndex.sibling(currentIndex.row(), 0);
    }

    const QModelIndexList selectedRows = selectionModel->selectedRows();

    if (!selectedRows.empty())
    {
        return selectedRows.front().sibling(selectedRows.front().row(), 0);
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

std::vector<const CacheEntry*> SelectedEntries(
    const QTableView& table,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel)
{
    std::vector<const CacheEntry*> entries;
    const QItemSelectionModel* selectionModel = table.selectionModel();

    if (selectionModel == nullptr)
    {
        return entries;
    }

    const QModelIndexList selectedRows = selectionModel->selectedRows(0);
    entries.reserve(static_cast<std::size_t>(selectedRows.size()));

    for (const QModelIndex& proxyIndex : selectedRows)
    {
        const QModelIndex sourceIndex = proxyModel.mapToSource(proxyIndex);
        const CacheEntry* entry = tableModel.EntryAt(sourceIndex.row());

        if (entry != nullptr)
        {
            entries.push_back(entry);
        }
    }

    return entries;
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

    QItemSelectionModel* selectionModel = table.selectionModel();

    if (selectionModel != nullptr)
    {
        selectionModel->setCurrentIndex(
            proxyIndex,
            QItemSelectionModel::ClearAndSelect |
                QItemSelectionModel::Current |
                QItemSelectionModel::Rows);
    }

    table.scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
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
        galleryView.scrollTo(selectedIndex, QAbstractItemView::PositionAtCenter);
    }
    else
    {
        table.scrollTo(selectedIndex, QAbstractItemView::PositionAtCenter);
    }
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
        preview.panelMessageIsError = true;
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
