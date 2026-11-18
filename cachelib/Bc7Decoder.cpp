#include "Bc7Decoder.h"

#include "AuroraCacheFormat.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244)
#endif

#define BCDEC_IMPLEMENTATION
#include <bcdec.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
    const AuroraTranscodedLevel& SelectLevel(
        const AuroraTranscodedTextureInfo& texture,
        std::uint32_t maximumDimension)
    {
        if (maximumDimension == 0)
        {
            return texture.levels.front();
        }

        const auto selected = std::find_if(
            texture.levels.begin(),
            texture.levels.end(),
            [maximumDimension](const AuroraTranscodedLevel& level)
            {
                return std::max(level.width, level.height) <=
                    maximumDimension;
            });

        return selected != texture.levels.end()
            ? *selected
            : texture.levels.back();
    }

    Bc7DecodeError MapContainerError(AuroraFormatError error)
    {
        return error == AuroraFormatError::UnsupportedVersion
            ? Bc7DecodeError::UnsupportedVersion
            : Bc7DecodeError::InvalidContainer;
    }
}

Bc7DecodeError Bc7Decoder::Decode(
    const std::vector<std::uint8_t>& encodedData,
    DecodedImage& outputImage,
    std::uint32_t maximumDimension) const
{
    outputImage = {};

    if (encodedData.empty())
    {
        return Bc7DecodeError::EmptyInput;
    }

    AuroraTranscodedTextureInfo texture;
    const AuroraFormatError parseError =
        ParseAuroraTranscodedTexture(encodedData, texture);

    if (parseError != AuroraFormatError::None)
    {
        return MapContainerError(parseError);
    }

    if (texture.kind != AuroraTranscodedKind::Bc7 ||
        texture.levels.empty())
    {
        return Bc7DecodeError::UnsupportedTextureKind;
    }

    const AuroraTranscodedLevel& level =
        SelectLevel(texture, maximumDimension);
    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(level.width) * level.height;

    if (pixelCount >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max() / 4))
    {
        return Bc7DecodeError::ImageTooLarge;
    }

    outputImage.width = level.width;
    outputImage.height = level.height;
    outputImage.rgba.resize(
        static_cast<std::size_t>(pixelCount * 4));

    const std::size_t blockColumns =
        (static_cast<std::size_t>(level.width) + 3) / 4;
    const std::size_t blockRows =
        (static_cast<std::size_t>(level.height) + 3) / 4;
    const std::size_t levelOffset =
        static_cast<std::size_t>(level.offset);
    std::array<std::uint8_t, 4 * 4 * 4> blockPixels{};

    for (std::size_t blockY = 0; blockY < blockRows; ++blockY)
    {
        for (std::size_t blockX = 0;
             blockX < blockColumns;
             ++blockX)
        {
            const std::size_t blockIndex =
                blockY * blockColumns + blockX;
            const std::uint8_t* compressedBlock =
                encodedData.data() + levelOffset + blockIndex * 16;

            bcdec_bc7(
                compressedBlock,
                blockPixels.data(),
                4 * 4);

            const std::size_t destinationX = blockX * 4;
            const std::size_t destinationY = blockY * 4;
            const std::size_t copyWidth = std::min<std::size_t>(
                4,
                static_cast<std::size_t>(level.width) - destinationX);
            const std::size_t copyHeight = std::min<std::size_t>(
                4,
                static_cast<std::size_t>(level.height) - destinationY);

            for (std::size_t row = 0; row < copyHeight; ++row)
            {
                const std::uint8_t* source =
                    blockPixels.data() + row * 4 * 4;
                std::uint8_t* destination =
                    outputImage.rgba.data() +
                    ((destinationY + row) * level.width + destinationX) * 4;

                std::copy_n(source, copyWidth * 4, destination);
            }
        }
    }

    if (!texture.hasAlphaChannel)
    {
        for (std::size_t alpha = 3;
             alpha < outputImage.rgba.size();
             alpha += 4)
        {
            outputImage.rgba[alpha] = 255;
        }
    }

    return Bc7DecodeError::None;
}

const char* Bc7Decoder::ErrorMessage(Bc7DecodeError error)
{
    switch (error)
    {
        case Bc7DecodeError::None:
            return "No error.";

        case Bc7DecodeError::EmptyInput:
            return "The Aurora BC7 cache entry is empty.";

        case Bc7DecodeError::InvalidContainer:
            return "The Aurora BC7 cache entry is malformed.";

        case Bc7DecodeError::UnsupportedVersion:
            return "The Aurora BC7 cache version is not supported.";

        case Bc7DecodeError::UnsupportedTextureKind:
            return "The Aurora cache entry does not contain BC7 pixels.";

        case Bc7DecodeError::ImageTooLarge:
            return "The Aurora BC7 image is too large to decode safely.";
    }

    return "Unknown Aurora BC7 decode error.";
}
