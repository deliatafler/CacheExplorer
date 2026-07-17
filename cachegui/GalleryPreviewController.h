#pragma once

#include "GalleryActivityIndicator.h"
#include "GalleryPreviewMetrics.h"
#include "GalleryPreviewQueue.h"

#include <chrono>
#include <deque>
#include <optional>

class PreviewCache;

enum class GallerySearchAction
{
    None,
    Clear,
    Refresh,
    Rebuild
};

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
    GallerySearchAction FinishScheduledSearch(
        bool galleryMode,
        bool databaseOpen,
        bool workerActive);
    void Clear();

    void RequestRefresh();
    bool ConsumeRefreshRequest();

    void ReplaceQueue(std::deque<CacheEntry> entries);
    std::optional<CacheEntry> TakeNextAttemptableEntry(
        const PreviewCache& previewCache);
    void MarkCompleted(std::chrono::milliseconds duration, bool succeeded);
    void ResetMetrics();

    GalleryActivityState ActivityState(
        bool galleryMode,
        bool databaseOpen,
        bool workerActive) const;

private:
    GalleryPreviewQueue queue_;
    GalleryPreviewMetrics metrics_;
    bool searchPending_ = false;
};
