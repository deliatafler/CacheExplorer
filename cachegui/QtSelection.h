#pragma once

#include "TextureCacheSource.h"

#include <QModelIndex>
#include <QPixmap>
#include <QString>

#include <vector>

class CacheEntryTableModel;
class QListView;
class PreviewCache;
class QSortFilterProxyModel;
class QTableView;

struct CachedSelectionPreview
{
    bool available = false;
    QPixmap pixmap;
    QString statusText;
    QString panelMessage;
    bool panelMessageIsError = false;
};

QModelIndex SelectedProxyIndex(
    bool galleryMode,
    const QTableView& table,
    const QListView& galleryView);

const TextureCatalogEntry* SelectedEntry(
    bool galleryMode,
    const QTableView& table,
    const QListView& galleryView,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel);

std::vector<const TextureCatalogEntry*> SelectedEntries(
    const QTableView& table,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel);

bool SelectEntry(
    const TextureCatalogEntry& entry,
    const CacheEntryTableModel& tableModel,
    const QSortFilterProxyModel& proxyModel,
    QTableView& table,
    QListView& galleryView);

void SyncActiveViewSelection(
    bool galleryMode,
    const QModelIndex& selectedIndex,
    QTableView& table,
    QListView& galleryView);

CachedSelectionPreview CachedPreviewForSelection(
    const TextureCatalogEntry& entry,
    const PreviewCache& previewCache);
