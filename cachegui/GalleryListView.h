#pragma once

#include <QListView>

class GalleryListView final : public QListView
{
public:
    explicit GalleryListView(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QModelIndex IndexAtExpandedGalleryCell(const QPoint& position) const;
};
