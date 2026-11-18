#pragma once

#include "AuroraCacheKey.h"
#include "AuroraCacheProbe.h"
#include "TextureCacheSource.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using AuroraCacheKeyLoader =
    std::function<AuroraCredentialError(AuroraCacheKey&)>;

class AuroraTextureCacheSource final : public TextureCacheSource
{
public:
    AuroraTextureCacheSource();
    explicit AuroraTextureCacheSource(AuroraCacheKeyLoader keyLoader);

    TextureCacheOpenError Open(
        const std::filesystem::path& cacheDirectory) override;

    void Close() override;

    bool IsOpen() const override
    {
        return open_;
    }

    TextureCacheFormat Format() const override
    {
        return TextureCacheFormat::Aurora;
    }

    const std::filesystem::path& CacheDirectory() const override
    {
        return cacheDirectory_;
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

    std::unique_ptr<TexturePayloadRequest> CreatePayloadRequest(
        const std::string& sourceKey) const override;

    AuroraCredentialError CredentialError() const
    {
        return credentialError_;
    }

    std::size_t CacheMissCount() const
    {
        return cacheMissCount_;
    }

private:
    const TextureCatalogEntry* FindBySourceKey(
        const std::string& sourceKey) const;

    AuroraCacheKeyLoader keyLoader_;
    bool open_ = false;
    std::filesystem::path cacheDirectory_;
    std::shared_ptr<const AuroraCacheKey> key_;
    std::vector<AuroraTextureRecord> records_;
    std::vector<TextureCatalogEntry> entries_;
    std::unordered_map<std::string, std::size_t> entryIndices_;
    std::unordered_map<std::string, std::size_t> recordIndices_;
    AuroraCredentialError credentialError_ = AuroraCredentialError::None;
    std::size_t cacheMissCount_ = 0;
};
