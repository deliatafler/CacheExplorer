#include "PreviewDecodeWorker.h"

#include "TextureRebuilder.h"

#include <cstdint>
#include <chrono>
#include <vector>

namespace
{
    void FinishDecodeTiming(
        PreviewDecodeResult& result,
        std::chrono::steady_clock::time_point startedAt)
    {
        result.decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt);
    }
}

PreviewDecodeResult DecodePreview(
    std::uint64_t requestId,
    const std::filesystem::path& cacheDirectory,
    const CacheEntry& entry)
{
    const auto startedAt = std::chrono::steady_clock::now();
    PreviewDecodeResult result;
    result.requestId = requestId;
    result.entry = entry;

    TextureRebuilder rebuilder;
    std::vector<std::uint8_t> encodedData;

    const RebuildError rebuildResult =
        rebuilder.Rebuild(cacheDirectory, entry, encodedData);

    if (rebuildResult != RebuildError::None)
    {
        result.status = PreviewDecodeStatus::RebuildFailed;
        result.message = TextureRebuilder::ErrorMessage(rebuildResult);
        FinishDecodeTiming(result, startedAt);
        return result;
    }

    J2CDecoder decoder;
    const DecodeError decodeResult =
        decoder.Decode(encodedData, result.image, false);

    if (decodeResult != DecodeError::None)
    {
        result.status = PreviewDecodeStatus::Incomplete;
        result.message = J2CDecoder::ErrorMessage(decodeResult);
        FinishDecodeTiming(result, startedAt);
        return result;
    }

    result.status = PreviewDecodeStatus::Decoded;
    result.succeeded = true;
    FinishDecodeTiming(result, startedAt);
    return result;
}
