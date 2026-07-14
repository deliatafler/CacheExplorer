#include "PreviewImage.h"

#include <QImage>

bool CreatePreviewPixmap(
    const DecodedImage& decodedImage,
    QPixmap& pixmap,
    QString& errorMessage)
{
    const QImage image(
        decodedImage.rgba.data(),
        static_cast<int>(decodedImage.width),
        static_cast<int>(decodedImage.height),
        static_cast<int>(decodedImage.width * 4),
        QImage::Format_RGBA8888);

    if (image.isNull())
    {
        errorMessage = QStringLiteral("Preview image could not be created.");
        return false;
    }

    pixmap = QPixmap::fromImage(image.copy());
    return true;
}
