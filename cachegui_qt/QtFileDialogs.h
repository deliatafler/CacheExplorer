#pragma once

#include <QString>

class QWidget;

QString ChooseCacheDirectory(
    QWidget& parent,
    const QString& currentPath);

QString ChoosePngOutputFile(
    QWidget& parent,
    const QString& defaultName);

QString ChoosePngOutputDirectory(
    QWidget& parent);
