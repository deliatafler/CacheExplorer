#include "GalleryPreviewController.h"

#include "PreviewCache.h"

#include <utility>

bool GalleryPreviewController::CanScheduleSearch(
    bool galleryMode,
    bool databaseOpen) const
{
    return galleryMode && databaseOpen && !searchPending_;
}

bool GalleryPreviewController::CanStartQueuedPreview(
    bool galleryMode,
    bool databaseOpen,
    bool busy,
    bool workerActive,
    bool tryNextActive) const
{
    return galleryMode &&
        databaseOpen &&
        !busy &&
        !workerActive &&
        !tryNextActive;
}

void GalleryPreviewController::BeginScheduledSearch()
{
    searchPending_ = true;
}

GallerySearchAction GalleryPreviewController::FinishScheduledSearch(
    bool galleryMode,
    bool databaseOpen,
    bool workerActive)
{
    searchPending_ = false;

    if (!galleryMode || !databaseOpen)
    {
        Clear();
        return GallerySearchAction::Clear;
    }

    if (workerActive)
    {
        RequestRefresh();
        return GallerySearchAction::Refresh;
    }

    return GallerySearchAction::Rebuild;
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

std::optional<CacheEntry> GalleryPreviewController::TakeNextAttemptableEntry(
    const PreviewCache& previewCache)
{
    while (queue_.HasEntries())
    {
        const CacheEntry entry = queue_.TakeNext();

        if (previewCache.ShouldAttemptPreview(entry))
        {
            return entry;
        }

        queue_.MarkCompleted();
    }

    queue_.Replace({});
    return std::nullopt;
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
