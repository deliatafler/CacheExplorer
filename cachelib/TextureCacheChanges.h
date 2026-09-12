#pragma once

#include "TextureCacheDatabase.h"

#include <cstdint>
#include <vector>

std::vector<std::uint32_t> FindChangedCacheIndices(
    const std::vector<CacheEntry>& previousEntries,
    const std::vector<CacheEntry>& currentEntries);
