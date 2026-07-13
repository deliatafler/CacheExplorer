#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "TextureCacheDatabase.h"

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

struct TextureExportOptions
{
    bool overwriteExisting = false;
    bool verboseDecoderErrors = false;

    std::size_t maximumStoredMessages = 50;
};

class TextureExporter
{
public:
    BulkExportResults ExportPngEntries(
        const TextureCacheDatabase& database,
        const std::vector<const CacheEntry*>& entries,
        const std::filesystem::path& outputDirectory,
        const TextureExportOptions& options = {}) const;
};
