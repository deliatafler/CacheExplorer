#include "TextureSelection.h"

#include <algorithm>

std::vector<const CacheEntry*> TextureSelection::All(
    const TextureCacheDatabase& database)
{
    std::vector<const CacheEntry*> selection;
    selection.reserve(database.Entries().size());

    for (const CacheEntry& entry : database.Entries())
    {
        selection.push_back(&entry);
    }

    return selection;
}

std::vector<const CacheEntry*> TextureSelection::Range(
    const TextureCacheDatabase& database,
    std::size_t start,
    std::size_t count)
{
    const auto& entries = database.Entries();

    if (start >= entries.size() || count == 0)
    {
        return {};
    }

    const std::size_t end =
        std::min(entries.size(), start + count);

    std::vector<const CacheEntry*> selection;
    selection.reserve(end - start);

    for (std::size_t index = start; index < end; ++index)
    {
        selection.push_back(&entries[index]);
    }

    return selection;
}

std::vector<const CacheEntry*> TextureSelection::ByUuid(
    const TextureCacheDatabase& database,
    const std::vector<UUID>& uuids)
{
    std::vector<const CacheEntry*> selection;
    selection.reserve(uuids.size());

    for (const UUID& uuid : uuids)
    {
        const CacheEntry* entry = database.Find(uuid);

        if (entry != nullptr)
        {
            selection.push_back(entry);
        }
    }

    return selection;
}
