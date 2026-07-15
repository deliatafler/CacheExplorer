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
