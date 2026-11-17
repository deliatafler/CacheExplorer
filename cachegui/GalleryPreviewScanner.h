#pragma once

#include "TextureCacheSource.h"

#include <deque>

class CacheEntryTableModel;
class QListView;
class PreviewCache;
class QSortFilterProxyModel;

std::deque<TextureCatalogEntry> BuildVisibleGalleryPreviewQueue(
    const QListView& galleryView,
    const QSortFilterProxyModel& proxyModel,
    const CacheEntryTableModel& tableModel,
    const PreviewCache& previewCache);
