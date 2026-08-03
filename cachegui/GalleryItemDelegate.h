#pragma once

#include <QStyledItemDelegate>

class GalleryItemDelegate final : public QStyledItemDelegate
{
public:
    explicit GalleryItemDelegate(QObject* parent = nullptr);

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
};
