#pragma once

#include <cstdint>
#include <unordered_map>

#include <QPixmap>
#include <QString>

struct CacheEntry;

enum class PreviewState
{
    Unknown,
    Checking,
    Previewable,
    Unavailable,
    LoadFailed
};

struct PreviewRecord
{
    PreviewState state = PreviewState::Unknown;
    QString message;
    QPixmap pixmap;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class PreviewCache
{
public:
    void Clear();
    void SetChecking(const CacheEntry& entry);
    void SetUnavailable(const CacheEntry& entry, const QString& message);
    void SetLoadFailed(const CacheEntry& entry, const QString& message);
    void SetPreviewable(
        const CacheEntry& entry,
        const QPixmap& pixmap,
        std::uint32_t width,
        std::uint32_t height);

    const PreviewRecord* Find(const CacheEntry& entry) const;
    bool ShouldAttemptPreview(const CacheEntry& entry) const;
    QString StatusText(const CacheEntry& entry) const;
    int StatusRank(const CacheEntry& entry) const;

private:
    std::unordered_map<std::uint32_t, PreviewRecord> records_;
};
