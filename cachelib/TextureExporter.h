#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "TextureCacheDatabase.h"

enum class TexturePngExportStatus
{
    Exported,
    SkippedExisting,
    Incomplete,
    RebuildFailed,
    WriteFailed
};

struct TexturePngExportResult
{
    TexturePngExportStatus status = TexturePngExportStatus::Incomplete;
    std::filesystem::path outputFile;
    std::string message;
    std::size_t cachedBytes = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool Succeeded() const
    {
        return status == TexturePngExportStatus::Exported ||
            status == TexturePngExportStatus::SkippedExisting;
    }
};

struct BulkExportResults
{
    std::size_t totalEntries = 0;
    std::size_t exported = 0;
    std::size_t skippedExisting = 0;
    std::size_t incompleteTextures = 0;
    std::size_t rebuildFailures = 0;
    std::size_t writeFailures = 0;

    std::vector<std::string> messages;

    std::size_t ErrorCount() const
    {
        return rebuildFailures + writeFailures;
    }
};

struct TextureExportProgress
{
    std::size_t processed = 0;
    std::size_t total = 0;
    std::size_t exported = 0;
    std::size_t skippedExisting = 0;
    std::size_t incompleteTextures = 0;
    std::size_t errors = 0;
};

struct TextureExportOptions
{
    bool overwriteExisting = false;
    bool verboseDecoderErrors = false;

    std::size_t maximumStoredMessages = 50;
};

using TextureExportProgressCallback =
    std::function<void(const TextureExportProgress&)>;

class TextureExporter
{
public:
    TexturePngExportResult ExportPngEntry(
        const TextureCacheDatabase& database,
        const CacheEntry& entry,
        const std::filesystem::path& outputFile,
        const TextureExportOptions& options = {}) const;

    BulkExportResults ExportPngEntries(
        const TextureCacheDatabase& database,
        const std::vector<const CacheEntry*>& entries,
        const std::filesystem::path& outputDirectory,
        const TextureExportOptions& options = {},
        TextureExportProgressCallback progressCallback = {}) const;

    static const char* StatusMessage(TexturePngExportStatus status);
};
