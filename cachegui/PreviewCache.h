#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <QPixmap>
#include <QString>

struct TextureCatalogEntry;

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

struct StoredPreviewRecord
{
    std::string sourceKey;
    PreviewState state = PreviewState::Unknown;
    QString message;
};

class PreviewCache
{
public:
    void Clear();
    void SetChecking(const TextureCatalogEntry& entry);
    void SetUnavailable(
        const TextureCatalogEntry& entry,
        const QString& message);
    void SetLoadFailed(
        const TextureCatalogEntry& entry,
        const QString& message);
    void SetPreviewable(
        const TextureCatalogEntry& entry,
        const QPixmap& pixmap,
        std::uint32_t width,
        std::uint32_t height);

    const PreviewRecord* Find(const TextureCatalogEntry& entry) const;
    bool ShouldAttemptPreview(const TextureCatalogEntry& entry) const;
    QString StatusText(const TextureCatalogEntry& entry) const;
    int StatusRank(const TextureCatalogEntry& entry) const;
    std::vector<StoredPreviewRecord> TerminalRecords() const;

private:
    std::unordered_map<std::string, PreviewRecord> records_;
};
