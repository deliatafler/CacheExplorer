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

BulkExportResults ExportTexturePngs(
    const TextureCacheDatabase& database,
    const std::vector<const CacheEntry*>& entries,
    const std::filesystem::path& outputDirectory);

QString PngExportStatusText(
    const TexturePngExportResult& result,
    const QString& outputFile);

QString BulkPngExportStatusText(
    const BulkExportResults& results,
    const QString& outputDirectory);
