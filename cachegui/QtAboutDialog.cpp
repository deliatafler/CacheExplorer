#include "QtAboutDialog.h"

#include "QtHelpers.h"
#include "TextureCacheDatabase.h"

#include <QApplication>
#include <QString>
#include <QtGlobal>
#include <QMessageBox>

#ifndef CACHEEXPLORER_VERSION
#define CACHEEXPLORER_VERSION "unknown"
#endif

namespace
{
    QString CacheDiagnosticText(const TextureCacheDatabase& database)
    {
        if (!database.IsOpen())
        {
            return QStringLiteral("Cache: not open");
        }

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

    QString BuildDiagnosticText(const TextureCacheDatabase& database)
    {
        return QStringLiteral(
            "Application version: %1\n"
            "Qt runtime: %2\n"
            "Qt build: %3\n"
            "%4")
            .arg(QStringLiteral(CACHEEXPLORER_VERSION))
            .arg(QString::fromLatin1(qVersion()))
            .arg(QStringLiteral(QT_VERSION_STR))
            .arg(CacheDiagnosticText(database));
    }
}

void ShowAboutDialog(
    QWidget& parent,
    const TextureCacheDatabase& database)
{
    QMessageBox aboutBox(&parent);
    aboutBox.setWindowTitle(QStringLiteral("About Cache Explorer"));
    aboutBox.setIconPixmap(QApplication::windowIcon().pixmap(64, 64));
    aboutBox.setText(QStringLiteral("Cache Explorer"));
    aboutBox.setInformativeText(
        QStringLiteral(
            "Standalone Second Life viewer texture-cache browser and PNG exporter.\n"
            "Verified with the official Second Life viewer and Firestorm.\n\n"
            "Open-source software licensed under the MIT License."));
    aboutBox.setDetailedText(BuildDiagnosticText(database));
    aboutBox.exec();
}
