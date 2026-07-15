#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

#include "Structures.h"

enum class CacheError
{
    None,
    FileNotFound,
    ReadError,
    InvalidHeader,
    UnsupportedVersion
};

struct CacheHeader
{
    float version = 0.0f;
    std::uint32_t addressSize = 0;
    std::string encoderVersion;
    std::uint32_t entryCount = 0;
};

struct CacheEntry
{
    UUID uuid;

    // Index of this entry in texture.entries. This determines its
    // corresponding 600-byte block within texture.cache.
    std::uint32_t cacheIndex = 0;

    std::int32_t imageSize = 0;
    std::int32_t bodySize = 0;

    std::time_t timestamp = 0;
};

inline constexpr std::uint32_t TextureCacheHeaderBlockSize = 600;

std::uint64_t CachedTextureByteCount(const CacheEntry& entry);
bool HasCompleteCachedTexture(const CacheEntry& entry);

class TextureCacheDatabase
{
public:
    CacheError Open(
        const std::filesystem::path& cacheDirectory);

    void Close();

    bool IsOpen() const
    {
        return mOpen;
    }

    const CacheHeader& Header() const
    {
        return mHeader;
    }

    const std::vector<CacheEntry>& Entries() const
    {
        return mEntries;
    }

    const std::filesystem::path& CacheDirectory() const
    {
        return mCacheDirectory;
    }

    const CacheEntry* Find(const UUID& uuid) const;

private:
    bool ReadHeader(std::istream& stream);
    bool ReadEntries(std::istream& stream);

    bool mOpen = false;

    std::filesystem::path mCacheDirectory;

    CacheHeader mHeader{};

    std::vector<CacheEntry> mEntries;
};
