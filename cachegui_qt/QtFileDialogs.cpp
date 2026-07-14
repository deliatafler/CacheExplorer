#include "QtFileDialogs.h"

#include <QFileDialog>
#include <QWidget>

QString ChooseCacheDirectory(
    QWidget& parent,
    const QString& currentPath)
{
    return QFileDialog::getExistingDirectory(
        &parent,
        QStringLiteral("Choose Firestorm texture cache folder"),
        currentPath);
}

QString ChoosePngOutputFile(
    QWidget& parent,
    const QString& defaultName)
{
    return QFileDialog::getSaveFileName(
        &parent,
        QStringLiteral("Export PNG"),
        defaultName,
        QStringLiteral("PNG images (*.png)"));
}
