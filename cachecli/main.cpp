#include "TextureCacheDatabase.h"
#include "TextureRebuilder.h"
#include "UUID.h"
#include "TextureExporter.h"
#include "TextureSelection.h"

#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
	struct VerificationResults
{
    std::size_t entriesChecked = 0;
    std::size_t entriesVerified = 0;

    std::size_t invalidMetadata = 0;
    std::size_t missingHeaderData = 0;
    std::size_t invalidJ2CSignature = 0;

    std::size_t missingBodyFiles = 0;
    std::size_t undersizedBodyFiles = 0;
    std::size_t oversizedBodyFiles = 0;

    std::size_t headerOnlyEntries = 0;
    std::size_t entriesWithBodies = 0;

    std::vector<std::string> problems;
    std::vector<std::string> warnings;

    std::size_t ProblemCount() const
    {
        return invalidMetadata
            + missingHeaderData
            + invalidJ2CSignature
            + missingBodyFiles
            + undersizedBodyFiles;
    }

    std::size_t WarningCount() const
    {
        return oversizedBodyFiles;
    }
};

    void RecordWarning(
    VerificationResults& results,
    const CacheEntry& entry,
    const std::string& description,
    std::size_t maximumStoredWarnings = 25)
{
    if (results.warnings.size() >= maximumStoredWarnings)
    {
        return;
    }

    std::ostringstream stream;

    stream
        << entry.uuid.ToString()
        << ": "
        << description;

    results.warnings.push_back(stream.str());
}
    void RecordProblem(
        VerificationResults& results,
        const CacheEntry& entry,
        const std::string& description,
        std::size_t maximumStoredProblems = 25)
    {
        if (results.problems.size() >= maximumStoredProblems)
        {
            return;
        }

        std::ostringstream stream;

        stream
            << entry.uuid.ToString()
            << ": "
            << description;

        results.problems.push_back(stream.str());
    }

    bool HasJ2CSignature(
        std::ifstream& headerStream,
        std::uint64_t offset)
    {
        constexpr std::uint8_t ExpectedSignature[4] =
        {
            0xFF, 0x4F, 0xFF, 0x51
        };

        if (offset >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::streamoff>::max()))
        {
            return false;
        }

        headerStream.clear();

        headerStream.seekg(
            static_cast<std::streamoff>(offset),
            std::ios::beg);

        if (!headerStream)
        {
            return false;
        }

        std::uint8_t signature[4]{};

        headerStream.read(
            reinterpret_cast<char*>(signature),
            static_cast<std::streamsize>(
                sizeof(signature)));

        if (headerStream.gcount() !=
            static_cast<std::streamsize>(
                sizeof(signature)))
        {
            return false;
        }

        return std::equal(
            std::begin(signature),
            std::end(signature),
                std::begin(ExpectedSignature));
    }

    std::string FormatByteSize(std::uint64_t bytes)
{
    constexpr double Kilobyte = 1024.0;
    constexpr double Megabyte = Kilobyte * 1024.0;
    constexpr double Gigabyte = Megabyte * 1024.0;

    std::ostringstream stream;

    stream
        << std::fixed
        << std::setprecision(2);

    if (bytes >=
        static_cast<std::uint64_t>(Gigabyte))
    {
        stream
            << static_cast<double>(bytes) / Gigabyte
            << " GiB";
    }
    else if (bytes >=
             static_cast<std::uint64_t>(Megabyte))
    {
        stream
            << static_cast<double>(bytes) / Megabyte
            << " MiB";
    }
    else if (bytes >=
             static_cast<std::uint64_t>(Kilobyte))
    {
        stream
            << static_cast<double>(bytes) / Kilobyte
            << " KiB";
    }
    else
    {
        stream
            << bytes
            << " bytes";
    }

    return stream.str();
}

