#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

inline constexpr std::size_t AuroraSealedHeaderSize = 28;
inline constexpr std::uint64_t AuroraSealedChunkSize = 64u * 1024u;
inline constexpr std::uint64_t AuroraSealedTagSize = 16;

enum class AuroraFormatError
{
    None,
    FileNotFound,
    ReadError,
    InvalidFormat,
    UnsupportedVersion,
    SizeMismatch,
    InvalidMetadata,
    AuthenticationFailed,
    CryptoError
};

const char* AuroraFormatErrorMessage(AuroraFormatError error);

struct AuroraSealedFileInfo
{
    std::uint64_t plaintextBytes = 0;
    std::uint64_t sealedBytes = 0;
    std::uint64_t chunkCount = 0;
};

AuroraFormatError InspectAuroraSealedFile(
    const std::filesystem::path& filename,
    AuroraSealedFileInfo& output);

AuroraFormatError InspectAuroraSealedBytes(
    const std::vector<std::uint8_t>& bytes,
    AuroraSealedFileInfo& output);

bool IsAuroraTextureKey(std::string_view value);

enum class AuroraTranscodedKind
{
    KeepRgba,
    Bc7
};

enum class AuroraTranscodedAlpha
{
    Opaque,
    Mask,
    Blend
};

struct AuroraTranscodedLevel
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t offset = 0;
    std::uint64_t byteCount = 0;
};

struct AuroraTranscodedTextureInfo
{
    std::uint8_t version = 0;
    AuroraTranscodedKind kind = AuroraTranscodedKind::KeepRgba;
    std::uint8_t finestDiscard = 0;
    AuroraTranscodedAlpha alpha = AuroraTranscodedAlpha::Opaque;
    bool hasAlphaChannel = false;

    std::uint32_t imageWidth = 0;
    std::uint32_t imageHeight = 0;
    std::uint8_t jpeg2000Components = 0;
    std::uint8_t jpeg2000Levels = 0;

    std::uint32_t maskWidth = 0;
    std::uint32_t maskHeight = 0;
    std::uint32_t maskBytes = 0;

    std::vector<AuroraTranscodedLevel> levels;
};

AuroraFormatError ParseAuroraTranscodedTexture(
    const std::vector<std::uint8_t>& bytes,
    AuroraTranscodedTextureInfo& output);
