#pragma once

#include "J2CDecoder.h"

#include <cstdint>
#include <vector>

enum class Bc7DecodeError
{
    None,
    EmptyInput,
    InvalidContainer,
    UnsupportedVersion,
    UnsupportedTextureKind,
    ImageTooLarge
};

class Bc7Decoder
{
public:
    // maximumDimension selects the first cached mip no larger than the
    // requested side. Zero decodes the finest cached mip.
    Bc7DecodeError Decode(
        const std::vector<std::uint8_t>& encodedData,
        DecodedImage& outputImage,
        std::uint32_t maximumDimension = 0) const;

    static const char* ErrorMessage(Bc7DecodeError error);
};
