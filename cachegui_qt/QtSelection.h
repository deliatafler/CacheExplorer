#pragma once

#include "TextureCacheDatabase.h"

#include <QModelIndex>

class CacheEntryTableModel;
class QListView;
class QSortFilterProxyModel;
class QTableView;

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
