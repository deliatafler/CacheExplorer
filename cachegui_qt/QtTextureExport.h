#pragma once

#include "TextureCacheDatabase.h"
#include "TextureExporter.h"

#include <filesystem>

#include <QString>

QString DefaultPngExportFileName(const CacheEntry& entry);

TexturePngExportResult ExportTexturePng(
    const TextureCacheDatabase& database,
    const CacheEntry& entry,
    const std::filesystem::path& outputFile,
    bool overwriteExisting);

QString PngExportStatusText(
    const TexturePngExportResult& result,
    const QString& outputFile);
