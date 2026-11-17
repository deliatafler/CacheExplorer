#include "QtTextureExport.h"

#include "QtHelpers.h"

namespace
{
    QString FormatCachedByteCount(std::size_t bytes)
    {
        if (bytes == 1)
        {
            return QStringLiteral("1 cached byte");
        }

        return QStringLiteral("%1 cached bytes")
            .arg(bytes);
    }

    QString ExportFailurePrefix(TexturePngExportStatus status)
    {
        switch (status)
        {
            case TexturePngExportStatus::Incomplete:
                return QStringLiteral("Export unavailable: ");

            case TexturePngExportStatus::RebuildFailed:
            case TexturePngExportStatus::WriteFailed:
            case TexturePngExportStatus::SkippedExisting:
            case TexturePngExportStatus::Exported:
                return QStringLiteral("Export failed: ");
        }

        return QStringLiteral("Export failed: ");
    }
}

QString DefaultPngExportFileName(const TextureCatalogEntry& entry)
{
    return ToQString(entry.DisplayIdentifier()) + QStringLiteral(".png");
}

TexturePngExportResult ExportTexturePng(
    const TextureCacheSource& source,
    const TextureCatalogEntry& entry,
    const std::filesystem::path& outputFile,
    bool overwriteExisting)
{
    TextureExportOptions options;
    options.overwriteExisting = overwriteExisting;
    options.verboseDecoderErrors = false;

    TextureExporter exporter;
    return exporter.ExportPngEntry(
        source,
        entry,
        outputFile,
        options);
}

BulkExportResults ExportTexturePngs(
    const TextureCacheSource& source,
    const std::vector<const TextureCatalogEntry*>& entries,
    const std::filesystem::path& outputDirectory)
{
    TextureExportOptions options;
    options.overwriteExisting = false;
    options.verboseDecoderErrors = false;

    TextureExporter exporter;
    return exporter.ExportPngEntries(
        source,
        entries,
        outputDirectory,
        options);
}

QString PngExportStatusText(
    const TexturePngExportResult& result,
    const QString& outputFile)
{
    if (!result.Succeeded())
    {
        return ExportFailurePrefix(result.status)
            + QString::fromUtf8(TextureExporter::StatusMessage(result.status))
            + QStringLiteral(" ")
            + ToQString(result.message);
    }

    if (result.status == TexturePngExportStatus::SkippedExisting)
    {
        return QStringLiteral("PNG already exists: %1")
            .arg(outputFile);
    }

    return QStringLiteral("Exported PNG: %1 (%2 x %3, %4)")
        .arg(outputFile)
        .arg(result.width)
        .arg(result.height)
        .arg(FormatCachedByteCount(result.cachedBytes));
}

QString BulkPngExportStatusText(
    const BulkExportResults& results,
    const QString& outputDirectory)
{
    const std::size_t incomplete =
        results.skippedKnownIncomplete + results.incompleteTextures;

    return QStringLiteral(
        "Exported %1 of %2 PNGs to %3. %4 incomplete, %5 existing, %6 failed.")
        .arg(results.exported)
        .arg(results.totalEntries)
        .arg(outputDirectory)
        .arg(incomplete)
        .arg(results.skippedExisting)
        .arg(results.ErrorCount());
}
