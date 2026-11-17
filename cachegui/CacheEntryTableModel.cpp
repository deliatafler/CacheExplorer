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

void CacheEntryTableModel::SetSource(const TextureCacheSource* source)
{
    beginResetModel();
    source_ = source;
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
            {
                Qt::DisplayRole,
                Qt::DecorationRole,
                Qt::UserRole,
                PreviewStateRole,
                CachedCompleteRole});
    }
}

void CacheEntryTableModel::NotifyPreviewStatusChanged(
    const std::string& sourceKey)
{
    const int row = RowForSourceKey(sourceKey);

    if (row >= 0)
    {
        emit dataChanged(
            index(row, 0),
            index(row, 5),
            {
                Qt::DisplayRole,
                Qt::DecorationRole,
                Qt::UserRole,
                PreviewStateRole,
                CachedCompleteRole});
    }
}

int CacheEntryTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || source_ == nullptr)
    {
        return 0;
    }

    return static_cast<int>(source_->Entries().size());
}

int CacheEntryTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 6;
}

QVariant CacheEntryTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || source_ == nullptr)
    {
        return {};
    }

    const TextureCatalogEntry* entry = EntryAt(index.row());

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
                return display == nullptr ? QString{} : display->identifier;

            case 1:
                return entry->expectedBytes.has_value()
                    ? QVariant::fromValue(
                        static_cast<qulonglong>(*entry->expectedBytes))
                    : QVariant{};

            case 2:
                return entry->payloadBytes.has_value()
                    ? QVariant::fromValue(
                        static_cast<qulonglong>(*entry->payloadBytes))
                    : QVariant{};

            case 3:
                return entry->sourceIndex.has_value()
                    ? QVariant::fromValue(
                        static_cast<qulonglong>(*entry->sourceIndex))
                    : QVariant{};

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
                return display == nullptr ? QString{} : display->identifier;

            case 1:
                return entry->expectedBytes.has_value()
                    ? QVariant::fromValue(
                        static_cast<qulonglong>(*entry->expectedBytes))
                    : QVariant{};

            case 2:
                return entry->payloadBytes.has_value()
                    ? QVariant::fromValue(
                        static_cast<qulonglong>(*entry->payloadBytes))
                    : QVariant{};

            case 3:
                return entry->sourceIndex.has_value()
                    ? QVariant::fromValue(
                        static_cast<qulonglong>(*entry->sourceIndex))
                    : QVariant{};

            case 4:
                return static_cast<qlonglong>(entry->timestamp);

            case 5:
                return PreviewStatusRank(*entry);

            default:
                return {};
        }
    }

    if (role == PreviewStateRole)
    {
        return PreviewStateValue(*entry);
    }

    if (role == CachedCompleteRole)
    {
        return entry->HasCompletePayload();
    }

    if (role == CacheIndexRole)
    {
        return entry->sourceIndex.has_value()
            ? QVariant::fromValue(
                static_cast<qulonglong>(*entry->sourceIndex))
            : QVariant{};
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

const TextureCatalogEntry* CacheEntryTableModel::EntryAt(int row) const
{
    if (source_ == nullptr || row < 0)
    {
        return nullptr;
    }

    const auto& entries = source_->Entries();
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

    if (source_ == nullptr)
    {
        return;
    }

    const auto& entries = source_->Entries();
    displayCache_.reserve(entries.size());

    for (const TextureCatalogEntry& entry : entries)
    {
        const std::string identifier = entry.uuid.has_value()
            ? entry.uuid->ToString()
            : entry.sourceKey;
        displayCache_.push_back(
            RowDisplayCache{
                ToQString(identifier),
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

int CacheEntryTableModel::RowForEntry(
    const TextureCatalogEntry& entry) const
{
    return RowForSourceKey(entry.sourceKey);
}

QString CacheEntryTableModel::PreviewStatusText(
    const TextureCatalogEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return {};
    }

    return previewCache_->StatusText(entry);
}

int CacheEntryTableModel::PreviewStatusRank(
    const TextureCatalogEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return 0;
    }

    return previewCache_->StatusRank(entry);
}

int CacheEntryTableModel::PreviewStateValue(
    const TextureCatalogEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return static_cast<int>(PreviewState::Unknown);
    }

    const PreviewRecord* record = previewCache_->Find(entry);

    if (record == nullptr)
    {
        return static_cast<int>(PreviewState::Unknown);
    }

    return static_cast<int>(record->state);
}

QVariant CacheEntryTableModel::PreviewIcon(
    const TextureCatalogEntry& entry) const
{
    if (previewCache_ == nullptr)
    {
        return QIcon(PlaceholderPixmap(
            QColor(38, 40, 43),
            QColor(82, 88, 96),
            QStringLiteral("")));
    }

    const PreviewRecord* record = previewCache_->Find(entry);

    if (record == nullptr)
    {
        return QIcon(PlaceholderPixmap(
            QColor(38, 40, 43),
            QColor(82, 88, 96),
            QStringLiteral("")));
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
                QColor(38, 40, 43),
                QColor(82, 88, 96),
                QStringLiteral("")));

        case PreviewState::Checking:
            return QIcon(PlaceholderPixmap(
                QColor(38, 40, 43),
                QColor(82, 88, 96),
                QStringLiteral("")));

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
                QColor(38, 40, 43),
                QColor(82, 88, 96),
                QStringLiteral("")));
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
    painter.setPen(QPen(foreground, 2));
    painter.drawRoundedRect(QRect(10, 10, 108, 108), 4, 4);

    if (label.isEmpty())
    {
        return pixmap;
    }

    painter.fillRect(QRect(0, 86, 128, 42), background.darker(125));
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

int CacheEntryTableModel::RowForSourceKey(
    const std::string& sourceKey) const
{
    if (source_ == nullptr)
    {
        return -1;
    }

    const auto& entries = source_->Entries();

    for (std::size_t row = 0; row < entries.size(); ++row)
    {
        if (entries[row].sourceKey == sourceKey)
        {
            return static_cast<int>(row);
        }
    }

    return -1;
}
