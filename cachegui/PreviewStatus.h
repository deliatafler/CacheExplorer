#pragma once

#include <cstdint>

#include <QPixmap>
#include <QString>

class CacheEntryTableModel;
struct DecodedImage;
class PreviewCache;
struct TextureCatalogEntry;

void ClearPreviewStatuses(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel);

void SetPreviewChecking(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const TextureCatalogEntry& entry);

void SetPreviewUnavailable(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const TextureCatalogEntry& entry,
    const QString& message);

void SetPreviewLoadFailed(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const TextureCatalogEntry& entry,
    const QString& message);

void SetPreviewable(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const TextureCatalogEntry& entry,
    const QPixmap& pixmap,
    std::uint32_t width,
    std::uint32_t height);

bool StoreDecodedPreview(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const TextureCatalogEntry& entry,
    const DecodedImage& decodedImage,
    QPixmap& pixmap,
    QString& errorMessage);
