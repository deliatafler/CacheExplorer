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

const TextureCatalogEntry* SelectedEntry(
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

std::vector<const TextureCatalogEntry*> SelectedEntries(
    const QTableView& table,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel)
{
    std::vector<const TextureCatalogEntry*> entries;
    const QItemSelectionModel* selectionModel = table.selectionModel();

    if (selectionModel == nullptr)
    {
        return entries;
    }

    QModelIndexList selectedRows = selectionModel->selectedRows(0);

    if (selectedRows.empty())
    {
        const QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

        for (const QModelIndex& selectedIndex : selectedIndexes)
        {
            if (selectedIndex.column() == 0)
            {
                selectedRows.push_back(selectedIndex);
            }
        }
    }

    entries.reserve(static_cast<std::size_t>(selectedRows.size()));

    for (const QModelIndex& proxyIndex : selectedRows)
    {
        const QModelIndex sourceIndex = proxyModel.mapToSource(proxyIndex);
        const TextureCatalogEntry* entry =
            tableModel.EntryAt(sourceIndex.row());

        if (entry != nullptr)
        {
            entries.push_back(entry);
        }
    }

    return entries;
}

bool SelectEntry(
    const TextureCatalogEntry& entry,
    const CacheEntryTableModel& tableModel,
    const QSortFilterProxyModel& proxyModel,
    QTableView& table,
    QListView& galleryView)
{
    const int sourceRow = tableModel.RowForEntry(entry);

    if (sourceRow < 0)
    {
        return false;
    }

    const QModelIndex sourceIndex = tableModel.index(sourceRow, 0);
    const QModelIndex proxyIndex = proxyModel.mapFromSource(sourceIndex);

    if (!proxyIndex.isValid())
    {
        return false;
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
    return true;
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
    const TextureCatalogEntry& entry,
    const PreviewCache& previewCache)
{
    const PreviewRecord* record = previewCache.Find(entry);

    if (record == nullptr ||
        record->state == PreviewState::Unknown)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Preview not loaded yet: ")
            + ToQString(entry.DisplayIdentifier());
        preview.panelMessage =
            QStringLiteral("Preview not loaded yet.");
        return preview;
    }

    if (record->state == PreviewState::Checking)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Checking preview: ")
            + ToQString(entry.DisplayIdentifier());
        preview.panelMessage =
            QStringLiteral("Checking preview...");
        return preview;
    }

    if (record->state == PreviewState::Unavailable)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("No preview available: ")
            + ToQString(entry.DisplayIdentifier());
        preview.panelMessage =
            QStringLiteral("No preview available.");
        return preview;
    }

    if (record->state == PreviewState::LoadFailed)
    {
        CachedSelectionPreview preview;
        preview.statusText =
            QStringLiteral("Preview file could not be loaded: ")
            + ToQString(entry.DisplayIdentifier());
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
            + ToQString(entry.DisplayIdentifier());
        preview.panelMessage =
            QStringLiteral("Preview not loaded yet.");
        return preview;
    }

    CachedSelectionPreview preview;
    preview.available = true;
    preview.pixmap = record->pixmap;
    preview.statusText =
        PreviewReadyStatus(
            entry.DisplayIdentifier(),
            record->width,
            record->height);
    return preview;
}
