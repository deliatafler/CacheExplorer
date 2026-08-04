#include "QtGalleryDiagnostics.h"

QString GalleryPerformanceDiagnosticText(
    const GalleryPreviewMetricsSnapshot& metrics)
{
    if (metrics.completed == 0)
    {
        return QStringLiteral(
            "Gallery thumbnail sample: not measured this session");
    }

    return QStringLiteral(
        "Gallery thumbnails checked: %1\n"
        "Gallery previews: %2\n"
        "Gallery unavailable: %3\n"
        "Gallery decode rate: %4 per second (single worker)")
        .arg(static_cast<qulonglong>(metrics.completed))
        .arg(static_cast<qulonglong>(metrics.succeeded))
        .arg(static_cast<qulonglong>(metrics.unavailable))
        .arg(metrics.previewsPerSecond, 0, 'f', 1);
}
