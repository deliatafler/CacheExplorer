#include "TextureExporter.h"

#include "TextureExportState.h"

#include "J2CDecoder.h"
#include "PngWriter.h"
#include "TextureRebuilder.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    void RecordMessage(
        BulkExportResults& results,
        const CacheEntry& entry,
        const std::string& message,
        std::size_t maximumStoredMessages)
    {
        if (results.messages.size() >= maximumStoredMessages)
        {
            return;
        }

        results.messages.push_back(
            entry.uuid.ToString() + ": " + message);
    }

    void ReportProgress(
        const BulkExportResults& results,
        std::size_t processed,
        TextureExportProgressCallback& progressCallback)
    {
        if (!progressCallback)
        {
            return;
        }

        TextureExportProgress progress;
        progress.processed = processed;
        progress.total = results.totalEntries;
        progress.exported = results.exported;
        progress.skippedExisting = results.skippedExisting;
        progress.skippedKnownIncomplete = results.skippedKnownIncomplete;
        progress.incompleteTextures = results.incompleteTextures;
        progress.errors = results.ErrorCount();

        progressCallback(progress);
    }
}

TexturePngExportResult TextureExporter::ExportPngEntry(
    const TextureCacheDatabase& database,
    const CacheEntry& entry,
    const fs::path& outputFile,
    const TextureExportOptions& options) const
{
    TexturePngExportResult result;
    result.outputFile = outputFile;

    if (!options.overwriteExisting)
    {
        std::error_code existsError;

        if (fs::exists(outputFile, existsError) &&
            !existsError)
        {
            result.status = TexturePngExportStatus::SkippedExisting;
            result.message = StatusMessage(result.status);
            return result;
        }
    }

    TextureRebuilder rebuilder;
    std::vector<std::uint8_t> encodedData;

    const RebuildError rebuildResult =
        rebuilder.Rebuild(
            database,
            entry,
            encodedData);

    if (rebuildResult != RebuildError::None)
    {
        result.status = TexturePngExportStatus::RebuildFailed;
        result.message = TextureRebuilder::ErrorMessage(rebuildResult);
        return result;
    }

    result.cachedBytes = encodedData.size();

    J2CDecoder decoder;
    DecodedImage decodedImage;

    const DecodeError decodeResult =
        decoder.Decode(
            encodedData,
            decodedImage,
            options.verboseDecoderErrors);

    if (decodeResult != DecodeError::None)
    {
        result.status = TexturePngExportStatus::Incomplete;
        result.message = J2CDecoder::ErrorMessage(decodeResult);
        return result;
    }

    PngWriter writer;

    const PngWriteError writeResult =
        writer.Write(
            outputFile,
            decodedImage);

    if (writeResult != PngWriteError::None)
    {
        result.status = TexturePngExportStatus::WriteFailed;
        result.message = PngWriter::ErrorMessage(writeResult);
        return result;
    }

    result.status = TexturePngExportStatus::Exported;
    result.message = StatusMessage(result.status);
    result.width = decodedImage.width;
    result.height = decodedImage.height;

    return result;
}

BulkExportResults TextureExporter::ExportPngEntries(
    const TextureCacheDatabase& database,
    const std::vector<const CacheEntry*>& entries,
    const fs::path& outputDirectory,
    const TextureExportOptions& options,
    TextureExportProgressCallback progressCallback) const
{
    BulkExportResults results;
    results.totalEntries = entries.size();

    std::error_code directoryError;

    fs::create_directories(
        outputDirectory,
        directoryError);

    if (directoryError)
    {
        results.writeFailures = entries.size();
        results.messages.push_back(
            "Could not create output directory: "
            + outputDirectory.string());
        ReportProgress(results, entries.size(), progressCallback);
        return results;
    }

    std::size_t processed = 0;

    for (const CacheEntry* entry : entries)
    {
        ++processed;

        if (entry == nullptr)
        {
            ++results.rebuildFailures;
            ReportProgress(results, processed, progressCallback);
            continue;
        }

        const fs::path outputFile =
            outputDirectory
            / (entry->uuid.ToString() + ".png");

        if (options.exportState != nullptr &&
            options.skipKnownIncomplete &&
            options.exportState->IsKnownIncomplete(*entry))
        {
            if (!options.overwriteExisting)
            {
                std::error_code existsError;

                if (fs::exists(outputFile, existsError) &&
                    !existsError)
                {
                    ++results.skippedExisting;
                    options.exportState->MarkSucceeded(*entry);
                    ReportProgress(results, processed, progressCallback);
                    continue;
                }
            }

            ++results.skippedKnownIncomplete;
            ReportProgress(results, processed, progressCallback);
            continue;
        }

        const TexturePngExportResult exportResult =
            ExportPngEntry(
                database,
                *entry,
                outputFile,
                options);

        switch (exportResult.status)
        {
            case TexturePngExportStatus::Exported:
                ++results.exported;
                if (options.exportState != nullptr)
                {
                    options.exportState->MarkSucceeded(*entry);
                }
                break;

            case TexturePngExportStatus::SkippedExisting:
                ++results.skippedExisting;
                if (options.exportState != nullptr)
                {
                    options.exportState->MarkSucceeded(*entry);
                }
                break;

            case TexturePngExportStatus::Incomplete:
                ++results.incompleteTextures;
                if (options.exportState != nullptr)
                {
                    options.exportState->MarkIncomplete(
                        *entry,
                        exportResult.message);
                }
                RecordMessage(
                    results,
                    *entry,
                    std::string("not decodable from cached data: ")
                        + exportResult.message,
                    options.maximumStoredMessages);
                break;

            case TexturePngExportStatus::RebuildFailed:
                ++results.rebuildFailures;
                RecordMessage(
                    results,
                    *entry,
                    std::string("rebuild failed: ")
                        + exportResult.message,
                    options.maximumStoredMessages);
                break;

            case TexturePngExportStatus::WriteFailed:
                ++results.writeFailures;
                RecordMessage(
                    results,
                    *entry,
                    std::string("PNG write failed: ")
                        + exportResult.message,
                    options.maximumStoredMessages);
                break;
        }

        ReportProgress(results, processed, progressCallback);
    }

    return results;
}

const char* TextureExporter::StatusMessage(
    TexturePngExportStatus status)
{
    switch (status)
    {
        case TexturePngExportStatus::Exported:
            return "PNG exported successfully.";

        case TexturePngExportStatus::SkippedExisting:
            return "The PNG already exists.";

        case TexturePngExportStatus::Incomplete:
            return "The cached texture data is incomplete or undecodable.";

        case TexturePngExportStatus::RebuildFailed:
            return "The cached JPEG2000 stream could not be rebuilt.";

        case TexturePngExportStatus::WriteFailed:
            return "The PNG could not be written.";
    }

    return "Unknown PNG export status.";
}
