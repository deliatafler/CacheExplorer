#include "PreviewCache.h"

#include "TextureCacheSource.h"

void PreviewCache::Clear()
{
    records_.clear();
}

void PreviewCache::SetChecking(const TextureCatalogEntry& entry)
{
    PreviewRecord record;
    record.state = PreviewState::Checking;
    records_[entry.sourceKey] = record;
}

void PreviewCache::SetUnavailable(
    const TextureCatalogEntry& entry,
    const QString& message)
{
    PreviewRecord record;
    record.state = PreviewState::Unavailable;
    record.message = message;
    records_[entry.sourceKey] = record;
}

void PreviewCache::SetLoadFailed(
    const TextureCatalogEntry& entry,
    const QString& message)
{
    PreviewRecord record;
    record.state = PreviewState::LoadFailed;
    record.message = message;
    records_[entry.sourceKey] = record;
}

void PreviewCache::SetPreviewable(
    const TextureCatalogEntry& entry,
    const QPixmap& pixmap,
    std::uint32_t width,
    std::uint32_t height)
{
    PreviewRecord record;
    record.state = PreviewState::Previewable;
    record.pixmap = pixmap;
    record.width = width;
    record.height = height;
    records_[entry.sourceKey] = record;
}

const PreviewRecord* PreviewCache::Find(
    const TextureCatalogEntry& entry) const
{
    const auto record = records_.find(entry.sourceKey);

    if (record == records_.end())
    {
        return nullptr;
    }

    return &record->second;
}

bool PreviewCache::ShouldAttemptPreview(
    const TextureCatalogEntry& entry) const
{
    const PreviewRecord* record = Find(entry);
    return record == nullptr || record->state == PreviewState::Unknown;
}

QString PreviewCache::StatusText(const TextureCatalogEntry& entry) const
{
    const PreviewRecord* record = Find(entry);

    if (record == nullptr)
    {
        return {};
    }

    switch (record->state)
    {
        case PreviewState::Unknown:
            return {};

        case PreviewState::Checking:
            return QStringLiteral("Checking");

        case PreviewState::Previewable:
            return QStringLiteral("Preview");

        case PreviewState::Unavailable:
            return QStringLiteral("No preview");

        case PreviewState::LoadFailed:
            return QStringLiteral("Load failed");
    }

    return {};
}

int PreviewCache::StatusRank(const TextureCatalogEntry& entry) const
{
    const PreviewRecord* record = Find(entry);

    if (record == nullptr)
    {
        return 0;
    }

    switch (record->state)
    {
        case PreviewState::Unknown:
            return 0;

        case PreviewState::Checking:
            return 1;

        case PreviewState::Unavailable:
        case PreviewState::LoadFailed:
            return 2;

        case PreviewState::Previewable:
            return 3;
    }

    return 0;
}

std::vector<StoredPreviewRecord> PreviewCache::TerminalRecords() const
{
    std::vector<StoredPreviewRecord> records;

    for (const auto& item : records_)
    {
        const PreviewRecord& record = item.second;

        if (record.state != PreviewState::Unavailable &&
            record.state != PreviewState::LoadFailed)
        {
            continue;
        }

        records.push_back(
            StoredPreviewRecord{
                item.first,
                record.state,
                record.message});
    }

    return records;
}
