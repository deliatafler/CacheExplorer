#include "PreviewDecodeWorker.h"

#include "TextureRebuilder.h"

#include <cstdint>
#include <vector>

PreviewDecodeResult DecodePreview(
    std::uint64_t requestId,
    const std::filesystem::path& cacheDirectory,
    const CacheEntry& entry)
{
    PreviewDecodeResult result;
    result.requestId = requestId;
    result.entry = entry;

    TextureRebuilder rebuilder;
    std::vector<std::uint8_t> encodedData;

    const RebuildError rebuildResult =
        rebuilder.Rebuild(cacheDirectory, entry, encodedData);

    if (rebuildResult != RebuildError::None)
    {
        result.message = TextureRebuilder::ErrorMessage(rebuildResult);
        return result;
    }

    J2CDecoder decoder;
    const DecodeError decodeResult =
        decoder.Decode(encodedData, result.image, false);

    if (decodeResult != DecodeError::None)
    {
        result.message = J2CDecoder::ErrorMessage(decodeResult);
        return result;
    }

    result.succeeded = true;
    return result;
}
