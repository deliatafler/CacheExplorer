#pragma once

#include "J2CDecoder.h"
#include "TextureCacheDatabase.h"

#include <cstdint>
#include <filesystem>
#include <string>

struct PreviewDecodeResult
{
    std::uint64_t requestId = 0;
    CacheEntry entry;
    bool succeeded = false;
    std::string message;
    DecodedImage image;
};

PreviewDecodeResult DecodePreview(
    std::uint64_t requestId,
    const std::filesystem::path& cacheDirectory,
    const CacheEntry& entry);
