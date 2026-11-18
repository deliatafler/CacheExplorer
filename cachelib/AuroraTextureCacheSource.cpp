#include "AuroraTextureCacheSource.h"

#include "AuroraSealedFile.h"
#include "AuroraTextureKey.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace
{
    struct PayloadCandidate
    {
        std::filesystem::path path;
        TexturePayloadFormat format = TexturePayloadFormat::Jpeg2000;
    };

    TexturePayloadError MapFormatError(AuroraFormatError error)
    {
        switch (error)
        {
            case AuroraFormatError::None:
                return TexturePayloadError::None;

            case AuroraFormatError::UnsupportedVersion:
                return TexturePayloadError::UnsupportedFormat;

            case AuroraFormatError::ReadError:
            case AuroraFormatError::CryptoError:
                return TexturePayloadError::ReadError;

            case AuroraFormatError::FileNotFound:
            case AuroraFormatError::InvalidFormat:
            case AuroraFormatError::SizeMismatch:
            case AuroraFormatError::InvalidMetadata:
            case AuroraFormatError::AuthenticationFailed:
                return TexturePayloadError::MissingData;
        }

        return TexturePayloadError::ReadError;
    }

    class AuroraTexturePayloadRequest final : public TexturePayloadRequest
    {
    public:
        AuroraTexturePayloadRequest(
            std::shared_ptr<const AuroraCacheKey> key,
            std::vector<PayloadCandidate> candidates)
            : key_(std::move(key)),
              candidates_(std::move(candidates))
        {
        }

        TexturePayloadError Read(TexturePayload& output) const override
        {
            output = {};

            if (!key_ || !key_->IsValid())
            {
                return TexturePayloadError::SourceNotOpen;
            }

            TexturePayloadError bestError =
                TexturePayloadError::MissingData;

            for (const PayloadCandidate& candidate : candidates_)
            {
                std::vector<std::uint8_t> bytes;
                const AuroraFormatError error =
                    OpenAuroraSealedFile(
                        candidate.path,
                        *key_,
                        bytes);

                if (error == AuroraFormatError::None)
                {
                    output.format = candidate.format;
                    output.bytes = std::move(bytes);
                    return TexturePayloadError::None;
                }

                const TexturePayloadError mapped =
                    MapFormatError(error);

                if (mapped == TexturePayloadError::ReadError ||
                    (mapped == TexturePayloadError::UnsupportedFormat &&
                     bestError == TexturePayloadError::MissingData))
                {
                    bestError = mapped;
                }
            }

            return bestError;
        }

    private:
        std::shared_ptr<const AuroraCacheKey> key_;
        std::vector<PayloadCandidate> candidates_;
    };

    void AddPayloadCandidates(
        const AuroraTextureRecord& record,
        std::vector<PayloadCandidate>& candidates)
    {
        if (record.completeJpeg2000)
        {
            candidates.push_back({
                record.completeJpeg2000->path,
                TexturePayloadFormat::Jpeg2000});
        }

        if (record.partialJpeg2000)
        {
            candidates.push_back({
                record.partialJpeg2000->path,
                TexturePayloadFormat::Jpeg2000});
        }

        if (record.transcodedBc7)
        {
            candidates.push_back({
                record.transcodedBc7->path,
                TexturePayloadFormat::Bc7});
        }
    }

    const AuroraTextureFile* PreferredFile(
        const AuroraTextureRecord& record)
    {
        if (record.completeJpeg2000)
        {
            return &*record.completeJpeg2000;
        }

        if (record.partialJpeg2000)
        {
            return &*record.partialJpeg2000;
        }

        return record.transcodedBc7
            ? &*record.transcodedBc7
            : nullptr;
    }

    std::time_t MostRecentTime(
        const AuroraTextureRecord& record)
    {
        std::time_t timestamp = 0;

        if (record.completeJpeg2000)
        {
            timestamp = std::max(
                timestamp,
                record.completeJpeg2000->modifiedTime);
        }

        if (record.partialJpeg2000)
        {
            timestamp = std::max(
                timestamp,
                record.partialJpeg2000->modifiedTime);
        }

        if (record.transcodedBc7)
        {
            timestamp = std::max(
                timestamp,
                record.transcodedBc7->modifiedTime);
        }

        return timestamp;
    }
}

AuroraTextureCacheSource::AuroraTextureCacheSource()
    : AuroraTextureCacheSource(LoadAuroraCacheKey)
{
}

