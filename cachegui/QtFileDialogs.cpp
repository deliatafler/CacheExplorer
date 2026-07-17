#include "QtFileDialogs.h"

#include "QtHelpers.h"

#include <QFileDialog>
#include <QWidget>

QString ChooseCacheDirectory(
    QWidget& parent,
    const QString& currentPath)
{
    return QFileDialog::getExistingDirectory(
        &parent,
        QStringLiteral("Choose Second Life viewer texture cache folder"),
        CacheFolderDialogStartPath(currentPath));
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

QString ChoosePngOutputDirectory(QWidget& parent)
{
    return QFileDialog::getExistingDirectory(
        &parent,
        QStringLiteral("Choose PNG export folder"));
}
