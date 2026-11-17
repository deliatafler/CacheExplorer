#include "LegacyTextureCacheSource.h"

#include "TextureRebuilder.h"

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

    for (const CacheEntry& legacyEntry : database_.Entries())
    {
        TextureCatalogEntry entry;
        entry.sourceKey = legacyEntry.uuid.ToString();
        entry.uuid = legacyEntry.uuid;
        entry.cachedBytes = CachedTextureByteCount(legacyEntry);

        if (legacyEntry.imageSize > 0)
        {
            entry.expectedBytes =
                static_cast<std::uint64_t>(legacyEntry.imageSize);
        }

        entry.timestamp = legacyEntry.timestamp;

        entries_.push_back(std::move(entry));
    }

    return TextureCacheOpenError::None;
}

void LegacyTextureCacheSource::Close()
{
    entries_.clear();
    database_.Close();
}

const TextureCatalogEntry* LegacyTextureCacheSource::FindByUuid(
    const UUID& uuid) const
{
    const CacheEntry* legacyEntry = database_.Find(uuid);

    if (legacyEntry == nullptr)
    {
        return nullptr;
    }

    return FindBySourceKey(legacyEntry->uuid.ToString());
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

    const TextureCatalogEntry* entry = FindBySourceKey(sourceKey);

    if (entry == nullptr || !entry->uuid.has_value())
    {
        return TexturePayloadError::EntryNotFound;
    }

    const CacheEntry* legacyEntry = database_.Find(*entry->uuid);

    if (legacyEntry == nullptr)
    {
        return TexturePayloadError::EntryNotFound;
    }

    TextureRebuilder rebuilder;
    const RebuildError error =
        rebuilder.Rebuild(database_, *legacyEntry, output.bytes);

    if (error != RebuildError::None)
    {
        output.bytes.clear();
        return MapRebuildError(error);
    }

    output.format = TexturePayloadFormat::Jpeg2000;
    return TexturePayloadError::None;
}

const TextureCatalogEntry* LegacyTextureCacheSource::FindBySourceKey(
    const std::string& sourceKey) const
{
    for (const TextureCatalogEntry& entry : entries_)
    {
        if (entry.sourceKey == sourceKey)
        {
            return &entry;
        }
    }

    return nullptr;
}
