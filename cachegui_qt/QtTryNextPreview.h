#pragma once

#include "TextureCacheDatabase.h"
#include "TryNextPreviewState.h"

#include <QString>

class CacheEntryTableModel;
class QModelIndex;
class QSortFilterProxyModel;

int FirstTryNextProxyRow(const QModelIndex& selectedIndex);

const CacheEntry* TakeNextTryNextEntry(
    TryNextPreviewState& state,
    QSortFilterProxyModel& proxyModel,
    CacheEntryTableModel& tableModel);

QString TryNextSearchStartedStatus();
QString TryNextSearchExhaustedStatus(int attempts);
QString TryingPreviewStatus(const CacheEntry& entry);
