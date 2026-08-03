#include "GalleryItemDelegate.h"

#include <algorithm>

#include <QIcon>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace
{
    constexpr int TileWidth = 184;
    constexpr int TileHeight = 184;
    constexpr int TileInset = 5;
    constexpr int ContentInset = 9;
    constexpr int ImageExtent = 144;

    QColor Translucent(QColor color, int alpha)
    {
        color.setAlpha(alpha);
        return color;
    }
}

GalleryItemDelegate::GalleryItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void GalleryItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const QColor accent = option.palette.color(QPalette::Highlight);
    const QColor border = selected
        ? accent
        : hovered
            ? Translucent(accent, 180)
            : option.palette.color(QPalette::Midlight);
    const QColor background = selected
        ? Translucent(accent, 38)
        : hovered
            ? option.palette.color(QPalette::AlternateBase)
            : option.palette.color(QPalette::Base);

    const QRect tile = option.rect.adjusted(
        TileInset,
        TileInset,
        -TileInset,
        -TileInset);
    painter->setPen(QPen(border, selected ? 2.0 : 1.0));
    painter->setBrush(background);
    painter->drawRoundedRect(tile, 6.0, 6.0);

    const QRect imageArea(
        tile.center().x() - ImageExtent / 2,
        tile.top() + ContentInset,
        ImageExtent,
        ImageExtent);
    const QVariant decoration = index.data(Qt::DecorationRole);
    QPixmap pixmap;

    if (decoration.canConvert<QIcon>())
    {
        pixmap = decoration.value<QIcon>().pixmap(
            ImageExtent,
            ImageExtent);
    }
    else if (decoration.canConvert<QPixmap>())
    {
        pixmap = decoration.value<QPixmap>();
    }

    if (!pixmap.isNull())
    {
        const QSize fittedSize = pixmap.size().scaled(
            imageArea.size(),
            Qt::KeepAspectRatio);
        const QRect target(
            imageArea.center().x() - fittedSize.width() / 2,
            imageArea.center().y() - fittedSize.height() / 2,
            fittedSize.width(),
            fittedSize.height());
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter->drawPixmap(target, pixmap);
    }

    QFont textFont = option.font;
    if (textFont.pointSize() > 8)
    {
        textFont.setPointSize(textFont.pointSize() - 1);
    }
    painter->setFont(textFont);
    painter->setPen(option.palette.color(QPalette::Text));

    const QRect textArea(
        tile.left() + ContentInset,
        imageArea.bottom() + 5,
        tile.width() - ContentInset * 2,
        std::max(0, tile.bottom() - imageArea.bottom() - 8));
    const QString uuid = index.data(Qt::DisplayRole).toString();
    painter->drawText(
        textArea,
        Qt::AlignHCenter | Qt::AlignTop,
        option.fontMetrics.elidedText(
            uuid,
            Qt::ElideMiddle,
            textArea.width()));

    painter->restore();
}

QSize GalleryItemDelegate::sizeHint(
    const QStyleOptionViewItem&,
    const QModelIndex&) const
{
    return QSize(TileWidth, TileHeight);
}
