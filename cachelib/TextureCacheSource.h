#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "UUID.h"

enum class TextureCacheFormat
{
    LegacyViewer,
    Aurora
};

enum class TextureCacheOpenError
{
    None,
    FileNotFound,
    ReadError,
    InvalidFormat,
    UnsupportedVersion
};

enum class TexturePayloadFormat
{
    Jpeg2000,
    Bc7
};

enum class TexturePayloadError
{
    None,
    SourceNotOpen,
    EntryNotFound,
    InvalidEntry,
    MissingData,
    ReadError,
    UnsupportedFormat
};

struct TextureCatalogEntry
{
    // Stable within one cache source. This may be a UUID for legacy caches or
    // an opaque keyed identifier for cache formats that hide asset UUIDs.
    std::string sourceKey;

    std::optional<UUID> uuid;

    std::uint64_t cachedBytes = 0;
    std::optional<std::uint64_t> expectedBytes;

    std::time_t timestamp = 0;

    bool HasCompletePayload() const
    {
        return expectedBytes.has_value() &&
            *expectedBytes > 0 &&
            cachedBytes >= *expectedBytes;
    }
};

struct TexturePayload
{
    TexturePayloadFormat format = TexturePayloadFormat::Jpeg2000;
    std::vector<std::uint8_t> bytes;
};

class TextureCacheSource
{
public:
    virtual ~TextureCacheSource() = default;

    virtual TextureCacheOpenError Open(
        const std::filesystem::path& cacheDirectory) = 0;

    virtual void Close() = 0;

    virtual bool IsOpen() const = 0;
    virtual TextureCacheFormat Format() const = 0;

    virtual const std::filesystem::path& CacheDirectory() const = 0;

    // Returned pointers remain valid only until the source is closed or Open
    // is called again, including an Open call that fails.
    virtual const std::vector<TextureCatalogEntry>& Entries() const = 0;

    virtual const TextureCatalogEntry* FindByUuid(
        const UUID& uuid) const = 0;

    virtual TexturePayloadError ReadPayload(
        const std::string& sourceKey,
        TexturePayload& output) const = 0;
};
