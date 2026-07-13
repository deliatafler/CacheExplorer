#pragma once

#include "TextureCacheDatabase.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

class TextureExportState
{
public:
    bool Load(
        const std::filesystem::path& stateFile,
        std::string& errorMessage);

    bool Save(
        const std::filesystem::path& stateFile,
        std::string& errorMessage) const;

    bool IsKnownIncomplete(const CacheEntry& entry) const;

    void MarkIncomplete(
        const CacheEntry& entry,
        const std::string& message);

    void MarkSucceeded(const CacheEntry& entry);

    std::size_t EntryCount() const
    {
        return mRecords.size();
    }

private:
    struct Record
    {
        std::size_t cacheIndex = 0;
        std::int32_t imageSize = 0;
        std::int32_t bodySize = 0;
        std::string status;
        std::string message;
    };

    static std::string KeyFor(const CacheEntry& entry);

    std::unordered_map<std::string, Record> mRecords;
};
