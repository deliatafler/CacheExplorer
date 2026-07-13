#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "TextureCacheDatabase.h"

enum class RebuildError
{
    None,
    DatabaseNotOpen,
    InvalidEntry,
    HeaderFileNotFound,
    HeaderReadError,
    BodyFileNotFound,
    BodyReadError,
    OutputOpenError,
    OutputWriteError
};

class TextureRebuilder
{
public:
    static constexpr std::uint32_t HeaderBlockSize = 600;

    /// Reconstructs the cached JPEG2000 byte stream and writes it to disk.
    ///
    /// The output normally uses the .j2c extension, although this method does
    /// not enforce a particular extension.
    RebuildError ExportJ2C(
        const TextureCacheDatabase& database,
        const CacheEntry& entry,
        const std::filesystem::path& outputFile) const;

    /// Reconstructs the cached JPEG2000 byte stream in memory.
    RebuildError Rebuild(
        const TextureCacheDatabase& database,
        const CacheEntry& entry,
        std::vector<std::uint8_t>& outputData) const;

    /// Returns the expected body-file path for an entry.
    static std::filesystem::path BodyFilePath(
        const TextureCacheDatabase& database,
        const CacheEntry& entry);

    /// Returns a human-readable description of a rebuild error.
    static const char* ErrorMessage(RebuildError error);
};
