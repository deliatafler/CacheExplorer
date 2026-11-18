#include "QtAboutDialog.h"

#include "QtGalleryDiagnostics.h"
#include "QtHelpers.h"
#include "ViewerTextureCacheSource.h"

#include <QApplication>
#include <QString>
#include <QtGlobal>
#include <QMessageBox>

#ifndef CACHEEXPLORER_VERSION
#define CACHEEXPLORER_VERSION "unknown"
#endif

namespace
{
    QString CacheDiagnosticText(const ViewerTextureCacheSource& source)
    {
        if (!source.IsOpen())
        {
            return QStringLiteral("Cache: not open");
        }

        const LegacyTextureCacheSource* legacySource =
            source.LegacySource();
        if (legacySource == nullptr)
        {
            const AuroraTextureCacheSource* auroraSource =
                source.AuroraSource();
            return QStringLiteral(
                "Cache: %1\n"
                "Cache format: Aurora (experimental)\n"
                "Texture records: %2\n"
                "Files changed during scan: %3\n"
                "Cache key source: %4")
                .arg(PathToQString(source.CacheDirectory()))
                .arg(source.Entries().size())
                .arg(auroraSource != nullptr
                    ? auroraSource->CacheMissCount()
                    : 0)
                .arg(QString::fromLatin1(
                    auroraSource != nullptr
                        ? AuroraCredentialSourceName(
                            auroraSource->CredentialSource())
                        : "Unavailable"));
        }

        const TextureCacheDatabase& database =
            legacySource->LegacyDatabase();
        const CacheHeader& header = database.Header();

        return QStringLiteral(
            "Cache: %1\n"
            "Valid entries: %2\n"
            "Entry slots: %3\n"
            "Cache version: %4\n"
            "Address size: %5\n"
            "Encoder version: %6")
            .arg(PathToQString(database.CacheDirectory()))
            .arg(database.Entries().size())
            .arg(header.entryCount)
            .arg(header.version)
            .arg(header.addressSize)
            .arg(ToQString(header.encoderVersion));
    }

    QString BuildDiagnosticText(
        const ViewerTextureCacheSource& source,
        const GalleryPreviewMetricsSnapshot& galleryMetrics)
    {
        return QStringLiteral(
            "Application version: %1\n"
            "Qt runtime: %2\n"
            "Qt build: %3\n"
            "%4\n"
            "%5")
            .arg(QStringLiteral(CACHEEXPLORER_VERSION))
            .arg(QString::fromLatin1(qVersion()))
            .arg(QStringLiteral(QT_VERSION_STR))
            .arg(CacheDiagnosticText(source))
            .arg(GalleryPerformanceDiagnosticText(galleryMetrics));
    }
}

void ShowAboutDialog(
    QWidget& parent,
    const ViewerTextureCacheSource& source,
    const GalleryPreviewMetricsSnapshot& galleryMetrics)
{
    QMessageBox aboutBox(&parent);
    aboutBox.setWindowTitle(QStringLiteral("About Cache Explorer"));
    aboutBox.setIconPixmap(QApplication::windowIcon().pixmap(64, 64));
    aboutBox.setText(QStringLiteral("Cache Explorer"));
    aboutBox.setInformativeText(
        QStringLiteral(
            "Standalone Second Life® viewer texture-cache browser and PNG exporter.\n"
            "Verified with the official Second Life viewer and Firestorm Viewer.\n"
            "Experimental Aurora Viewer cache support is also enabled.\n\n"
            "Open-source software licensed under the MIT License.\n"
            "BC7 decoding uses bcdec under the MIT License.\n\n"
            "Second Life® is a registered trademark of Linden Research, Inc.\n"
            "Firestorm Viewer is a trademark of The Phoenix Firestorm Project, Inc.\n"
            "CacheExplorer is not affiliated with, sponsored by, or endorsed by "
            "either organization."));
    aboutBox.setDetailedText(BuildDiagnosticText(source, galleryMetrics));
    aboutBox.exec();
}
