#include "GalleryPreviewMetrics.h"

#include <algorithm>

void GalleryPreviewMetrics::Reset()
{
    completed_ = 0;
    succeeded_ = 0;
    totalDuration_ = std::chrono::milliseconds{0};
}

void GalleryPreviewMetrics::Record(
    std::chrono::milliseconds duration,
    bool succeeded)
{
    ++completed_;
    succeeded_ += succeeded ? 1u : 0u;
    totalDuration_ += std::max(duration, std::chrono::milliseconds{1});
}

GalleryPreviewMetricsSnapshot GalleryPreviewMetrics::Snapshot() const
{
    const double previewsPerSecond = totalDuration_.count() > 0
        ? (static_cast<double>(completed_) * 1000.0) /
            static_cast<double>(totalDuration_.count())
        : 0.0;

    return GalleryPreviewMetricsSnapshot{
        completed_,
        succeeded_,
        completed_ - succeeded_,
        previewsPerSecond};
}
