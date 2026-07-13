#include "TextureExporter.h"

#include "J2CDecoder.h"
#include "PngWriter.h"
#include "TextureRebuilder.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
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
}

BulkExportResults TextureExporter::ExportPngEntries(
    const TextureCacheDatabase& database,
    const std::vector<const CacheEntry*>& entries,
    const fs::path& outputDirectory,
    const TextureExportOptions& options) const
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

        return results;
    }

    TextureRebuilder rebuilder;
    J2CDecoder decoder;
    PngWriter writer;

    std::size_t processed = 0;

    for (const CacheEntry* entry : entries)
    {
        ++processed;

        if (entry == nullptr)
        {
            ++results.rebuildFailures;
            continue;
        }

        const fs::path outputFile =
            outputDirectory
            / (entry->uuid.ToString() + ".png");

        if (!options.overwriteExisting)
        {
            std::error_code existsError;

            if (fs::exists(outputFile, existsError) &&
                !existsError)
            {
                ++results.skippedExisting;
                continue;
            }
        }

        std::vector<std::uint8_t> encodedData;

        const RebuildError rebuildResult =
            rebuilder.Rebuild(
                database,
                *entry,
                encodedData);

        if (rebuildResult != RebuildError::None)
        {
            ++results.rebuildFailures;

            RecordMessage(
                results,
                *entry,
                std::string("rebuild failed: ")
                    + TextureRebuilder::ErrorMessage(
                        rebuildResult),
                options.maximumStoredMessages);

            continue;
        }

        DecodedImage decodedImage;

        const DecodeError decodeResult =
            decoder.Decode(
                encodedData,
                decodedImage,
                options.verboseDecoderErrors);

        if (decodeResult != DecodeError::None)
        {
            ++results.incompleteTextures;

            RecordMessage(
                results,
                *entry,
                std::string("not decodable from cached data: ")
                    + J2CDecoder::ErrorMessage(
                        decodeResult),
                options.maximumStoredMessages);

            continue;
        }

        const PngWriteError writeResult =
            writer.Write(
                outputFile,
                decodedImage);

        if (writeResult != PngWriteError::None)
        {
            ++results.writeFailures;

            RecordMessage(
                results,
                *entry,
                std::string("PNG write failed: ")
                    + PngWriter::ErrorMessage(
                        writeResult),
                options.maximumStoredMessages);

            continue;
        }

        ++results.exported;

        if (processed % 100 == 0 ||
            processed == entries.size())
        {
            std::cout
                << "\rProcessed "
                << processed
                << " / "
                << entries.size()
                << "  Exported: "
                << results.exported
                << "  Incomplete: "
                << results.incompleteTextures
                << "  Errors: "
                << results.ErrorCount()
                << "  Skipped: "
                << results.skippedExisting
                << std::flush;
        }
    }

    if (!entries.empty())
    {
        std::cout << "\n";
    }

    return results;
}
