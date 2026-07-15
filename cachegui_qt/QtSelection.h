#pragma once

#include "TextureCacheDatabase.h"

#include <QModelIndex>
#include <QPixmap>
#include <QString>

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

const CacheEntry* SelectedEntry(
    bool galleryMode,
    const QTableView& table,
    const QListView& galleryView,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel);

void SelectEntry(
    const CacheEntry& entry,
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
    const CacheEntry& entry,
    const PreviewCache& previewCache);
