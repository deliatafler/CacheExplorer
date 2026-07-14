#pragma once

#include "GalleryActivityIndicator.h"
#include "GalleryPreviewQueue.h"

#include <deque>

class GalleryPreviewController
{
public:
    bool CanScheduleSearch(bool galleryMode, bool databaseOpen) const;
    void BeginScheduledSearch();
    void FinishScheduledSearch();
    void Clear();

    void RequestRefresh();
    bool ConsumeRefreshRequest();

    void ReplaceQueue(std::deque<CacheEntry> entries);
    bool HasQueuedEntries() const;
    CacheEntry TakeNextQueuedEntry();
    void MarkCompleted();

    GalleryActivityState ActivityState(
        bool galleryMode,
        bool databaseOpen,
        bool workerActive) const;

private:
    GalleryPreviewQueue queue_;
    bool searchPending_ = false;
};
