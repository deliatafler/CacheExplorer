#pragma once

#include <cstddef>

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
};

class GalleryActivityIndicator
{
public:
    void SetLabel(QLabel* label);
    void Update(const GalleryActivityState& state);

private:
    QLabel* label_ = nullptr;
};
