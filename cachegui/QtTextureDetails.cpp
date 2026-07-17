#include "QtTextureDetails.h"

#include "TextureCacheDatabase.h"

#include <QDateTime>
#include <QStringList>

namespace
{
    QString FormatByteCount(std::uint64_t bytes)
    {
        constexpr std::uint64_t Kibibyte = 1024;
        constexpr std::uint64_t Mebibyte = Kibibyte * 1024;

        if (bytes >= Mebibyte)
        {
            return QStringLiteral("%1 MiB")
                .arg(static_cast<double>(bytes) / Mebibyte, 0, 'f', 1);
        }

        if (bytes >= Kibibyte)
        {
            return QStringLiteral("%1 KiB")
                .arg(static_cast<double>(bytes) / Kibibyte, 0, 'f', 1);
        }

        return QStringLiteral("%1 B").arg(bytes);
    }

    QString FormatTimestamp(std::time_t timestamp)
    {
        if (timestamp <= 0)
        {
            return QStringLiteral("Time unavailable");
        }

        return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(timestamp))
            .toLocalTime()
            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
}

QString TextureDetailsText(
    const CacheEntry& entry,
    std::uint32_t width,
    std::uint32_t height)
{
    QStringList details;

    if (width > 0 && height > 0)
    {
        details.push_back(
            QStringLiteral("%1 x %2 pixels").arg(width).arg(height));
    }

    const std::uint64_t advertisedBytes = entry.imageSize > 0
        ? static_cast<std::uint64_t>(entry.imageSize)
        : 0;
    details.push_back(
        QStringLiteral("Cached %1 of %2 | %3")
            .arg(FormatByteCount(CachedTextureByteCount(entry)))
            .arg(FormatByteCount(advertisedBytes))
            .arg(HasCompleteCachedTexture(entry)
                ? QStringLiteral("Complete")
                : QStringLiteral("Partial")));
    details.push_back(FormatTimestamp(entry.timestamp));

    return details.join(QStringLiteral("\n"));
}
