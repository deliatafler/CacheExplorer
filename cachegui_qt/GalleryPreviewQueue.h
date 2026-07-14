#pragma once

#include "TextureCacheDatabase.h"

#include <cstddef>
#include <deque>

class GalleryPreviewQueue
{
public:
    void Clear();

    void Replace(std::deque<CacheEntry> entries);

    bool HasEntries() const;

    CacheEntry TakeNext();

    void MarkCompleted();

    void RequestRefresh();

    bool RefreshPending() const;

    bool ConsumeRefreshRequest();

    std::size_t Total() const;

    std::size_t Completed() const;

private:
    std::deque<CacheEntry> entries_;
    std::size_t total_ = 0;
    std::size_t completed_ = 0;
    bool refreshPending_ = false;
};
