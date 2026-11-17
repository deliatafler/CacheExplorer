#pragma once

#include "J2CDecoder.h"
#include "TextureCacheSource.h"

#include <cstdint>
#include <chrono>
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
    TextureCatalogEntry entry;
    bool succeeded = false;
    PreviewDecodeStatus status = PreviewDecodeStatus::Incomplete;
    std::string message;
    DecodedImage image;
    std::chrono::milliseconds decodeDuration{0};
};

PreviewDecodeResult DecodePreview(
    std::uint64_t requestId,
    const TexturePayloadRequest& payloadRequest,
    const TextureCatalogEntry& entry);
