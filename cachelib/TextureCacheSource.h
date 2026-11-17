#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <memory>
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
    // Stable and filesystem-safe within one cache source. This may be a UUID
    // for legacy caches or an opaque keyed identifier for formats that hide
    // asset UUIDs.
    std::string sourceKey;

    std::optional<UUID> uuid;

    std::uint64_t cachedBytes = 0;
    std::optional<std::uint64_t> expectedBytes;
    std::optional<std::uint64_t> payloadBytes;
    std::optional<std::uint64_t> sourceIndex;

    std::time_t timestamp = 0;

    bool HasCompletePayload() const
    {
        return expectedBytes.has_value() &&
            *expectedBytes > 0 &&
            cachedBytes >= *expectedBytes;
    }

    std::string DisplayIdentifier() const
    {
        return uuid.has_value() ? uuid->ToString() : sourceKey;
    }
};

const char* TextureCacheOpenErrorMessage(TextureCacheOpenError error);
const char* TexturePayloadErrorMessage(TexturePayloadError error);

struct TexturePayload
{
    TexturePayloadFormat format = TexturePayloadFormat::Jpeg2000;
    std::vector<std::uint8_t> bytes;
};

class TexturePayloadRequest
{
public:
    virtual ~TexturePayloadRequest() = default;

    virtual TexturePayloadError Read(TexturePayload& output) const = 0;
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

    // Produces an immutable request that remains safe to read after the source
    // is refreshed or closed by the UI thread.
    virtual std::unique_ptr<TexturePayloadRequest> CreatePayloadRequest(
        const std::string& sourceKey) const = 0;
};
