#pragma once

#include <chrono>
#include <cstddef>

struct GalleryPreviewMetricsSnapshot
{
    std::size_t completed = 0;
    std::size_t succeeded = 0;
    std::size_t unavailable = 0;
    double previewsPerSecond = 0.0;
};

class GalleryPreviewMetrics
{
public:
    void Reset();
    void Record(std::chrono::milliseconds duration, bool succeeded);
    GalleryPreviewMetricsSnapshot Snapshot() const;

private:
    std::size_t completed_ = 0;
    std::size_t succeeded_ = 0;
    std::chrono::milliseconds totalDuration_{0};
};
