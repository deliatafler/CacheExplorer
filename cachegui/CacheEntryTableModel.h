#pragma once

#include "TextureCacheSource.h"

#include <string>
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
    static constexpr int CachedCompleteRole = Qt::UserRole + 2;
    static constexpr int CacheIndexRole = Qt::UserRole + 3;

    explicit CacheEntryTableModel(QObject* parent = nullptr);

    void SetSource(const TextureCacheSource* source);
    void SetPreviewCache(const PreviewCache* previewCache);
    void NotifyAllPreviewStatusesChanged();
    void NotifyPreviewStatusChanged(const std::string& sourceKey);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role) const override;

    const TextureCatalogEntry* EntryAt(int row) const;
    int RowForEntry(const TextureCatalogEntry& entry) const;

private:
    struct RowDisplayCache
    {
        QString identifier;
        QString timestamp;
    };

    void RebuildDisplayCache();
    const RowDisplayCache* DisplayCacheAt(int row) const;
    int PreviewStateValue(const TextureCatalogEntry& entry) const;
    QString PreviewStatusText(const TextureCatalogEntry& entry) const;
    int PreviewStatusRank(const TextureCatalogEntry& entry) const;
    QVariant PreviewIcon(const TextureCatalogEntry& entry) const;
    QPixmap PlaceholderPixmap(
        const QColor& background,
        const QColor& foreground,
        const QString& label) const;
    int RowForSourceKey(const std::string& sourceKey) const;

    const TextureCacheSource* source_ = nullptr;
    const PreviewCache* previewCache_ = nullptr;
    std::vector<RowDisplayCache> displayCache_;
};
