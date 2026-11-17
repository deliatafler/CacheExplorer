#include "AuroraCacheFormat.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <utility>

namespace
{
    constexpr std::array<std::uint8_t, 4> SealedMagic = {
        'A', 'U', 'S', 'L'};
    constexpr std::array<std::uint8_t, 4> TranscodedMagic = {
        'A', 'U', 'T', 'X'};

    constexpr std::size_t SealedPlaintextSizeOffset = 20;
    constexpr std::size_t TranscodedHeaderSize = 32;
    constexpr std::size_t TranscodedLevelSize = 8;
    constexpr std::uint8_t MinimumTranscodedVersion = 3;
    constexpr std::uint8_t CurrentTranscodedVersion = 6;
    constexpr std::uint8_t ExactMaskVersion = 5;
    constexpr std::size_t MaximumTranscodedLevels = 16;
    constexpr std::uint32_t MaximumTranscodedDimension = 16384;

    std::uint32_t ReadLittleEndian32(
        const std::vector<std::uint8_t>& bytes,
        std::size_t offset)
    {
        return
            static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    std::uint64_t ReadLittleEndian64(
        const std::array<std::uint8_t, AuroraSealedHeaderSize>& bytes,
        std::size_t offset)
    {
        std::uint64_t value = 0;

        for (std::size_t i = 0; i < 8; ++i)
        {
            value |=
                static_cast<std::uint64_t>(bytes[offset + i]) <<
                (i * 8);
        }

        return value;
    }

    bool HasMagic(
        const std::array<std::uint8_t, AuroraSealedHeaderSize>& bytes,
        const std::array<std::uint8_t, 4>& magic)
    {
        return std::equal(
            magic.begin(),
            magic.end(),
            bytes.begin());
    }

    bool HasMagic(
        const std::vector<std::uint8_t>& bytes,
        const std::array<std::uint8_t, 4>& magic)
    {
        return bytes.size() >= magic.size() &&
            std::equal(
                magic.begin(),
                magic.end(),
                bytes.begin());
    }

    bool IsFutureSealedMagic(
        const std::array<std::uint8_t, AuroraSealedHeaderSize>& bytes)
    {
        return bytes[0] == 'A' &&
            bytes[1] == 'U' &&
            bytes[2] == 'S';
    }

    bool AddWithoutOverflow(
        std::uint64_t left,
        std::uint64_t right,
        std::uint64_t& output)
    {
        if (right > std::numeric_limits<std::uint64_t>::max() - left)
        {
            return false;
        }

        output = left + right;
        return true;
    }

    std::uint64_t Bc7LevelBytes(
        std::uint32_t width,
        std::uint32_t height)
    {
        const std::uint64_t blockColumns =
            (static_cast<std::uint64_t>(width) + 3) / 4;
        const std::uint64_t blockRows =
            (static_cast<std::uint64_t>(height) + 3) / 4;

        return blockColumns * blockRows * 16;
    }

    std::uint64_t ExpectedMaskBytes(
        std::uint32_t width,
        std::uint32_t height)
    {
        const std::uint64_t coarsePixels =
            ((static_cast<std::uint64_t>(width) + 1) / 2) *
            ((static_cast<std::uint64_t>(height) + 1) / 2);
        const std::uint64_t exactPixels =
            static_cast<std::uint64_t>(width) * height;

        return ((coarsePixels + 7) / 8) +
            ((exactPixels + 7) / 8);
    }

    bool IsValidDimension(std::uint32_t value)
    {
        return value > 0 && value <= MaximumTranscodedDimension;
    }
}

const char* AuroraFormatErrorMessage(AuroraFormatError error)
{
    switch (error)
    {
        case AuroraFormatError::None:
            return "No error.";

        case AuroraFormatError::FileNotFound:
            return "The Aurora cache file is no longer available.";

        case AuroraFormatError::ReadError:
            return "The Aurora cache file could not be read.";

        case AuroraFormatError::InvalidFormat:
            return "The file is not a supported Aurora cache format.";

        case AuroraFormatError::UnsupportedVersion:
            return "The Aurora cache format version is not supported.";

        case AuroraFormatError::SizeMismatch:
            return "The Aurora cache file size does not match its header.";

        case AuroraFormatError::InvalidMetadata:
            return "The Aurora texture metadata is invalid.";
    }

    return "Unknown Aurora cache format error.";
}

AuroraFormatError InspectAuroraSealedFile(
    const std::filesystem::path& filename,
    AuroraSealedFileInfo& output)
{
    output = {};

    std::ifstream file(
        filename,
        std::ios::binary | std::ios::ate);

    if (!file)
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(filename, error);

        if (!error && !exists)
        {
            return AuroraFormatError::FileNotFound;
        }

        return AuroraFormatError::ReadError;
    }

