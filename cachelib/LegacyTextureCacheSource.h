#pragma once

#include "TextureCacheDatabase.h"
#include "TextureCacheSource.h"

class LegacyTextureCacheSource final : public TextureCacheSource
{
public:
    TextureCacheOpenError Open(
        const std::filesystem::path& cacheDirectory) override;

    void Close() override;

    bool IsOpen() const override
    {
        return database_.IsOpen();
    }

    TextureCacheFormat Format() const override
    {
        return TextureCacheFormat::LegacyViewer;
    }

    const std::filesystem::path& CacheDirectory() const override
    {
        return database_.CacheDirectory();
    }

    const std::vector<TextureCatalogEntry>& Entries() const override
    {
        return entries_;
    }

    const TextureCatalogEntry* FindByUuid(
        const UUID& uuid) const override;

    TexturePayloadError ReadPayload(
        const std::string& sourceKey,
        TexturePayload& output) const override;

private:
    const TextureCatalogEntry* FindBySourceKey(
        const std::string& sourceKey) const;

    TextureCacheDatabase database_;
    std::vector<TextureCatalogEntry> entries_;
};
