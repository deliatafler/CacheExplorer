#pragma once

#include "TextureCacheDatabase.h"

#include <deque>

class CacheEntryTableModel;
class QListView;
class PreviewCache;
class QSortFilterProxyModel;

std::deque<CacheEntry> BuildVisibleGalleryPreviewQueue(
    const QListView& galleryView,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel,
    const PreviewCache& previewCache);
