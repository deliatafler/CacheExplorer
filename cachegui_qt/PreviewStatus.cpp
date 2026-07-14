#include "PreviewStatus.h"

#include "CacheEntryTableModel.h"
#include "PreviewImage.h"
#include "PreviewCache.h"
#include "TextureCacheDatabase.h"

void ClearPreviewStatuses(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel)
{
    previewCache.Clear();
    tableModel.NotifyAllPreviewStatusesChanged();
}

void SetPreviewChecking(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry)
{
    previewCache.SetChecking(entry);
    tableModel.NotifyPreviewStatusChanged(entry.cacheIndex);
}

void SetPreviewUnavailable(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const QString& message)
{
    previewCache.SetUnavailable(entry, message);
    tableModel.NotifyPreviewStatusChanged(entry.cacheIndex);
}

void SetPreviewLoadFailed(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const QString& message)
{
    previewCache.SetLoadFailed(entry, message);
    tableModel.NotifyPreviewStatusChanged(entry.cacheIndex);
}

void SetPreviewable(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const QPixmap& pixmap,
    std::uint32_t width,
    std::uint32_t height)
{
    previewCache.SetPreviewable(
        entry,
        pixmap,
        width,
        height);
    tableModel.NotifyPreviewStatusChanged(entry.cacheIndex);
}

bool StoreDecodedPreview(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const DecodedImage& decodedImage,
    QPixmap& pixmap,
    QString& errorMessage)
{
    if (!CreatePreviewPixmap(decodedImage, pixmap, errorMessage))
    {
        SetPreviewLoadFailed(
            previewCache,
            tableModel,
            entry,
            errorMessage);
        return false;
    }

    SetPreviewable(
        previewCache,
        tableModel,
        entry,
        pixmap,
        decodedImage.width,
        decodedImage.height);
    return true;
}
