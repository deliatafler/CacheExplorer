#pragma once

#include <cstdint>

#include <QPixmap>
#include <QString>

class CacheEntryTableModel;
class PreviewCache;
struct CacheEntry;

void ClearPreviewStatuses(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel);

void SetPreviewChecking(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry);

void SetPreviewUnavailable(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const QString& message);

void SetPreviewLoadFailed(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const QString& message);

void SetPreviewable(
    PreviewCache& previewCache,
    CacheEntryTableModel& tableModel,
    const CacheEntry& entry,
    const QPixmap& pixmap,
    std::uint32_t width,
    std::uint32_t height);