    const std::streampos end = file.tellg();
    if (end < 0)
    {
        return AuroraFormatError::ReadError;
    }

    const std::uint64_t sealedBytes =
        static_cast<std::uint64_t>(end);

    if (sealedBytes < AuroraSealedHeaderSize)
    {
        return AuroraFormatError::InvalidFormat;
    }

    file.seekg(0, std::ios::beg);

    std::array<std::uint8_t, AuroraSealedHeaderSize> header{};
    file.read(
        reinterpret_cast<char*>(header.data()),
        static_cast<std::streamsize>(header.size()));

    if (!file)
    {
        return AuroraFormatError::ReadError;
    }

    if (!HasMagic(header, SealedMagic))
    {
        return IsFutureSealedMagic(header)
            ? AuroraFormatError::UnsupportedVersion
            : AuroraFormatError::InvalidFormat;
    }

    const std::uint64_t plaintextBytes =
        ReadLittleEndian64(header, SealedPlaintextSizeOffset);
    const std::uint64_t chunkCount =
        std::max<std::uint64_t>(
            1,
            (plaintextBytes / AuroraSealedChunkSize) +
                (plaintextBytes % AuroraSealedChunkSize != 0 ? 1 : 0));

    if (chunkCount >
        std::numeric_limits<std::uint64_t>::max() /
            AuroraSealedTagSize)
    {
        return AuroraFormatError::SizeMismatch;
    }

    std::uint64_t expectedSize = AuroraSealedHeaderSize;
    if (!AddWithoutOverflow(
            expectedSize,
            plaintextBytes,
            expectedSize) ||
        !AddWithoutOverflow(
            expectedSize,
            chunkCount * AuroraSealedTagSize,
            expectedSize) ||
        expectedSize != sealedBytes)
    {
        return AuroraFormatError::SizeMismatch;
    }

    output.plaintextBytes = plaintextBytes;
    output.sealedBytes = sealedBytes;
    output.chunkCount = chunkCount;
    return AuroraFormatError::None;
}

bool IsAuroraTextureKey(std::string_view value)
{
    if (value.size() != 32)
    {
        return false;
    }

    return std::all_of(
        value.begin(),
        value.end(),
        [](char value)
        {
            return
                (value >= '0' && value <= '9') ||
                (value >= 'a' && value <= 'f');
        });
}

