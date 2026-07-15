#include "GalleryListView.h"

#include <algorithm>
#include <limits>

#include <QAbstractItemModel>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QMouseEvent>

GalleryListView::GalleryListView(QWidget* parent)
    : QListView(parent)
{
}

void GalleryListView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex directIndex = indexAt(event->pos());

    if (directIndex.isValid())
    {
        QListView::mousePressEvent(event);
        return;
    }

    const QModelIndex expandedIndex =
        IndexAtExpandedGalleryCell(event->pos());

    if (!expandedIndex.isValid())
    {
        QListView::mousePressEvent(event);
        return;
    }

    QItemSelectionModel* itemSelectionModel = selectionModel();

    if (itemSelectionModel != nullptr)
    {
        const Qt::KeyboardModifiers modifiers = event->modifiers();

        if (modifiers.testFlag(Qt::ShiftModifier) &&
            itemSelectionModel->currentIndex().isValid())
        {
            itemSelectionModel->select(
                QItemSelection(itemSelectionModel->currentIndex(), expandedIndex),
                QItemSelectionModel::ClearAndSelect |
                    QItemSelectionModel::Rows);
            itemSelectionModel->setCurrentIndex(
                expandedIndex,
                QItemSelectionModel::Current);
        }
        else
        {
            QItemSelectionModel::SelectionFlags flags =
                QItemSelectionModel::Current |
                QItemSelectionModel::Rows;

            flags |= modifiers.testFlag(Qt::ControlModifier)
                ? QItemSelectionModel::Toggle
                : QItemSelectionModel::ClearAndSelect;

            itemSelectionModel->setCurrentIndex(expandedIndex, flags);
        }
    }

    event->accept();
}

QModelIndex GalleryListView::IndexAtExpandedGalleryCell(
    const QPoint& position) const
{
    const QAbstractItemModel* itemModel = model();

    if (itemModel == nullptr)
    {
        return {};
    }

    const int rowCount = itemModel->rowCount(rootIndex());
    const QRect viewportArea = viewport()->rect();
    QModelIndex bestIndex;
    int bestDistance = std::numeric_limits<int>::max();

    for (int row = 0; row < rowCount; ++row)
    {
        const QModelIndex index =
            itemModel->index(row, modelColumn(), rootIndex());

        if (!index.isValid())
        {
            continue;
        }

        const QRect visualArea = visualRect(index);

        if (!visualArea.isValid() ||
            !visualArea.intersects(viewportArea))
        {
            continue;
        }

        const QSize cellSize = gridSize().isValid()
            ? gridSize()
            : QSize(
                std::max(iconSize().width(), visualArea.width()),
                iconSize().height() + visualArea.height());
        const QRect expandedArea = visualArea.adjusted(
            -cellSize.width() / 2,
            -cellSize.height(),
            cellSize.width() / 2,
            cellSize.height());

        if (expandedArea.contains(position))
        {
            const QPoint delta = position - visualArea.center();
            const int distance =
                delta.x() * delta.x() + delta.y() * delta.y();

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = index;
            }
        }
    }

    return bestIndex;
}
