#pragma once

class TextureCacheDatabase;
class QWidget;
struct GalleryPreviewMetricsSnapshot;

void ShowAboutDialog(
    QWidget& parent,
    const TextureCacheDatabase& database,
    const GalleryPreviewMetricsSnapshot& galleryMetrics);
