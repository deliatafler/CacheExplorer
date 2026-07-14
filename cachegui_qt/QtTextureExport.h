#pragma once

#include "TextureCacheDatabase.h"
#include "TextureExporter.h"

#include <filesystem>

TexturePngExportResult ExportTexturePng(
    const TextureCacheDatabase& database,
    const CacheEntry& entry,
    const std::filesystem::path& outputFile,
    bool overwriteExisting);