AuroraTextureCacheSource::AuroraTextureCacheSource(
    AuroraCacheKeyLoader keyLoader)
    : keyLoader_(std::move(keyLoader))
{
    if (!keyLoader_)
    {
        keyLoader_ = LoadAuroraCacheKey;
    }
}

TextureCacheOpenError AuroraTextureCacheSource::Open(
    const std::filesystem::path& cacheDirectory)
{
    Close();

    AuroraTextureStoreScan scan;
    const TextureCacheOpenError scanError =
        EnumerateAuroraTextureStore(cacheDirectory, scan);

    if (scanError != TextureCacheOpenError::None)
    {
        return scanError;
    }

    AuroraCacheKey loadedKey;
    credentialError_ = keyLoader_(loadedKey);

    if (credentialError_ != AuroraCredentialError::None ||
        !loadedKey.IsValid())
    {
        if (credentialError_ == AuroraCredentialError::None)
        {
            credentialError_ = AuroraCredentialError::InvalidKey;
        }

        return TextureCacheOpenError::CredentialsUnavailable;
    }

    key_ = std::make_shared<AuroraCacheKey>(
        std::move(loadedKey));
    cacheDirectory_ = std::move(scan.probe.cacheRoot);
    records_ = std::move(scan.records);
    cacheMissCount_ = scan.cacheMissCount;

    entries_.reserve(records_.size());
    entryIndices_.reserve(records_.size());
    recordIndices_.reserve(records_.size());

    for (std::size_t i = 0; i < records_.size(); ++i)
    {
        const AuroraTextureRecord& record = records_[i];
        const AuroraTextureFile* preferred =
            PreferredFile(record);

        if (preferred == nullptr)
        {
            continue;
        }

        TextureCatalogEntry entry;
        entry.sourceKey = record.sourceKey;
        entry.cachedBytes = preferred->plaintextBytes.value_or(
            preferred->sealedBytes);
        entry.payloadBytes = preferred->plaintextBytes;
        entry.timestamp = MostRecentTime(record);

        if (record.completeJpeg2000 ||
            (!record.partialJpeg2000 && record.transcodedBc7))
        {
            entry.expectedBytes = preferred->plaintextBytes;
        }

        entryIndices_.emplace(entry.sourceKey, entries_.size());
        recordIndices_.emplace(entry.sourceKey, i);
        entries_.push_back(std::move(entry));
    }

    open_ = true;
    return TextureCacheOpenError::None;
}

void AuroraTextureCacheSource::Close()
{
    open_ = false;
    cacheDirectory_.clear();
    key_.reset();
    records_.clear();
    entries_.clear();
    entryIndices_.clear();
    recordIndices_.clear();
    credentialError_ = AuroraCredentialError::None;
    cacheMissCount_ = 0;
}

const TextureCatalogEntry* AuroraTextureCacheSource::FindByUuid(
    const UUID& uuid) const
{
    if (!open_ || !key_)
    {
        return nullptr;
    }

    const std::optional<std::string> sourceKey =
        AuroraKeyedTextureName(*key_, uuid);

    return sourceKey
        ? FindBySourceKey(*sourceKey)
        : nullptr;
}

TexturePayloadError AuroraTextureCacheSource::ReadPayload(
    const std::string& sourceKey,
    TexturePayload& output) const
{
    output = {};

    if (!open_)
    {
        return TexturePayloadError::SourceNotOpen;
    }

    std::unique_ptr<TexturePayloadRequest> request =
        CreatePayloadRequest(sourceKey);

    if (!request)
    {
        return TexturePayloadError::EntryNotFound;
    }

    return request->Read(output);
}

std::unique_ptr<TexturePayloadRequest>
AuroraTextureCacheSource::CreatePayloadRequest(
    const std::string& sourceKey) const
{
    if (!open_ || !key_)
    {
        return {};
    }

    const auto recordIndex = recordIndices_.find(sourceKey);
    if (recordIndex == recordIndices_.end() ||
        recordIndex->second >= records_.size())
    {
        return {};
    }

    std::vector<PayloadCandidate> candidates;
    AddPayloadCandidates(
        records_[recordIndex->second],
        candidates);

    return std::make_unique<AuroraTexturePayloadRequest>(
        key_,
        std::move(candidates));
}

const TextureCatalogEntry*
AuroraTextureCacheSource::FindBySourceKey(
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
