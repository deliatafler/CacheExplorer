#include "CacheEntryTableModel.h"

#include "PreviewCache.h"

#include <algorithm>
#include <ctime>
#include <string>

#include <QDateTime>
#include <QIcon>
#include <QPainter>

namespace
{
    QString ToQString(const std::string& value)
    {
        return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
    }

    QString FormatTime(std::time_t timestamp)
    {
        if (timestamp <= 0)
        {
            return {};
        }

        return QDateTime::fromSecsSinceEpoch(
                static_cast<qint64>(timestamp))
            .toLocalTime()
            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
}

CacheEntryTableModel::CacheEntryTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void CacheEntryTableModel::SetDatabase(const TextureCacheDatabase* database)
{
    beginResetModel();
    database_ = database;
    RebuildDisplayCache();
    endResetModel();
}

void CacheEntryTableModel::SetPreviewCache(const PreviewCache* previewCache)
{
    beginResetModel();
    previewCache_ = previewCache;
    endResetModel();
}

void CacheEntryTableModel::NotifyAllPreviewStatusesChanged()
{
    if (rowCount() > 0)
    {
        emit dataChanged(
            index(0, 0),
            index(rowCount() - 1, 5),
            {Qt::DisplayRole, Qt::DecorationRole, Qt::UserRole});
    }
}

void CacheEntryTableModel::NotifyPreviewStatusChanged(std::uint32_t cacheIndex)
{
    const int row = RowForCacheIndex(cacheIndex);

    if (row >= 0)
    {
        emit dataChanged(
            index(row, 0),
            index(row, 5),
            {Qt::DisplayRole, Qt::DecorationRole, Qt::UserRole});
    }
}

int CacheEntryTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || database_ == nullptr)
    {
        return 0;
    }

    return static_cast<int>(database_->Entries().size());
}

int CacheEntryTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 6;
}

QVariant CacheEntryTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || database_ == nullptr)
    {
        return {};
    }

    const CacheEntry* entry = EntryAt(index.row());

    if (entry == nullptr)
    {
        return {};
    }

    if (role == Qt::DisplayRole)
    {
        const RowDisplayCache* display = DisplayCacheAt(index.row());

        switch (index.column())
        {
            case 0:
                return display == nullptr ? QString{} : display->uuid;

            case 1:
                return entry->imageSize;

            case 2:
                return entry->bodySize;

            case 3:
                return static_cast<qulonglong>(entry->cacheIndex);

            case 4:
                return display == nullptr ? QString{} : display->timestamp;

            case 5:
                return PreviewStatusText(*entry);

            default:
                return {};
        }
    }

    if (role == Qt::UserRole)
    {
        const RowDisplayCache* display = DisplayCacheAt(index.row());

        switch (index.column())
        {
            case 0:
                return display == nullptr ? QString{} : display->uuid;

            case 1:
                return entry->imageSize;

            case 2:
                return entry->bodySize;

            case 3:
                return static_cast<qulonglong>(entry->cacheIndex);

            case 4:
                return static_cast<qlonglong>(entry->timestamp);

            case 5:
                return PreviewStatusRank(*entry);

            default:
                return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == 0)
    {
        return PreviewIcon(*entry);
    }

    return {};
}

QVariant CacheEntryTableModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    switch (section)
    {
        case 0:
            return QStringLiteral("UUID");

        case 1:
            return QStringLiteral("Image");

        case 2:
            return QStringLiteral("Body");

        case 3:
            return QStringLiteral("Cache index");

        case 4:
            return QStringLiteral("Timestamp");

        case 5:
            return QStringLiteral("Preview");

        default:
            return {};
    }
}

const CacheEntry* CacheEntryTableModel::EntryAt(int row) const
{
    if (database_ == nullptr || row < 0)
    {
        return nullptr;
    }

    const auto& entries = database_->Entries();
    const auto index = static_cast<std::size_t>(row);

    if (index >= entries.size())
    {
        return nullptr;
    }

    return &entries[index];
}

