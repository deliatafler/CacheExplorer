#pragma once

#include "J2CDecoder.h"
#include "TextureCacheSource.h"

#include <cstdint>
#include <string>

enum class TextureDecodeError
{
    None,
    Incomplete,
    UnsupportedFormat
};

struct TextureDecodeOptions
{
    bool verboseJpeg2000Errors = false;

    // Applied to mipmapped payloads such as Aurora BC7. Zero requests the
    // finest cached level.
    std::uint32_t maximumDimension = 0;
};

struct TextureDecodeResult
{
    TextureDecodeError error = TextureDecodeError::None;
    std::string message;

    bool Succeeded() const
    {
        return error == TextureDecodeError::None;
    }
};

class TextureDecoder
{
public:
    TextureDecodeResult Decode(
        const TexturePayload& payload,
        DecodedImage& outputImage,
        const TextureDecodeOptions& options = {}) const;
};
