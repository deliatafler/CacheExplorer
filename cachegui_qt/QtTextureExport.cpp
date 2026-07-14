#include "QtTextureExport.h"

#include "QtHelpers.h"

QString DefaultPngExportFileName(const CacheEntry& entry)
{
    return ToQString(entry.uuid.ToString()) + QStringLiteral(".png");
}

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

QString PngExportStatusText(
    const TexturePngExportResult& result,
    const QString& outputFile)
{
    if (!result.Succeeded())
    {
        return QStringLiteral("Export failed: ")
            + ToQString(result.message);
    }

    return QStringLiteral("Exported PNG: %1").arg(outputFile);
}
