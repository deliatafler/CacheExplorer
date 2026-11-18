#include "PreviewDecodeWorker.h"

#include "TextureDecoder.h"

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
    const TexturePayloadRequest& payloadRequest,
    const TextureCatalogEntry& entry)
{
    const auto startedAt = std::chrono::steady_clock::now();
    PreviewDecodeResult result;
    result.requestId = requestId;
    result.entry = entry;

    TexturePayload payload;

    const TexturePayloadError payloadResult = payloadRequest.Read(payload);

    if (payloadResult != TexturePayloadError::None)
    {
        result.status = PreviewDecodeStatus::RebuildFailed;
        result.message = TexturePayloadErrorMessage(payloadResult);
        FinishDecodeTiming(result, startedAt);
        return result;
    }

    TextureDecoder decoder;
    TextureDecodeOptions options;
    options.maximumDimension = 256;
    const TextureDecodeResult decodeResult =
        decoder.Decode(payload, result.image, options);

    if (!decodeResult.Succeeded())
    {
        result.status = PreviewDecodeStatus::Incomplete;
        result.message = decodeResult.message;
        FinishDecodeTiming(result, startedAt);
        return result;
    }

    result.status = PreviewDecodeStatus::Decoded;
    result.succeeded = true;
    FinishDecodeTiming(result, startedAt);
    return result;
}
