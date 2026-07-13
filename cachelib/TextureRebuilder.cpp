#include "TextureRebuilder.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <system_error>

namespace
{
    bool ReadExact(
        std::istream& stream,
        std::uint8_t* destination,
        std::size_t byteCount)
    {
        if (byteCount == 0)
        {
            return true;
        }

        if (byteCount >
            static_cast<std::size_t>(
                std::numeric_limits<std::streamsize>::max()))
        {
            return false;
        }

        stream.read(
            reinterpret_cast<char*>(destination),
            static_cast<std::streamsize>(byteCount));

        return stream.gcount() ==
            static_cast<std::streamsize>(byteCount);
    }
}

RebuildError TextureRebuilder::Rebuild(
    const TextureCacheDatabase& database,
    const CacheEntry& entry,
    std::vector<std::uint8_t>& outputData) const
{
    outputData.clear();

    if (!database.IsOpen())
    {
        return RebuildError::DatabaseNotOpen;
    }

    if (entry.imageSize <= 0 || entry.bodySize < 0)
    {
        return RebuildError::InvalidEntry;
    }

    const std::uint64_t imageSize =
        static_cast<std::uint64_t>(entry.imageSize);

    const std::uint64_t bodySize =
        static_cast<std::uint64_t>(entry.bodySize);

    /*
     * Firestorm stores:
     *
     *   texture.cache:
     *       One fixed 600-byte block per texture.entries record.
     *
     *   <uuid>.texture:
     *       Bytes following the first 600 cached bytes.
     *
     * For textures smaller than 600 bytes, the texture.cache record is padded.
     * In that case, only imageSize bytes should be returned.
     *
     * For partially cached progressive JPEG2000 images, imageSize can describe
     * the complete asset while the cache contains fewer bytes. The amount
     * actually available is therefore determined by bodySize.
     */
    const std::uint64_t headerBytes =
    std::min<std::uint64_t>(
        imageSize,
        HeaderBlockSize);

    const std::uint64_t availableSize = headerBytes + bodySize;

    if (availableSize == 0 ||
        availableSize >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()))
    {
        return RebuildError::InvalidEntry;
    }

    outputData.resize(static_cast<std::size_t>(availableSize));

    const auto headerFile =
        database.CacheDirectory() / "texture.cache";

    std::ifstream headerStream(headerFile, std::ios::binary);

    if (!headerStream)
    {
        outputData.clear();
        return RebuildError::HeaderFileNotFound;
    }

    const std::uint64_t headerOffset =
        static_cast<std::uint64_t>(entry.cacheIndex) *
        HeaderBlockSize;

    if (headerOffset >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::streamoff>::max()))
    {
        outputData.clear();
        return RebuildError::InvalidEntry;
    }

    headerStream.seekg(
        static_cast<std::streamoff>(headerOffset),
        std::ios::beg);

    if (!headerStream)
    {
        outputData.clear();
        return RebuildError::HeaderReadError;
    }

    if (!ReadExact(
            headerStream,
            outputData.data(),
            static_cast<std::size_t>(headerBytes)))
    {
        outputData.clear();
        return RebuildError::HeaderReadError;
    }

    if (bodySize == 0)
    {
        return RebuildError::None;
    }

    const auto bodyFile = BodyFilePath(database, entry);

    std::ifstream bodyStream(bodyFile, std::ios::binary);

    if (!bodyStream)
    {
        outputData.clear();
        return RebuildError::BodyFileNotFound;
    }

    if (!ReadExact(
            bodyStream,
            outputData.data() + static_cast<std::size_t>(headerBytes),
            static_cast<std::size_t>(bodySize)))
    {
        outputData.clear();
        return RebuildError::BodyReadError;
    }

    return RebuildError::None;
}

RebuildError TextureRebuilder::ExportJ2C(
    const TextureCacheDatabase& database,
    const CacheEntry& entry,
    const std::filesystem::path& outputFile) const
{
    std::vector<std::uint8_t> reconstructedData;

    const RebuildError rebuildResult =
        Rebuild(database, entry, reconstructedData);

    if (rebuildResult != RebuildError::None)
    {
        return rebuildResult;
    }

    const std::filesystem::path parentDirectory =
        outputFile.parent_path();

    if (!parentDirectory.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(
            parentDirectory,
            error);

        if (error)
        {
            return RebuildError::OutputOpenError;
        }
    }

    std::ofstream outputStream(
        outputFile,
        std::ios::binary | std::ios::trunc);

    if (!outputStream)
    {
        return RebuildError::OutputOpenError;
    }

    if (reconstructedData.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::streamsize>::max()))
    {
        return RebuildError::OutputWriteError;
    }

    outputStream.write(
        reinterpret_cast<const char*>(reconstructedData.data()),
        static_cast<std::streamsize>(reconstructedData.size()));

    if (!outputStream)
    {
        return RebuildError::OutputWriteError;
    }

    return RebuildError::None;
}

std::filesystem::path TextureRebuilder::BodyFilePath(
    const TextureCacheDatabase& database,
    const CacheEntry& entry)
{
    const std::string uuid = entry.uuid.ToString();

    if (uuid.empty())
    {
        return {};
    }

    /*
     * The body file is stored beneath a directory named after the first
     * hexadecimal character of the UUID:
     *
     * texturecache/2/2b8f....texture
     */
    return database.CacheDirectory()
        / std::string(1, uuid.front())
        / (uuid + ".texture");
}

const char* TextureRebuilder::ErrorMessage(
    RebuildError error)
{
    switch (error)
    {
        case RebuildError::None:
            return "No error.";

        case RebuildError::DatabaseNotOpen:
            return "The texture cache database is not open.";

        case RebuildError::InvalidEntry:
            return "The cache entry contains invalid metadata.";

        case RebuildError::HeaderFileNotFound:
            return "The texture.cache file could not be opened.";

        case RebuildError::HeaderReadError:
            return "The texture header block could not be read.";

        case RebuildError::BodyFileNotFound:
            return "The texture body file could not be opened.";

        case RebuildError::BodyReadError:
            return "The complete texture body could not be read.";

        case RebuildError::OutputOpenError:
            return "The output file could not be opened.";

        case RebuildError::OutputWriteError:
            return "The reconstructed texture could not be written.";
    }

    return "Unknown texture reconstruction error.";
}
