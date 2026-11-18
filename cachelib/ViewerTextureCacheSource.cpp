#include "ViewerTextureCacheSource.h"

#include "AuroraCacheProbe.h"

#include <system_error>
#include <utility>

namespace
{
    const std::filesystem::path& EmptyPath()
    {
        static const std::filesystem::path empty;
        return empty;
    }

    const std::vector<TextureCatalogEntry>& EmptyEntries()
    {
        static const std::vector<TextureCatalogEntry> empty;
        return empty;
    }

    bool HasLegacyEntryTable(
        const std::filesystem::path& cacheDirectory)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(
                   cacheDirectory / "texture.entries",
                   error) &&
            !error;
    }
}

ViewerTextureCacheSource::ViewerTextureCacheSource() = default;

ViewerTextureCacheSource::ViewerTextureCacheSource(
    std::shared_ptr<const AuroraCacheKeyProvider> keyProvider)
    : auroraSource_(std::move(keyProvider))
{
}

ViewerTextureCacheSource::ViewerTextureCacheSource(
    ViewerTextureCacheSource&& other) noexcept
    : legacySource_(std::move(other.legacySource_)),
      auroraSource_(std::move(other.auroraSource_))
{
    if (other.activeSource_ == &other.legacySource_)
    {
        activeSource_ = &legacySource_;
    }
    else if (other.activeSource_ == &other.auroraSource_)
    {
        activeSource_ = &auroraSource_;
    }

    other.activeSource_ = nullptr;
}

ViewerTextureCacheSource& ViewerTextureCacheSource::operator=(
    ViewerTextureCacheSource&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    const bool legacyActive =
        other.activeSource_ == &other.legacySource_;
    const bool auroraActive =
        other.activeSource_ == &other.auroraSource_;

    activeSource_ = nullptr;
    legacySource_ = std::move(other.legacySource_);
    auroraSource_ = std::move(other.auroraSource_);

    if (legacyActive)
    {
        activeSource_ = &legacySource_;
    }
    else if (auroraActive)
    {
        activeSource_ = &auroraSource_;
    }

    other.activeSource_ = nullptr;
    return *this;
}

TextureCacheOpenError ViewerTextureCacheSource::Open(
    const std::filesystem::path& cacheDirectory)
{
    Close();

    if (HasLegacyEntryTable(cacheDirectory))
    {
        const TextureCacheOpenError error =
            legacySource_.Open(cacheDirectory);
        if (error == TextureCacheOpenError::None)
        {
            activeSource_ = &legacySource_;
        }
        return error;
    }

    AuroraTextureStoreProbe probe;
    const TextureCacheOpenError probeError =
        ProbeAuroraTextureStore(cacheDirectory, probe);
    if (probeError == TextureCacheOpenError::None)
    {
        const TextureCacheOpenError error =
            auroraSource_.Open(cacheDirectory);
        if (error == TextureCacheOpenError::None)
        {
            activeSource_ = &auroraSource_;
        }
        return error;
    }

    return legacySource_.Open(cacheDirectory);
}

void ViewerTextureCacheSource::Close()
{
    activeSource_ = nullptr;
    legacySource_.Close();
    auroraSource_.Close();
}

bool ViewerTextureCacheSource::IsOpen() const
{
    return activeSource_ != nullptr && activeSource_->IsOpen();
}

TextureCacheFormat ViewerTextureCacheSource::Format() const
{
    return activeSource_ != nullptr
        ? activeSource_->Format()
        : TextureCacheFormat::LegacyViewer;
}

const std::filesystem::path&
ViewerTextureCacheSource::CacheDirectory() const
{
    return activeSource_ != nullptr
        ? activeSource_->CacheDirectory()
        : EmptyPath();
}

const std::vector<TextureCatalogEntry>&
ViewerTextureCacheSource::Entries() const
{
    return activeSource_ != nullptr
        ? activeSource_->Entries()
        : EmptyEntries();
}

const TextureCatalogEntry* ViewerTextureCacheSource::FindByUuid(
    const UUID& uuid) const
{
    return activeSource_ != nullptr
        ? activeSource_->FindByUuid(uuid)
        : nullptr;
}

TexturePayloadError ViewerTextureCacheSource::ReadPayload(
    const std::string& sourceKey,
    TexturePayload& output) const
{
    if (activeSource_ == nullptr)
    {
        output = {};
        return TexturePayloadError::SourceNotOpen;
    }

    return activeSource_->ReadPayload(sourceKey, output);
}

std::unique_ptr<TexturePayloadRequest>
ViewerTextureCacheSource::CreatePayloadRequest(
    const std::string& sourceKey) const
{
    return activeSource_ != nullptr
        ? activeSource_->CreatePayloadRequest(sourceKey)
        : nullptr;
}

const LegacyTextureCacheSource*
ViewerTextureCacheSource::LegacySource() const
{
    return activeSource_ == &legacySource_
        ? &legacySource_
        : nullptr;
}

const AuroraTextureCacheSource*
ViewerTextureCacheSource::AuroraSource() const
{
    return activeSource_ == &auroraSource_
        ? &auroraSource_
        : nullptr;
}
