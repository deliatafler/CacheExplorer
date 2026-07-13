#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class DecodeError
{
    None,
    EmptyInput,
    StreamCreationFailed,
    CodecCreationFailed,
    HeaderReadFailed,
    DecodeFailed,
    UnsupportedComponentCount,
    UnsupportedComponentLayout,
    InvalidDimensions,
    ImageTooLarge,
    MissingComponentData
};

struct DecodedImage
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    // Always stored as interleaved, 8-bit RGBA.
    std::vector<std::uint8_t> rgba;
};

class J2CDecoder
{
public:
    DecodeError Decode(
        const std::vector<std::uint8_t>& encodedData,
        DecodedImage& outputImage) const;

    static const char* ErrorMessage(DecodeError error);
};
