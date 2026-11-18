#pragma once

class ViewerTextureCacheSource;
class QWidget;
struct GalleryPreviewMetricsSnapshot;

void ShowAboutDialog(
    QWidget& parent,
    const ViewerTextureCacheSource& source,
    const GalleryPreviewMetricsSnapshot& galleryMetrics);
