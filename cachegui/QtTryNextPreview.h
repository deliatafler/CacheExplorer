#pragma once

#include "TextureCacheSource.h"
#include "TryNextPreviewState.h"

#include <QString>

class CacheEntryTableModel;
class QModelIndex;
class QSortFilterProxyModel;

int FirstTryNextProxyRow(const QModelIndex& selectedIndex);

const TextureCatalogEntry* TakeNextTryNextEntry(
    TryNextPreviewState& state,
    QSortFilterProxyModel& proxyModel,
    CacheEntryTableModel& tableModel);

QString TryNextSearchStartedStatus();
QString TryNextSearchExhaustedStatus(int attempts);
QString TryingPreviewStatus(const TextureCatalogEntry& entry);
