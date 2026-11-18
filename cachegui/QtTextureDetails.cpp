#include "QtTextureDetails.h"

#include "TextureCacheSource.h"

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

    QString FormatCacheData(const TextureCatalogEntry& entry)
    {
        QStringList formats;

        if (entry.hasCompleteJpeg2000)
        {
            formats.push_back(QStringLiteral("Complete JPEG2000"));
        }

        if (entry.hasPartialJpeg2000)
        {
            formats.push_back(QStringLiteral("partial JPEG2000"));
        }

        if (entry.hasBc7)
        {
            formats.push_back(
                formats.empty()
                    ? QStringLiteral("BC7 only")
                    : QStringLiteral("BC7"));
        }

        return formats.empty()
            ? QString()
            : QStringLiteral("Cache data: %1")
                .arg(formats.join(QStringLiteral(" + ")));
    }
}

QString TextureDetailsText(
    const TextureCatalogEntry& entry,
    std::uint32_t width,
    std::uint32_t height)
{
    QStringList details;

    if (width > 0 && height > 0)
    {
        details.push_back(
            QStringLiteral("%1 x %2 pixels").arg(width).arg(height));
    }

    const QString cacheData = FormatCacheData(entry);
    if (!cacheData.isEmpty())
    {
        details.push_back(cacheData);
    }

    if (entry.expectedBytes.has_value())
    {
        details.push_back(
            QStringLiteral("Cached %1 of %2 | %3")
                .arg(FormatByteCount(entry.cachedBytes))
                .arg(FormatByteCount(*entry.expectedBytes))
                .arg(entry.HasCompletePayload()
                    ? QStringLiteral("Complete")
                    : QStringLiteral("Partial")));
    }
    else
    {
        details.push_back(
            QStringLiteral("Cached %1 | Complete size unavailable")
                .arg(FormatByteCount(entry.cachedBytes)));
    }
    details.push_back(FormatTimestamp(entry.timestamp));

    return details.join(QStringLiteral("\n"));
}
