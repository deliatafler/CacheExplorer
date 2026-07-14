#pragma once

#include "GalleryActivityIndicator.h"
#include "GalleryPreviewQueue.h"

#include <deque>
#include <optional>

class PreviewCache;

class GalleryPreviewController
{
public:
    bool CanScheduleSearch(bool galleryMode, bool databaseOpen) const;
    bool CanStartQueuedPreview(
        bool galleryMode,
        bool databaseOpen,
        bool busy,
        bool workerActive,
        bool tryNextActive) const;
    void BeginScheduledSearch();
    void FinishScheduledSearch();
    void Clear();

    void RequestRefresh();
    bool ConsumeRefreshRequest();

    void ReplaceQueue(std::deque<CacheEntry> entries);
    std::optional<CacheEntry> TakeNextAttemptableEntry(
        const PreviewCache& previewCache);
    void MarkCompleted();

    GalleryActivityState ActivityState(
        bool galleryMode,
        bool databaseOpen,
        bool workerActive) const;

private:
    GalleryPreviewQueue queue_;
    bool searchPending_ = false;
};