std::uint64_t CachedEntrySize(
    const CacheEntry& entry)
{
    const std::uint64_t headerBytes =
        std::min<std::uint64_t>(
            static_cast<std::uint64_t>(
                entry.imageSize),
            TextureRebuilder::HeaderBlockSize);

    return headerBytes
        + static_cast<std::uint64_t>(
            entry.bodySize);
}

    constexpr const char* ProgramName =
        "Firestorm Cache Explorer";

    constexpr const char* ProgramVersion = "0.1.0";

    void PrintUsage()
    {
        std::cout
            << ProgramName << " " << ProgramVersion << "\n\n"
            << "Usage:\n"
            << "  cachecli scan <cache-directory>\n"
            << "  cachecli list <cache-directory> [limit]\n"
            << "  cachecli verify <cache-directory>\n"
            << "  cachecli stats <cache-directory>\n"
            << "  cachecli export <cache-directory> <uuid> "
               "[output-file]\n\n"
	    << "  cachecli export-png <cache-directory> <uuid> "
               "[output-file]\n"
	       << "  cachecli export-png-all <cache-directory> "
   "<output-directory> [--overwrite]\n"
            << "The cache directory may be either:\n"
            << "  - the texturecache directory itself, or\n"
            << "  - its parent Firestorm cache directory.\n";
    }

    const char* CacheErrorMessage(CacheError error)
    {
        switch (error)
        {
            case CacheError::None:
                return "No error.";

            case CacheError::FileNotFound:
                return "texture.entries could not be found.";

            case CacheError::ReadError:
                return "The cache entry table could not be read.";

            case CacheError::InvalidHeader:
                return "The texture.entries header is invalid.";

            case CacheError::UnsupportedVersion:
                return "The cache version is unsupported.";
        }

        return "Unknown cache error.";
    }

    fs::path ResolveTextureCacheDirectory(
        const fs::path& suppliedPath)
    {
        if (fs::exists(suppliedPath / "texture.entries"))
        {
            return suppliedPath;
        }

        if (fs::exists(
                suppliedPath / "texturecache" / "texture.entries"))
        {
            return suppliedPath / "texturecache";
        }

        return suppliedPath;
    }

    std::string FormatTimestamp(std::time_t timestamp)
    {
        if (timestamp <= 0)
        {
            return "-";
        }

        std::tm localTime{};

#ifdef _WIN32
        if (localtime_s(&localTime, &timestamp) != 0)
        {
            return "-";
        }
#else
        if (localtime_r(&timestamp, &localTime) == nullptr)
        {
            return "-";
        }
#endif

        std::ostringstream stream;

        stream << std::put_time(
            &localTime,
            "%Y-%m-%d %H:%M:%S");

        return stream.str();
    }

    std::optional<std::size_t> ParseLimit(
        const std::string& value)
    {
        try
        {
            std::size_t parsedCharacters = 0;

            const unsigned long long parsed =
                std::stoull(value, &parsedCharacters, 10);

            if (parsedCharacters != value.size())
            {
                return std::nullopt;
            }

            if (parsed >
                static_cast<unsigned long long>(
                    std::numeric_limits<std::size_t>::max()))
            {
                return std::nullopt;
            }

            return static_cast<std::size_t>(parsed);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    bool OpenDatabase(
        const fs::path& suppliedPath,
        TextureCacheDatabase& database)
    {
        const fs::path cacheDirectory =
            ResolveTextureCacheDirectory(suppliedPath);

        const CacheError result =
            database.Open(cacheDirectory);

        if (result != CacheError::None)
        {
            std::cerr
                << "Error: "
                << CacheErrorMessage(result)
                << "\n"
                << "Path: "
                << cacheDirectory.string()
                << "\n";

            return false;
        }

        return true;
    }

    int ScanCommand(const fs::path& suppliedPath)
    {
        TextureCacheDatabase database;

        if (!OpenDatabase(suppliedPath, database))
        {
            return 1;
        }

        const CacheHeader& header = database.Header();

        std::cout
            << ProgramName << " " << ProgramVersion << "\n\n"
            << "Cache directory : "
            << database.CacheDirectory().string() << "\n"
            << "Version         : "
            << header.version << "\n"
            << "Address size    : "
            << header.addressSize << "\n"
            << "Encoder         : "
            << header.encoderVersion << "\n"
            << "Entry slots     : "
            << header.entryCount << "\n"
            << "Valid textures  : "
            << database.Entries().size() << "\n\n"
            << "Cache loaded successfully.\n";

        return 0;
    }

    int ListCommand(
        const fs::path& suppliedPath,
        std::size_t limit)
    {
        TextureCacheDatabase database;

        if (!OpenDatabase(suppliedPath, database))
        {
            return 1;
        }

        const auto& entries = database.Entries();

        const std::size_t count =
            std::min(limit, entries.size());

        std::cout
            << std::left
            << std::setw(38) << "UUID"
            << std::right
            << std::setw(12) << "Image"
            << std::setw(12) << "Body"
            << "  "
            << std::left
            << std::setw(19) << "Last Used"
            << "\n";

        std::cout << std::string(83, '-') << "\n";

        for (std::size_t i = 0; i < count; ++i)
        {
            const CacheEntry& entry = entries[i];

            std::cout
                << std::left
                << std::setw(38)
                << entry.uuid.ToString()
                << std::right
                << std::setw(12)
                << entry.imageSize
                << std::setw(12)
                << entry.bodySize
                << "  "
                << std::left
                << std::setw(19)
                << FormatTimestamp(entry.timestamp)
                << "\n";
        }

        if (count < entries.size())
        {
            std::cout
                << "\nShowing "
                << count
                << " of "
                << entries.size()
                << " entries.\n";
        }
        else
        {
            std::cout
                << "\n"
                << count
                << " entries shown.\n";
        }

        return 0;
    }

    int ExportCommand(
        const fs::path& suppliedPath,
        const std::string& uuidText,
        const std::optional<fs::path>& requestedOutput)
    {
        const std::optional<UUID> uuid =
            UUID::FromString(uuidText);

        if (!uuid)
        {
            std::cerr
                << "Error: Invalid UUID: "
                << uuidText
                << "\n";

            return 1;
        }

        TextureCacheDatabase database;

        if (!OpenDatabase(suppliedPath, database))
        {
            return 1;
        }

        const CacheEntry* entry = database.Find(*uuid);

        if (entry == nullptr)
        {
            std::cerr
                << "Error: UUID was not found in texture.entries.\n";

            return 1;
        }

        fs::path outputFile;

        if (requestedOutput)
        {
            outputFile = *requestedOutput;
        }
        else
        {
            outputFile = entry->uuid.ToString() + ".j2c";
        }

        TextureRebuilder rebuilder;

        const RebuildError result =
            rebuilder.ExportJ2C(
                database,
                *entry,
                outputFile);

        if (result != RebuildError::None)
        {
            std::cerr
                << "Error: "
                << TextureRebuilder::ErrorMessage(result)
                << "\n";

            return 1;
        }

        std::error_code error;

        const std::uintmax_t outputSize =
            fs::file_size(outputFile, error);

        std::cout
            << "Exported texture:\n"
            << "  UUID : "
            << entry->uuid.ToString()
            << "\n"
            << "  File : "
            << fs::absolute(outputFile).string()
            << "\n";

        if (!error)
        {
            std::cout
                << "  Size : "
                << outputSize
                << " bytes\n";
        }

        return 0;
    }

int ExportPngCommand(
    const fs::path& suppliedPath,
    const std::string& uuidText,
    const std::optional<fs::path>& requestedOutput)
{
    const std::optional<UUID> uuid =
        UUID::FromString(uuidText);

    if (!uuid)
    {
        std::cerr
            << "Error: Invalid UUID: "
            << uuidText
            << "\n";

        return 1;
    }

    TextureCacheDatabase database;

    if (!OpenDatabase(suppliedPath, database))
    {
        return 1;
    }

    const CacheEntry* entry = database.Find(*uuid);

    if (entry == nullptr)
    {
        std::cerr
            << "Error: UUID was not found in texture.entries.\n";

        return 1;
    }

    fs::path outputFile;

    if (requestedOutput)
    {
        outputFile = *requestedOutput;
    }
    else
    {
        outputFile = entry->uuid.ToString() + ".png";
    }

    TextureExportOptions options;
    options.overwriteExisting = true;
    options.verboseDecoderErrors = true;

    TextureExporter exporter;

    const TexturePngExportResult result =
        exporter.ExportPngEntry(
            database,
            *entry,
            outputFile,
            options);

    if (result.status != TexturePngExportStatus::Exported)
    {
        std::cerr
            << "Error exporting PNG: "
            << result.message
            << "\n";

        if (result.cachedBytes > 0)
        {
            std::cerr
                << "Cached bytes: "
                << result.cachedBytes
                << "\n";
        }

        return 1;
    }

    std::cout
        << "Exported PNG:\n"
        << "  UUID       : "
        << entry->uuid.ToString()
        << "\n"
        << "  Dimensions : "
        << result.width
        << " x "
        << result.height
        << "\n"
        << "  File       : "
        << fs::absolute(result.outputFile).string()
        << "\n";

    return 0;
}
int ExportPngAllCommand(
    const fs::path& suppliedPath,
    const fs::path& outputDirectory,
    bool overwriteExisting)
{
    TextureCacheDatabase database;

    if (!OpenDatabase(suppliedPath, database))
    {
        return 1;
    }

    const std::vector<const CacheEntry*> selectedEntries =
        TextureSelection::All(database);

    std::cout
        << "Bulk PNG export\n"
        << "---------------\n"
        << "Input cache : "
        << database.CacheDirectory().string()
        << "\n"
        << "Output      : "
        << fs::absolute(outputDirectory).string()
        << "\n"
        << "Textures    : "
        << selectedEntries.size()
        << "\n"
        << "Overwrite   : "
        << (overwriteExisting ? "yes" : "no")
        << "\n\n";

    TextureExportOptions options;
    options.overwriteExisting = overwriteExisting;
    options.verboseDecoderErrors = false;
    options.maximumStoredMessages = 50;

    TextureExporter exporter;

    const BulkExportResults results =
        exporter.ExportPngEntries(
            database,
            selectedEntries,
            outputDirectory,
            options,
            [](const TextureExportProgress& progress)
            {
                if (progress.processed % 100 != 0 &&
                    progress.processed != progress.total)
                {
                    return;
                }

                std::cout
                    << "\rProcessed "
                    << progress.processed
                    << " / "
                    << progress.total
                    << "  Exported: "
                    << progress.exported
                    << "  Incomplete: "
                    << progress.incompleteTextures
                    << "  Errors: "
                    << progress.errors
                    << "  Skipped: "
                    << progress.skippedExisting
                    << std::flush;
            });

    if (!selectedEntries.empty())
    {
        std::cout << "\n";
    }

    std::cout
        << "\nBulk export results\n"
        << "-------------------\n"
        << "Total entries         : "
        << results.totalEntries << "\n"
        << "Exported              : "
        << results.exported << "\n"
        << "Skipped existing      : "
        << results.skippedExisting << "\n"
        << "Incomplete/undecodable: "
        << results.incompleteTextures << "\n"
        << "Rebuild failures      : "
        << results.rebuildFailures << "\n"
        << "PNG write failures    : "
        << results.writeFailures << "\n"
        << "Total errors          : "
        << results.ErrorCount() << "\n";

    if (!results.messages.empty())
    {
        std::cout
            << "\nFirst "
            << results.messages.size()
            << " messages:\n";

        for (const std::string& message : results.messages)
        {
            std::cout
                << "  "
                << message
                << "\n";
        }
    }

    return results.ErrorCount() == 0 ? 0 : 2;
}

    int VerifyCommand(const fs::path& suppliedPath)
{
    TextureCacheDatabase database;

    if (!OpenDatabase(suppliedPath, database))
    {
        return 1;
    }

    const fs::path headerFile =
        database.CacheDirectory() / "texture.cache";

    std::ifstream headerStream(
        headerFile,
        std::ios::binary);

    if (!headerStream)
    {
        std::cerr
            << "Error: texture.cache could not be opened.\n"
            << "Path: "
            << headerFile.string()
            << "\n";

        return 1;
    }

    std::error_code fileSizeError;

    const std::uintmax_t headerFileSize =
        fs::file_size(
            headerFile,
            fileSizeError);

    if (fileSizeError)
    {
        std::cerr
            << "Error: Could not determine texture.cache size.\n";

        return 1;
    }

    VerificationResults results;

    for (const CacheEntry& entry : database.Entries())
{
    ++results.entriesChecked;

    /*
     * Firestorm considers entries valid when imageSize is positive
     * and greater than bodySize.
     */
    if (entry.imageSize <= 0 ||
        entry.bodySize < 0 ||
        entry.imageSize <= entry.bodySize)
    {
        ++results.invalidMetadata;

        RecordProblem(
            results,
            entry,
            "invalid image/body size metadata");

        continue;
    }

    const std::uint64_t headerOffset =
        static_cast<std::uint64_t>(
            entry.cacheIndex)
        * TextureRebuilder::HeaderBlockSize;

    /*
     * imageSize is the complete asset size when known. The amount
     * stored in texture.cache is only the first packet, capped at
     * 600 bytes.
     */
    const std::uint64_t requiredHeaderBytes =
        std::min<std::uint64_t>(
            static_cast<std::uint64_t>(
                entry.imageSize),
            TextureRebuilder::HeaderBlockSize);

    if (headerOffset >
            static_cast<std::uint64_t>(
                headerFileSize) ||
        requiredHeaderBytes >
            static_cast<std::uint64_t>(
                headerFileSize) - headerOffset)
    {
        ++results.missingHeaderData;

        RecordProblem(
            results,
            entry,
            "texture.cache does not contain the required header data");

        continue;
    }

    if (!HasJ2CSignature(
            headerStream,
            headerOffset))
    {
        ++results.invalidJ2CSignature;

        RecordProblem(
            results,
            entry,
            "header does not begin with FF 4F FF 51");

        continue;
    }

    if (entry.bodySize == 0)
    {
        ++results.headerOnlyEntries;
        ++results.entriesVerified;
        continue;
    }

    ++results.entriesWithBodies;

    const fs::path bodyFile =
        TextureRebuilder::BodyFilePath(
            database,
            entry);

    std::error_code existsError;

    const bool bodyExists =
        fs::exists(
            bodyFile,
            existsError);

    if (existsError || !bodyExists)
    {
        ++results.missingBodyFiles;

        RecordProblem(
            results,
            entry,
            "body file is missing");

        continue;
    }

    std::error_code sizeError;

    const std::uintmax_t actualBodySize =
        fs::file_size(
            bodyFile,
            sizeError);

    if (sizeError)
    {
        ++results.undersizedBodyFiles;

        RecordProblem(
            results,
            entry,
            "body-file size could not be read");

        continue;
    }

    const std::uintmax_t expectedBodySize =
        static_cast<std::uintmax_t>(
            entry.bodySize);

    if (actualBodySize < expectedBodySize)
    {
        ++results.undersizedBodyFiles;

        std::ostringstream description;

        description
            << "body file is too short; expected at least "
            << expectedBodySize
            << ", found "
            << actualBodySize;

        RecordProblem(
            results,
            entry,
            description.str());

        continue;
    }

    if (actualBodySize > expectedBodySize)
    {
        ++results.oversizedBodyFiles;

        std::ostringstream description;

        description
            << "body file contains "
            << actualBodySize - expectedBodySize
            << " unused trailing bytes";

        RecordWarning(
            results,
            entry,
            description.str());
    }

    ++results.entriesVerified;
}

std::cout
    << ProgramName << " "
    << ProgramVersion << "\n\n"
    << "Verification results\n"
    << "--------------------\n"
    << "Entries checked       : "
    << results.entriesChecked << "\n"
    << "Entries verified      : "
    << results.entriesVerified << "\n"
    << "Header-only entries   : "
    << results.headerOnlyEntries << "\n"
    << "Entries with bodies   : "
    << results.entriesWithBodies << "\n\n"
    << "Invalid metadata      : "
    << results.invalidMetadata << "\n"
    << "Missing header data   : "
    << results.missingHeaderData << "\n"
    << "Invalid J2C signatures: "
    << results.invalidJ2CSignature << "\n"
    << "Missing body files    : "
    << results.missingBodyFiles << "\n"
    << "Undersized body files : "
    << results.undersizedBodyFiles << "\n"
    << "Oversized body files  : "
    << results.oversizedBodyFiles
    << " (warnings)\n"
    << "Total problems        : "
    << results.ProblemCount() << "\n"
    << "Total warnings        : "
    << results.WarningCount() << "\n";

    if (!results.problems.empty())
    {
        std::cout
            << "\nFirst "
            << results.problems.size()
            << " problems:\n";

        for (const std::string& problem :
             results.problems)
        {
            std::cout
                << "  "
                << problem
                << "\n";
        }

        if (results.ProblemCount() >
            results.problems.size())
        {
            std::cout
                << "  ...and "
                << results.ProblemCount()
                    - results.problems.size()
                << " more.\n";
        }
    }

if (!results.warnings.empty())
{
    std::cout
        << "\nFirst "
        << results.warnings.size()
        << " warnings:\n";

    for (const std::string& warning :
         results.warnings)
    {
        std::cout
            << "  "
            << warning
            << "\n";
    }

    if (results.WarningCount() >
        results.warnings.size())
    {
        std::cout
            << "  ...and "
            << results.WarningCount()
                - results.warnings.size()
            << " more.\n";
    }
}

    if (results.ProblemCount() == 0)
    {
        std::cout
            << "\nCache verification passed.\n";

        return 0;
    }

    std::cout
        << "\nCache verification completed with problems.\n";

    return 2;

}
int StatsCommand(const fs::path& suppliedPath)
{
    TextureCacheDatabase database;

    if (!OpenDatabase(suppliedPath, database))
    {
        return 1;
    }

    const auto& entries = database.Entries();

    if (entries.empty())
    {
        std::cout
            << "The cache contains no valid textures.\n";

        return 0;
    }

    std::uint64_t totalCachedBytes = 0;
    std::uint64_t totalBodyBytes = 0;

    std::size_t headerOnlyCount = 0;
    std::size_t bodyCount = 0;

    const CacheEntry* smallestEntry = nullptr;
    const CacheEntry* largestEntry = nullptr;
    const CacheEntry* oldestEntry = nullptr;
    const CacheEntry* newestEntry = nullptr;

    for (const CacheEntry& entry : entries)
    {
        const std::uint64_t cachedSize =
            CachedEntrySize(entry);

        totalCachedBytes += cachedSize;

        if (entry.bodySize > 0)
        {
            ++bodyCount;

            totalBodyBytes +=
                static_cast<std::uint64_t>(
                    entry.bodySize);
        }
        else
        {
            ++headerOnlyCount;
        }

        if (smallestEntry == nullptr ||
            cachedSize <
                CachedEntrySize(*smallestEntry))
        {
            smallestEntry = &entry;
        }

        if (largestEntry == nullptr ||
            cachedSize >
                CachedEntrySize(*largestEntry))
        {
            largestEntry = &entry;
        }

        if (entry.timestamp > 0)
        {
            if (oldestEntry == nullptr ||
                entry.timestamp <
                    oldestEntry->timestamp)
            {
                oldestEntry = &entry;
            }

            if (newestEntry == nullptr ||
                entry.timestamp >
                    newestEntry->timestamp)
            {
                newestEntry = &entry;
            }
        }
    }

    std::error_code entriesError;
    std::error_code cacheError;
    std::error_code fastCacheError;

    const fs::path entriesFile =
        database.CacheDirectory()
        / "texture.entries";

    const fs::path cacheFile =
        database.CacheDirectory()
        / "texture.cache";

    const fs::path fastCacheFile =
        database.CacheDirectory()
        / "FastCache.cache";

    const std::uintmax_t entriesFileSize =
        fs::file_size(
            entriesFile,
            entriesError);

    const std::uintmax_t cacheFileSize =
        fs::file_size(
            cacheFile,
            cacheError);

    const std::uintmax_t fastCacheFileSize =
        fs::file_size(
            fastCacheFile,
            fastCacheError);

    const std::uint64_t averageSize =
        totalCachedBytes /
        static_cast<std::uint64_t>(
            entries.size());

    const std::size_t freeSlots =
        database.Header().entryCount >=
                entries.size()
            ? database.Header().entryCount
                - entries.size()
            : 0;

    std::cout
        << ProgramName << " "
        << ProgramVersion << "\n\n"
        << "Cache statistics\n"
        << "----------------\n"
        << "Cache version       : "
        << database.Header().version << "\n"
        << "Encoder             : "
        << database.Header().encoderVersion << "\n"
        << "Entry slots         : "
        << database.Header().entryCount << "\n"
        << "Valid textures      : "
        << entries.size() << "\n"
        << "Free/unused slots   : "
        << freeSlots << "\n"
        << "Header-only textures: "
        << headerOnlyCount << "\n"
        << "Textures with bodies: "
        << bodyCount << "\n\n"
        << "Cached texture data : "
        << FormatByteSize(totalCachedBytes)
        << "\n"
        << "Body-file data      : "
        << FormatByteSize(totalBodyBytes)
        << "\n"
        << "Average cached size : "
        << FormatByteSize(averageSize)
        << "\n";

    if (!entriesError)
    {
        std::cout
            << "texture.entries size: "
            << FormatByteSize(
                static_cast<std::uint64_t>(
                    entriesFileSize))
            << "\n";
    }

    if (!cacheError)
    {
        std::cout
            << "texture.cache size  : "
            << FormatByteSize(
                static_cast<std::uint64_t>(
                    cacheFileSize))
            << "\n";
    }

    if (!fastCacheError)
    {
        std::cout
            << "FastCache.cache size: "
            << FormatByteSize(
                static_cast<std::uint64_t>(
                    fastCacheFileSize))
            << "\n";
    }

    if (smallestEntry != nullptr)
    {
        std::cout
            << "\nSmallest cached texture\n"
            << "-----------------------\n"
            << "UUID: "
            << smallestEntry->uuid.ToString()
            << "\n"
            << "Size: "
            << FormatByteSize(
                CachedEntrySize(
                    *smallestEntry))
            << "\n";
    }

    if (largestEntry != nullptr)
    {
        std::cout
            << "\nLargest cached texture\n"
            << "----------------------\n"
            << "UUID: "
            << largestEntry->uuid.ToString()
            << "\n"
            << "Size: "
            << FormatByteSize(
                CachedEntrySize(
                    *largestEntry))
            << "\n";
    }

    if (oldestEntry != nullptr)
    {
        std::cout
            << "\nOldest timestamp\n"
            << "----------------\n"
            << "UUID: "
            << oldestEntry->uuid.ToString()
            << "\n"
            << "Time: "
            << FormatTimestamp(
                oldestEntry->timestamp)
            << "\n";
    }

    if (newestEntry != nullptr)
    {
        std::cout
            << "\nNewest timestamp\n"
            << "----------------\n"
            << "UUID: "
            << newestEntry->uuid.ToString()
            << "\n"
            << "Time: "
            << FormatTimestamp(
                newestEntry->timestamp)
            << "\n";
    }

    return 0;
}
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];

    if (command == "--help" ||
        command == "-h" ||
        command == "help")
    {
        PrintUsage();
        return 0;
    }

    if (command == "--version")
    {
        std::cout
            << ProgramName << " "
            << ProgramVersion << "\n";

        return 0;
    }

    if (command == "scan")
    {
        if (argc != 3)
        {
            std::cerr
                << "Error: scan requires a cache directory.\n\n";

            PrintUsage();
            return 1;
        }

        return ScanCommand(argv[2]);
    }

    if (command == "list")
    {
        if (argc < 3 || argc > 4)
        {
            std::cerr
                << "Error: list requires a cache directory "
                   "and an optional limit.\n\n";

            PrintUsage();
            return 1;
        }

        std::size_t limit =
            std::numeric_limits<std::size_t>::max();

        if (argc == 4)
        {
            const auto parsedLimit =
                ParseLimit(argv[3]);

            if (!parsedLimit)
            {
                std::cerr
                    << "Error: Invalid list limit: "
                    << argv[3]
                    << "\n";

                return 1;
            }

            limit = *parsedLimit;
        }

        return ListCommand(argv[2], limit);
    }

    if (command == "verify")
{
    if (argc != 3)
    {
        std::cerr
            << "Error: verify requires a cache directory.\n\n";

        PrintUsage();
        return 1;
    }

    return VerifyCommand(argv[2]);
}

if (command == "stats")
{
    if (argc != 3)
    {
        std::cerr
            << "Error: stats requires a cache directory.\n\n";

        PrintUsage();
        return 1;
    }

    return StatsCommand(argv[2]);
}

    if (command == "export")
    {
        if (argc < 4 || argc > 5)
        {
            std::cerr
                << "Error: export requires a cache directory, "
                   "UUID, and optional output file.\n\n";

            PrintUsage();
            return 1;
        }

        std::optional<fs::path> outputFile;

        if (argc == 5)
        {
            outputFile = fs::path(argv[4]);
        }

        return ExportCommand(
            argv[2],
            argv[3],
            outputFile);
    }

if (command == "export-png")
{
    if (argc < 4 || argc > 5)
    {
        std::cerr
            << "Error: export-png requires a cache directory, "
               "UUID, and optional output file.\n\n";

        PrintUsage();
        return 1;
    }

    std::optional<fs::path> outputFile;

    if (argc == 5)
    {
        outputFile = fs::path(argv[4]);
    }

    return ExportPngCommand(
        argv[2],
        argv[3],
        outputFile);
}

if (command == "export-png-all")
{
    if (argc < 4 || argc > 5)
    {
        std::cerr
            << "Error: export-png-all requires a cache directory, "
               "output directory, and optional --overwrite.\n\n";

        PrintUsage();
        return 1;
    }

    bool overwriteExisting = false;

    if (argc == 5)
    {
        const std::string option = argv[4];

        if (option != "--overwrite")
        {
            std::cerr
                << "Error: Unknown export-png-all option: "
                << option
                << "\n";

            return 1;
        }

        overwriteExisting = true;
    }

    return ExportPngAllCommand(
        argv[2],
        argv[3],
        overwriteExisting);
}

    std::cerr
        << "Error: Unknown command: "
        << command
        << "\n\n";

    PrintUsage();
    return 1;
}