void CacheEntryTableModel::RebuildDisplayCache()
{
    displayCache_.clear();

    if (database_ == nullptr)
    {
        return;
    }

    const auto& entries = database_->Entries();
    displayCache_.reserve(entries.size());

    for (const CacheEntry& entry : entries)
    {
        displayCache_.push_back(
            RowDisplayCache{
                ToQString(entry.uuid.ToString()),
                FormatTime(entry.timestamp)});
    }
}

const CacheEntryTableModel::RowDisplayCache*
CacheEntryTableModel::DisplayCacheAt(int row) const
{
    if (row < 0)
    {
        return nullptr;
    }

    const auto index = static_cast<std::size_t>(row);

    if (index >= displayCache_.size())
    {
        return nullptr;
    }

    return &displayCache_[index];
}

int CacheEntryTableModel::RowForEntry(const CacheEntry& entry) const
{
    return RowForCacheIndex(entry.cacheIndex);
}

QString CacheEntryTableModel::PreviewStatusText(const CacheEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return {};
    }

    return previewCache_->StatusText(entry);
}

int CacheEntryTableModel::PreviewStatusRank(const CacheEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return 0;
    }

    return previewCache_->StatusRank(entry);
}

QVariant CacheEntryTableModel::PreviewIcon(const CacheEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return QIcon(PlaceholderPixmap(
            QColor(44, 47, 51),
            QColor(112, 119, 128),
            QStringLiteral("WAIT")));
    }

    const PreviewRecord* record = previewCache_->Find(entry);

    if (record == nullptr)
    {
        return QIcon(PlaceholderPixmap(
            QColor(44, 47, 51),
            QColor(112, 119, 128),
            QStringLiteral("WAIT")));
    }

    if (record->state == PreviewState::Previewable && !record->pixmap.isNull())
    {
        return QIcon(record->pixmap.scaled(
            128,
            128,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    }

    switch (record->state)
    {
        case PreviewState::Unknown:
            return QIcon(PlaceholderPixmap(
                QColor(44, 47, 51),
                QColor(112, 119, 128),
                QStringLiteral("WAIT")));

        case PreviewState::Checking:
            return QIcon(PlaceholderPixmap(
                QColor(30, 60, 95),
                QColor(108, 166, 222),
                QStringLiteral("LOAD")));

        case PreviewState::Unavailable:
            return QIcon(PlaceholderPixmap(
                QColor(54, 45, 45),
                QColor(154, 130, 130),
                QStringLiteral("NO PREVIEW")));

        case PreviewState::LoadFailed:
            return QIcon(PlaceholderPixmap(
                QColor(64, 42, 42),
                QColor(207, 116, 116),
                QStringLiteral("ERR")));

        case PreviewState::Previewable:
            return QIcon(PlaceholderPixmap(
                QColor(44, 47, 51),
                QColor(112, 119, 128),
                QStringLiteral("WAIT")));
    }

    return {};
}

QPixmap CacheEntryTableModel::PlaceholderPixmap(
    const QColor& background,
    const QColor& foreground,
    const QString& label) const
{
    QPixmap pixmap(
        128,
        128);
    pixmap.fill(background);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(QRect(0, 86, 128, 42), background.darker(125));
    painter.setPen(QPen(foreground, 2));
    painter.drawRoundedRect(QRect(10, 10, 108, 108), 4, 4);
    painter.setPen(foreground);
    QFont labelFont = painter.font();
    labelFont.setPointSize(label.size() > 4 ? 12 : 18);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.drawText(
        QRect(8, 84, 112, 38),
        Qt::AlignCenter,
        label);
    return pixmap;
}

int CacheEntryTableModel::RowForCacheIndex(std::uint32_t cacheIndex) const
{
    if (database_ == nullptr)
    {
        return -1;
    }

    const auto& entries = database_->Entries();

    for (std::size_t row = 0; row < entries.size(); ++row)
    {
        if (entries[row].cacheIndex == cacheIndex)
        {
            return static_cast<int>(row);
        }
    }

    return -1;
}