AuroraFormatError ParseAuroraTranscodedTexture(
    const std::vector<std::uint8_t>& bytes,
    AuroraTranscodedTextureInfo& output)
{
    output = {};
    AuroraTranscodedTextureInfo parsed;

    if (!HasMagic(bytes, TranscodedMagic))
    {
        return AuroraFormatError::InvalidFormat;
    }

    if (bytes.size() < TranscodedHeaderSize)
    {
        return AuroraFormatError::InvalidFormat;
    }

    const std::uint8_t version = bytes[4];
    if (version < MinimumTranscodedVersion ||
        version > CurrentTranscodedVersion)
    {
        return AuroraFormatError::UnsupportedVersion;
    }

    AuroraTranscodedAlpha alpha = AuroraTranscodedAlpha::Opaque;
    switch (bytes[7])
    {
        case 0:
            alpha = AuroraTranscodedAlpha::Opaque;
            break;

        case 1:
        case 4:
            alpha = AuroraTranscodedAlpha::Mask;
            break;

        case 2:
            alpha = AuroraTranscodedAlpha::Blend;
            break;

        default:
            return AuroraFormatError::InvalidMetadata;
    }

    const std::uint32_t imageWidth =
        ReadLittleEndian32(bytes, 12);
    const std::uint32_t imageHeight =
        ReadLittleEndian32(bytes, 16);

    if (!IsValidDimension(imageWidth) ||
        !IsValidDimension(imageHeight) ||
        bytes[6] > 5)
    {
        return AuroraFormatError::InvalidMetadata;
    }

    parsed.version = version;
    parsed.finestDiscard = bytes[6];
    parsed.alpha = alpha;
    parsed.hasAlphaChannel = bytes[8] != 0;
    parsed.imageWidth = imageWidth;
    parsed.imageHeight = imageHeight;
    parsed.jpeg2000Components = bytes[9];
    parsed.jpeg2000Levels = bytes[10];

    if (bytes[5] == 0)
    {
        parsed.kind = AuroraTranscodedKind::KeepRgba;
        output = std::move(parsed);
        return AuroraFormatError::None;
    }

    if (bytes[5] != 1)
    {
        return AuroraFormatError::InvalidMetadata;
    }

    if (version < CurrentTranscodedVersion &&
        alpha == AuroraTranscodedAlpha::Opaque &&
        parsed.hasAlphaChannel)
    {
        return AuroraFormatError::InvalidMetadata;
    }

    const std::size_t levelCount = bytes[11];
    if (levelCount == 0 || levelCount > MaximumTranscodedLevels)
    {
        return AuroraFormatError::InvalidMetadata;
    }

    const std::size_t levelTableBytes =
        levelCount * TranscodedLevelSize;
    if (bytes.size() < TranscodedHeaderSize + levelTableBytes)
    {
        return AuroraFormatError::SizeMismatch;
    }

    const std::uint32_t maskWidth =
        ReadLittleEndian32(bytes, 20);
    const std::uint32_t maskHeight =
        ReadLittleEndian32(bytes, 24);
    const std::uint32_t maskBytes =
        ReadLittleEndian32(bytes, 28);

    if (maskBytes > 0)
    {
        if (version < ExactMaskVersion ||
            !IsValidDimension(maskWidth) ||
            !IsValidDimension(maskHeight) ||
            ExpectedMaskBytes(maskWidth, maskHeight) != maskBytes)
        {
            return AuroraFormatError::InvalidMetadata;
        }
    }

    parsed.kind = AuroraTranscodedKind::Bc7;
    parsed.maskWidth = maskWidth;
    parsed.maskHeight = maskHeight;
    parsed.maskBytes = maskBytes;
    parsed.levels.reserve(levelCount);

    for (std::size_t i = 0; i < levelCount; ++i)
    {
        const std::size_t offset =
            TranscodedHeaderSize + i * TranscodedLevelSize;
        const std::uint32_t width =
            ReadLittleEndian32(bytes, offset);
        const std::uint32_t height =
            ReadLittleEndian32(bytes, offset + 4);

        if (!IsValidDimension(width) ||
            !IsValidDimension(height))
        {
            return AuroraFormatError::InvalidMetadata;
        }

        parsed.levels.push_back({
            width,
            height,
            0,
            Bc7LevelBytes(width, height)});
    }

    const std::uint32_t firstWidth = parsed.levels.front().width;
    const std::uint32_t firstHeight = parsed.levels.front().height;

    if (firstWidth % 4 != 0 || firstHeight % 4 != 0)
    {
        return AuroraFormatError::InvalidMetadata;
    }

    for (std::size_t i = 0; i < parsed.levels.size(); ++i)
    {
        const std::uint32_t expectedWidth =
            std::max<std::uint32_t>(
                1,
                firstWidth >> i);
        const std::uint32_t expectedHeight =
            std::max<std::uint32_t>(
                1,
                firstHeight >> i);

        if (parsed.levels[i].width != expectedWidth ||
            parsed.levels[i].height != expectedHeight)
        {
            return AuroraFormatError::InvalidMetadata;
        }
    }

    std::uint64_t dataOffset =
        TranscodedHeaderSize + levelTableBytes;
    if (!AddWithoutOverflow(dataOffset, maskBytes, dataOffset))
    {
        return AuroraFormatError::SizeMismatch;
    }

    std::uint64_t expectedSize = dataOffset;
    for (AuroraTranscodedLevel& level : parsed.levels)
    {
        level.offset = expectedSize;

        if (!AddWithoutOverflow(
                expectedSize,
                level.byteCount,
                expectedSize))
        {
            return AuroraFormatError::SizeMismatch;
        }
    }

    if (expectedSize != bytes.size())
    {
        return AuroraFormatError::SizeMismatch;
    }

    output = std::move(parsed);
    return AuroraFormatError::None;
}
