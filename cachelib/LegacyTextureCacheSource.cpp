#include "LegacyTextureCacheSource.h"

#include "TextureRebuilder.h"

#include <memory>
#include <utility>

namespace
{
    TextureCacheOpenError MapOpenError(CacheError error)
    {
        switch (error)
        {
            case CacheError::None:
                return TextureCacheOpenError::None;

            case CacheError::FileNotFound:
                return TextureCacheOpenError::FileNotFound;

            case CacheError::ReadError:
                return TextureCacheOpenError::ReadError;

            case CacheError::InvalidHeader:
                return TextureCacheOpenError::InvalidFormat;

            case CacheError::UnsupportedVersion:
                return TextureCacheOpenError::UnsupportedVersion;
        }

        return TextureCacheOpenError::ReadError;
    }

    TexturePayloadError MapRebuildError(RebuildError error)
    {
        switch (error)
        {
            case RebuildError::None:
                return TexturePayloadError::None;

            case RebuildError::DatabaseNotOpen:
                return TexturePayloadError::SourceNotOpen;

            case RebuildError::InvalidEntry:
                return TexturePayloadError::InvalidEntry;

            case RebuildError::HeaderFileNotFound:
            case RebuildError::BodyFileNotFound:
                return TexturePayloadError::MissingData;

            case RebuildError::HeaderReadError:
            case RebuildError::BodyReadError:
            case RebuildError::OutputOpenError:
            case RebuildError::OutputWriteError:
                return TexturePayloadError::ReadError;
        }

        return TexturePayloadError::ReadError;
    }

    class LegacyTexturePayloadRequest final : public TexturePayloadRequest
    {
    public:
        LegacyTexturePayloadRequest(
            std::filesystem::path cacheDirectory,
            CacheEntry entry)
            : cacheDirectory_(std::move(cacheDirectory)),
              entry_(std::move(entry))
        {
        }

        TexturePayloadError Read(TexturePayload& output) const override
        {
            output = {};

            TextureRebuilder rebuilder;
            const RebuildError error =
                rebuilder.Rebuild(cacheDirectory_, entry_, output.bytes);

            if (error != RebuildError::None)
            {
                output.bytes.clear();
                return MapRebuildError(error);
            }

            output.format = TexturePayloadFormat::Jpeg2000;
            return TexturePayloadError::None;
        }

    private:
        std::filesystem::path cacheDirectory_;
        CacheEntry entry_;
    };
}

TextureCacheOpenError LegacyTextureCacheSource::Open(
    const std::filesystem::path& cacheDirectory)
{
    Close();

    const CacheError error = database_.Open(cacheDirectory);

    if (error != CacheError::None)
    {
        return MapOpenError(error);
    }

    entries_.reserve(database_.Entries().size());
    entryIndices_.reserve(database_.Entries().size());

    for (const CacheEntry& legacyEntry : database_.Entries())
    {
        TextureCatalogEntry entry;
        entry.sourceKey = legacyEntry.uuid.ToString();
        entry.uuid = legacyEntry.uuid;
        entry.cachedBytes = CachedTextureByteCount(legacyEntry);
        entry.sourceIndex = legacyEntry.cacheIndex;

        if (legacyEntry.imageSize > 0)
        {
            entry.expectedBytes =
                static_cast<std::uint64_t>(legacyEntry.imageSize);
        }

        if (legacyEntry.bodySize >= 0)
        {
            entry.payloadBytes =
                static_cast<std::uint64_t>(legacyEntry.bodySize);
        }

        if (entry.HasCompletePayload())
        {
            entry.hasCompleteJpeg2000 = true;
        }
        else
        {
            entry.hasPartialJpeg2000 = true;
        }

        entry.timestamp = legacyEntry.timestamp;

        entryIndices_.emplace(entry.sourceKey, entries_.size());
        entries_.push_back(std::move(entry));
    }

    return TextureCacheOpenError::None;
}

void LegacyTextureCacheSource::Close()
{
    entryIndices_.clear();
    entries_.clear();
    database_.Close();
}

const TextureCatalogEntry* LegacyTextureCacheSource::FindByUuid(
    const UUID& uuid) const
{
    return FindBySourceKey(uuid.ToString());
}

TexturePayloadError LegacyTextureCacheSource::ReadPayload(
    const std::string& sourceKey,
    TexturePayload& output) const
{
    output = {};

    if (!database_.IsOpen())
    {
        return TexturePayloadError::SourceNotOpen;
    }

    const auto entryIndex = entryIndices_.find(sourceKey);

    if (entryIndex == entryIndices_.end() ||
        entryIndex->second >= database_.Entries().size())
    {
        return TexturePayloadError::EntryNotFound;
    }

    const CacheEntry& legacyEntry = database_.Entries()[entryIndex->second];

    TextureRebuilder rebuilder;
    const RebuildError error =
        rebuilder.Rebuild(database_, legacyEntry, output.bytes);

    if (error != RebuildError::None)
    {
        output.bytes.clear();
        return MapRebuildError(error);
    }

    output.format = TexturePayloadFormat::Jpeg2000;
    return TexturePayloadError::None;
}

std::unique_ptr<TexturePayloadRequest>
LegacyTextureCacheSource::CreatePayloadRequest(
    const std::string& sourceKey) const
{
    if (!database_.IsOpen())
    {
        return {};
    }

    const auto entryIndex = entryIndices_.find(sourceKey);

    if (entryIndex == entryIndices_.end() ||
        entryIndex->second >= database_.Entries().size())
    {
        return {};
    }

    const CacheEntry& legacyEntry = database_.Entries()[entryIndex->second];

    return std::make_unique<LegacyTexturePayloadRequest>(
        database_.CacheDirectory(),
        legacyEntry);
}

const TextureCatalogEntry* LegacyTextureCacheSource::FindBySourceKey(
    const std::string& sourceKey) const
{
    const auto entryIndex = entryIndices_.find(sourceKey);

    if (entryIndex == entryIndices_.end() ||
        entryIndex->second >= entries_.size())
    {
        return nullptr;
    }

    return &entries_[entryIndex->second];
}
