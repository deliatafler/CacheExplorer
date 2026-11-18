#pragma once

#include "AuroraCacheFormat.h"
#include "TextureCacheSource.h"

#include <cstddef>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class AuroraTextureFileKind
{
    CompleteJpeg2000,
    PartialJpeg2000,
    TranscodedBc7
};

struct AuroraTextureStoreProbe
{
    std::filesystem::path cacheRoot;
    std::filesystem::path textureStore;
};

struct AuroraTextureFile
{
    AuroraTextureFileKind kind =
        AuroraTextureFileKind::CompleteJpeg2000;
    std::filesystem::path path;
    std::uint64_t sealedBytes = 0;
    std::optional<std::uint64_t> plaintextBytes;
    std::time_t modifiedTime = 0;
};

struct AuroraTextureRecord
{
    std::string sourceKey;
    std::optional<AuroraTextureFile> completeJpeg2000;
    std::optional<AuroraTextureFile> partialJpeg2000;
    std::optional<AuroraTextureFile> transcodedBc7;
};

struct AuroraTextureStoreScan
{
    AuroraTextureStoreProbe probe;
    std::vector<AuroraTextureRecord> records;

    std::size_t cacheMissCount = 0;
    std::size_t ignoredFileCount = 0;
};

TextureCacheOpenError ProbeAuroraTextureStore(
    const std::filesystem::path& selectedDirectory,
    AuroraTextureStoreProbe& output);

TextureCacheOpenError EnumerateAuroraTextureStore(
    const std::filesystem::path& selectedDirectory,
    AuroraTextureStoreScan& output);
