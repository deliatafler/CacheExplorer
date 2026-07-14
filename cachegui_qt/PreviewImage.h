#pragma once

#include "J2CDecoder.h"

#include <QPixmap>
#include <QString>

bool CreatePreviewPixmap(
    const DecodedImage& decodedImage,
    QPixmap& pixmap,
    QString& errorMessage);
