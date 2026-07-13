#pragma once

#include <cstddef>
#include <vector>

#include "TextureCacheDatabase.h"
#include "UUID.h"

class TextureSelection
{
public:
    static std::vector<const CacheEntry*> All(
        const TextureCacheDatabase& database);

    static std::vector<const CacheEntry*> Range(
        const TextureCacheDatabase& database,
        std::size_t start,
        std::size_t count);

    static std::vector<const CacheEntry*> ByUuid(
        const TextureCacheDatabase& database,
        const std::vector<UUID>& uuids);
};
