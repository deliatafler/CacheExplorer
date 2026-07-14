#pragma once

#include "J2CDecoder.h"
#include "TextureCacheDatabase.h"

#include <cstdint>
#include <filesystem>
#include <string>

enum class PreviewDecodeStatus
{
    Decoded,
    Incomplete,
    RebuildFailed
};

struct PreviewDecodeResult
{
    std::uint64_t requestId = 0;
    CacheEntry entry;
    bool succeeded = false;
    PreviewDecodeStatus status = PreviewDecodeStatus::Incomplete;
    std::string message;
    DecodedImage image;
};

PreviewDecodeResult DecodePreview(
    std::uint64_t requestId,
    const std::filesystem::path& cacheDirectory,
    const CacheEntry& entry);
