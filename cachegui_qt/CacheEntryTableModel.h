#pragma once

#include "TextureCacheDatabase.h"

#include <cstdint>
#include <vector>

#include <QAbstractTableModel>
#include <QColor>
#include <QPixmap>
#include <QString>
#include <QVariant>

class PreviewCache;

class CacheEntryTableModel final : public QAbstractTableModel
{
public:
    static constexpr int PreviewStateRole = Qt::UserRole + 1;

    explicit CacheEntryTableModel(QObject* parent = nullptr);

    void SetDatabase(const TextureCacheDatabase* database);
    void SetPreviewCache(const PreviewCache* previewCache);
    void NotifyAllPreviewStatusesChanged();
    void NotifyPreviewStatusChanged(std::uint32_t cacheIndex);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role) const override;

    const CacheEntry* EntryAt(int row) const;
    int RowForEntry(const CacheEntry& entry) const;

private:
    struct RowDisplayCache
    {
        QString uuid;
        QString timestamp;
    };

    void RebuildDisplayCache();
    const RowDisplayCache* DisplayCacheAt(int row) const;
    int PreviewStateValue(const CacheEntry& entry) const;
    QString PreviewStatusText(const CacheEntry& entry) const;
    int PreviewStatusRank(const CacheEntry& entry) const;
    QVariant PreviewIcon(const CacheEntry& entry) const;
    QPixmap PlaceholderPixmap(
        const QColor& background,
        const QColor& foreground,
        const QString& label) const;
    int RowForCacheIndex(std::uint32_t cacheIndex) const;

    const TextureCacheDatabase* database_ = nullptr;
    const PreviewCache* previewCache_ = nullptr;
    std::vector<RowDisplayCache> displayCache_;
};
