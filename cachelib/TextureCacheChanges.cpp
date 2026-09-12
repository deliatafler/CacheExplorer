#include "TextureCacheChanges.h"

#include <array>
#include <cstddef>
#include <unordered_map>

namespace
{
    bool HasSameCacheMetadata(
        const CacheEntry& left,
        const CacheEntry& right)
    {
        return left.cacheIndex == right.cacheIndex &&
            left.imageSize == right.imageSize &&
            left.bodySize == right.bodySize &&
            left.timestamp == right.timestamp;
    }

    struct UuidBytesHash
    {
        std::size_t operator()(
            const std::array<std::uint8_t, UUID::SIZE>& bytes) const noexcept
        {
            std::size_t hash = 0;

            for (const std::uint8_t byte : bytes)
            {
                hash = (hash * 131u) ^ byte;
            }

            return hash;
        }
    };
}

std::vector<std::uint32_t> FindChangedCacheIndices(
    const std::vector<CacheEntry>& previousEntries,
    const std::vector<CacheEntry>& currentEntries)
{
    std::unordered_map<
        std::array<std::uint8_t, UUID::SIZE>,
        const CacheEntry*,
        UuidBytesHash> previousByUuid;
    previousByUuid.reserve(previousEntries.size());

    for (const CacheEntry& entry : previousEntries)
    {
        previousByUuid[entry.uuid.Bytes()] = &entry;
    }

    std::vector<std::uint32_t> changedIndices;
    changedIndices.reserve(currentEntries.size());

    for (const CacheEntry& entry : currentEntries)
    {
        const auto previous = previousByUuid.find(entry.uuid.Bytes());

        if (previous == previousByUuid.end() ||
            !HasSameCacheMetadata(*previous->second, entry))
        {
            changedIndices.push_back(entry.cacheIndex);
        }
    }

    return changedIndices;
}
