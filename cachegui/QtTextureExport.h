#pragma once

#include "TextureCacheSource.h"
#include "TextureExporter.h"

#include <filesystem>

#include <QString>

QString DefaultPngExportFileName(const TextureCatalogEntry& entry);

TexturePngExportResult ExportTexturePng(
    const TextureCacheSource& source,
    const TextureCatalogEntry& entry,
    const std::filesystem::path& outputFile,
    bool overwriteExisting);

BulkExportResults ExportTexturePngs(
    const TextureCacheSource& source,
    const std::vector<const TextureCatalogEntry*>& entries,
    const std::filesystem::path& outputDirectory);

QString PngExportStatusText(
    const TexturePngExportResult& result,
    const QString& outputFile);

QString BulkPngExportStatusText(
    const BulkExportResults& results,
    const QString& outputDirectory);
