#pragma once

#include <cstddef>

#include <QString>

class QLabel;

struct GalleryActivityState
{
    bool galleryMode = false;
    bool databaseOpen = false;
    bool workerActive = false;
    bool searchPending = false;
    bool refreshPending = false;
    bool hasQueuedEntries = false;
    std::size_t queueTotal = 0;
    std::size_t queueCompleted = 0;
    std::size_t measuredCompleted = 0;
    std::size_t measuredSucceeded = 0;
    std::size_t measuredUnavailable = 0;
    double previewsPerSecond = 0.0;
};

QString GalleryActivityText(const GalleryActivityState& state);
QString GalleryActivityTooltip(const GalleryActivityState& state);

class GalleryActivityIndicator
{
public:
    void SetLabel(QLabel* label);
    void Update(const GalleryActivityState& state);

private:
    QLabel* label_ = nullptr;
};
