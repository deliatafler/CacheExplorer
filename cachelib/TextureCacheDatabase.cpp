#include "TextureCacheDatabase.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <utility>

namespace
{
    constexpr float MinSupportedVersion = 1.0f;
    constexpr float MaxSupportedVersion = 2.0f;

    constexpr std::uint32_t MaximumReasonableEntryCount =
        100000000;

    std::string ReadNullTerminatedString(
        const char* data,
        std::size_t dataSize)
    {
        const char* end =
            std::find(data, data + dataSize, '\0');

        return std::string(data, end);
    }
}

CacheError TextureCacheDatabase::Open(
    const std::filesystem::path& cacheDirectory)
{
    Close();

    const std::filesystem::path filename =
        cacheDirectory / "texture.entries";

    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        return CacheError::FileNotFound;
    }

    if (!ReadHeader(file))
    {
        Close();
        return CacheError::InvalidHeader;
    }

    if (mHeader.version < MinSupportedVersion ||
        mHeader.version >= MaxSupportedVersion)
    {
        Close();
        return CacheError::UnsupportedVersion;
    }

    if (!ReadEntries(file))
    {
        Close();
        return CacheError::ReadError;
    }

    mCacheDirectory = cacheDirectory;
    mOpen = true;

    return CacheError::None;
}

void TextureCacheDatabase::Close()
{
    mOpen = false;

    mCacheDirectory.clear();

    mHeader = {};

    mEntries.clear();
}

bool TextureCacheDatabase::ReadHeader(
    std::istream& stream)
{
    EntriesInfo diskHeader{};

    stream.read(
        reinterpret_cast<char*>(&diskHeader),
        static_cast<std::streamsize>(
            sizeof(diskHeader)));

    if (!stream)
    {
        return false;
    }

    if (diskHeader.entryCount >
        MaximumReasonableEntryCount)
    {
        return false;
    }

    mHeader.version = diskHeader.version;
    mHeader.addressSize = diskHeader.addressSize;
    mHeader.encoderVersion =
        ReadNullTerminatedString(
            diskHeader.encoderVersion,
            sizeof(diskHeader.encoderVersion));
    mHeader.entryCount = diskHeader.entryCount;

    return true;
}

bool TextureCacheDatabase::ReadEntries(
    std::istream& stream)
{
    mEntries.clear();
    mEntries.reserve(mHeader.entryCount);

    for (std::uint32_t i = 0;
         i < mHeader.entryCount;
         ++i)
    {
        Entry diskEntry{};

        stream.read(
            reinterpret_cast<char*>(&diskEntry),
            static_cast<std::streamsize>(
                sizeof(diskEntry)));

        if (!stream)
        {
            return false;
        }

        /*
         * Firestorm considers an entry usable only when the total
         * image size is greater than the separately stored body size.
         *
         * Free/deleted slots normally contain:
         *
         *     imageSize = -1
         *     bodySize  = 0
         *
         * We must still use the raw record number as cacheIndex,
         * because texture.cache has one 600-byte block for every
         * record, including free records.
         */
        if (diskEntry.imageSize <= diskEntry.bodySize)
        {
            continue;
        }

        CacheEntry entry;

        entry.uuid = diskEntry.uuid;
        entry.cacheIndex = i;
        entry.imageSize = diskEntry.imageSize;
        entry.bodySize = diskEntry.bodySize;
        entry.timestamp =
            static_cast<std::time_t>(
                diskEntry.timestamp);

        mEntries.push_back(std::move(entry));
    }

    return true;
}

const CacheEntry* TextureCacheDatabase::Find(
    const UUID& uuid) const
{
    for (const CacheEntry& entry : mEntries)
    {
        if (entry.uuid == uuid)
        {
            return &entry;
        }
    }

    return nullptr;
}

std::uint64_t CachedTextureByteCount(const CacheEntry& entry)
{
    if (entry.imageSize <= 0 || entry.bodySize < 0)
    {
        return 0;
    }

    const std::uint64_t imageSize =
        static_cast<std::uint64_t>(entry.imageSize);
    const std::uint64_t bodySize =
        static_cast<std::uint64_t>(entry.bodySize);
    const std::uint64_t headerBytes =
        std::min<std::uint64_t>(
            imageSize,
            TextureCacheHeaderBlockSize);

    return headerBytes + bodySize;
}

bool HasCompleteCachedTexture(const CacheEntry& entry)
{
    if (entry.imageSize <= 0 || entry.bodySize < 0)
    {
        return false;
    }

    return CachedTextureByteCount(entry) >=
        static_cast<std::uint64_t>(entry.imageSize);
}
