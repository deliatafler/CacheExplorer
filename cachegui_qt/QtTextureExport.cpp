#include "QtTextureExport.h"

TexturePngExportResult ExportTexturePng(
    const TextureCacheDatabase& database,
    const CacheEntry& entry,
    const std::filesystem::path& outputFile,
    bool overwriteExisting)
{
    TextureExportOptions options;
    options.overwriteExisting = overwriteExisting;
    options.verboseDecoderErrors = false;

    TextureExporter exporter;
    return exporter.ExportPngEntry(
        database,
        entry,
        outputFile,
        options);
}
