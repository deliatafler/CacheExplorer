#include "GalleryPreviewQueue.h"

#include <utility>

void GalleryPreviewQueue::Clear()
{
    entries_.clear();
    total_ = 0;
    completed_ = 0;
    refreshPending_ = false;
}

void GalleryPreviewQueue::Replace(std::deque<CacheEntry> entries)
{
    entries_ = std::move(entries);
    total_ = entries_.size();
    completed_ = 0;
}

bool GalleryPreviewQueue::HasEntries() const
{
    return !entries_.empty();
}

CacheEntry GalleryPreviewQueue::TakeNext()
{
    CacheEntry entry = entries_.front();
    entries_.pop_front();
    return entry;
}

void GalleryPreviewQueue::MarkCompleted()
{
    if (completed_ < total_)
    {
        ++completed_;
    }
}

void GalleryPreviewQueue::RequestRefresh()
{
    refreshPending_ = true;
}

bool GalleryPreviewQueue::RefreshPending() const
{
    return refreshPending_;
}

bool GalleryPreviewQueue::ConsumeRefreshRequest()
{
    if (!refreshPending_)
    {
        return false;
    }

    refreshPending_ = false;
    return true;
}

std::size_t GalleryPreviewQueue::Total() const
{
    return total_;
}

std::size_t GalleryPreviewQueue::Completed() const
{
    return completed_;
}
