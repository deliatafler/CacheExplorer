#include "QtFileDialogs.h"

#include "QtHelpers.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
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
    const QString outputFile = QFileDialog::getSaveFileName(
        &parent,
        QStringLiteral("Export PNG"),
        QDir(PreferredPngExportDirectory()).filePath(defaultName),
        QStringLiteral("PNG images (*.png)"));

    if (!outputFile.isEmpty())
    {
        RememberPngExportDirectory(QFileInfo(outputFile).absolutePath());
    }

    return outputFile;
}

QString ChoosePngOutputDirectory(QWidget& parent)
{
    const QString outputDirectory = QFileDialog::getExistingDirectory(
        &parent,
        QStringLiteral("Choose PNG export folder"),
        PreferredPngExportDirectory());

    if (!outputDirectory.isEmpty())
    {
        RememberPngExportDirectory(outputDirectory);
    }

    return outputDirectory;
}
