#include "GalleryPreviewController.h"

#include <utility>

bool GalleryPreviewController::CanScheduleSearch(
    bool galleryMode,
    bool databaseOpen) const
{
    return galleryMode && databaseOpen && !searchPending_;
}

void GalleryPreviewController::BeginScheduledSearch()
{
    searchPending_ = true;
}

void GalleryPreviewController::FinishScheduledSearch()
{
    searchPending_ = false;
}

void GalleryPreviewController::Clear()
{
    queue_.Clear();
    searchPending_ = false;
}

void GalleryPreviewController::RequestRefresh()
{
    queue_.RequestRefresh();
}

bool GalleryPreviewController::ConsumeRefreshRequest()
{
    return queue_.ConsumeRefreshRequest();
}

void GalleryPreviewController::ReplaceQueue(std::deque<CacheEntry> entries)
{
    queue_.Replace(std::move(entries));
}

bool GalleryPreviewController::HasQueuedEntries() const
{
    return queue_.HasEntries();
}

CacheEntry GalleryPreviewController::TakeNextQueuedEntry()
{
    return queue_.TakeNext();
}

void GalleryPreviewController::MarkCompleted()
{
    queue_.MarkCompleted();
}

GalleryActivityState GalleryPreviewController::ActivityState(
    bool galleryMode,
    bool databaseOpen,
    bool workerActive) const
{
    return GalleryActivityState{
        galleryMode,
        databaseOpen,
        workerActive,
        searchPending_,
        queue_.RefreshPending(),
        queue_.HasEntries(),
        queue_.Total(),
        queue_.Completed()};
}
