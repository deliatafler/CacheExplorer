#pragma once

#include "AuroraTextureCacheSource.h"
#include "LegacyTextureCacheSource.h"
#include "TextureCacheSource.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class ViewerTextureCacheSource final : public TextureCacheSource
{
public:
    ViewerTextureCacheSource();
    explicit ViewerTextureCacheSource(
        std::shared_ptr<const AuroraCacheKeyProvider> keyProvider);

    ViewerTextureCacheSource(const ViewerTextureCacheSource&) = delete;
    ViewerTextureCacheSource& operator=(
        const ViewerTextureCacheSource&) = delete;

    ViewerTextureCacheSource(
        ViewerTextureCacheSource&& other) noexcept;
    ViewerTextureCacheSource& operator=(
        ViewerTextureCacheSource&& other) noexcept;

    TextureCacheOpenError Open(
        const std::filesystem::path& cacheDirectory) override;

    void Close() override;

    bool IsOpen() const override;
    TextureCacheFormat Format() const override;

    const std::filesystem::path& CacheDirectory() const override;
    const std::vector<TextureCatalogEntry>& Entries() const override;

    const TextureCatalogEntry* FindByUuid(
        const UUID& uuid) const override;

    TexturePayloadError ReadPayload(
        const std::string& sourceKey,
        TexturePayload& output) const override;

    std::unique_ptr<TexturePayloadRequest> CreatePayloadRequest(
        const std::string& sourceKey) const override;

    const LegacyTextureCacheSource* LegacySource() const;
    const AuroraTextureCacheSource* AuroraSource() const;

private:
    TextureCacheSource* activeSource_ = nullptr;
    LegacyTextureCacheSource legacySource_;
    AuroraTextureCacheSource auroraSource_;
};
