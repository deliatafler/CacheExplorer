#include "AuroraCacheProbe.h"

#include <map>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    std::optional<AuroraTextureFileKind> FileKind(
        const fs::path& path)
    {
        const std::string extension = path.extension().string();

        if (extension == ".j2c")
        {
            return AuroraTextureFileKind::CompleteJpeg2000;
        }

        if (extension == ".part")
        {
            return AuroraTextureFileKind::PartialJpeg2000;
        }

        if (extension == ".bc7")
        {
            return AuroraTextureFileKind::TranscodedBc7;
        }

        return std::nullopt;
    }

    bool IsMissingPathError(const std::error_code& error)
    {
        const std::error_condition condition =
            error.default_error_condition();

        return condition ==
                std::make_error_condition(
                    std::errc::no_such_file_or_directory) ||
            condition ==
                std::make_error_condition(
                    std::errc::not_a_directory);
    }

    void AddCandidate(
        std::vector<fs::path>& candidates,
        const fs::path& candidate)
    {
        const fs::path normalized = candidate.lexically_normal();

        for (const fs::path& existing : candidates)
        {
            if (existing == normalized)
            {
                return;
            }
        }

        candidates.push_back(normalized);
    }

    std::optional<fs::path> ExistingDirectory(
        const fs::path& candidate,
        std::error_code& error)
    {
        error.clear();

        if (!fs::is_directory(candidate, error))
        {
            return std::nullopt;
        }

        fs::path canonical =
            fs::weakly_canonical(candidate, error);

        if (error)
        {
            return std::nullopt;
        }

        return canonical;
    }

    void StoreFile(
        AuroraTextureRecord& record,
        AuroraTextureFile file)
    {
        switch (file.kind)
        {
            case AuroraTextureFileKind::CompleteJpeg2000:
                record.completeJpeg2000 = std::move(file);
                return;

            case AuroraTextureFileKind::PartialJpeg2000:
                record.partialJpeg2000 = std::move(file);
                return;

            case AuroraTextureFileKind::TranscodedBc7:
                record.transcodedBc7 = std::move(file);
                return;
        }
    }
}

TextureCacheOpenError ProbeAuroraTextureStore(
    const std::filesystem::path& selectedDirectory,
    AuroraTextureStoreProbe& output)
{
    output = {};

    std::error_code error;
    const fs::file_status selectedStatus =
        fs::status(selectedDirectory, error);

    if (error)
    {
        return IsMissingPathError(error)
            ? TextureCacheOpenError::FileNotFound
            : TextureCacheOpenError::ReadError;
    }

    if (!fs::exists(selectedStatus))
    {
        return TextureCacheOpenError::FileNotFound;
    }

    if (!fs::is_directory(selectedStatus))
    {
        return TextureCacheOpenError::InvalidFormat;
    }

    std::vector<fs::path> candidates;
    const fs::path selected = selectedDirectory.lexically_normal();

    if (selected.filename() == "tex" &&
        selected.parent_path().filename() == "store")
    {
        AddCandidate(candidates, selected);
    }

    if (selected.filename() == "store")
    {
        AddCandidate(candidates, selected / "tex");
    }

    AddCandidate(candidates, selected / "store" / "tex");
    AddCandidate(
        candidates,
        selected /
            "AuroraViewerCache" /
            "store" /
            "tex");

    for (const fs::path& candidate : candidates)
    {
        const std::optional<fs::path> textureStore =
            ExistingDirectory(candidate, error);

        if (error)
        {
            if (IsMissingPathError(error))
            {
                continue;
            }

            return TextureCacheOpenError::ReadError;
        }

        if (!textureStore)
        {
            continue;
        }

        const fs::path storeDirectory =
            textureStore->parent_path();

        if (storeDirectory.filename() != "store")
        {
            continue;
        }

        output.textureStore = *textureStore;
        output.cacheRoot = storeDirectory.parent_path();
        return TextureCacheOpenError::None;
    }

    return TextureCacheOpenError::InvalidFormat;
}

TextureCacheOpenError EnumerateAuroraTextureStore(
    const std::filesystem::path& selectedDirectory,
    AuroraTextureStoreScan& output)
{
    output = {};

    const TextureCacheOpenError probeError =
        ProbeAuroraTextureStore(
            selectedDirectory,
            output.probe);

    if (probeError != TextureCacheOpenError::None)
    {
        return probeError;
    }

    std::error_code error;
    fs::directory_iterator iterator(
        output.probe.textureStore,
        error);

    if (error)
    {
        return TextureCacheOpenError::ReadError;
    }

    std::map<std::string, AuroraTextureRecord> records;
    const fs::directory_iterator end;

    while (iterator != end)
    {
        const fs::directory_entry entry = *iterator;

        iterator.increment(error);
        if (error)
        {
            return TextureCacheOpenError::ReadError;
        }

        std::error_code typeError;
        if (!entry.is_regular_file(typeError))
        {
            if (typeError)
            {
                ++output.cacheMissCount;
            }

            continue;
        }

        const std::optional<AuroraTextureFileKind> kind =
            FileKind(entry.path());
        const std::string sourceKey =
            entry.path().stem().string();

        if (!kind || !IsAuroraTextureKey(sourceKey))
        {
            ++output.ignoredFileCount;
            continue;
        }

        AuroraSealedFileInfo sealed;
        const AuroraFormatError formatError =
            InspectAuroraSealedFile(
                entry.path(),
                sealed);

        if (formatError == AuroraFormatError::UnsupportedVersion)
        {
            ++output.unsupportedFileCount;
            continue;
        }

        if (formatError != AuroraFormatError::None)
        {
            ++output.cacheMissCount;
            continue;
        }

        auto [record, inserted] = records.try_emplace(sourceKey);
        if (inserted)
        {
            record->second.sourceKey = sourceKey;
        }

        StoreFile(
            record->second,
            AuroraTextureFile{
                *kind,
                entry.path(),
                sealed});
    }

    output.records.reserve(records.size());
    for (auto& record : records)
    {
        output.records.push_back(
            std::move(record.second));
    }

    return TextureCacheOpenError::None;
}
