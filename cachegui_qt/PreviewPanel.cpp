#include "PreviewPanel.h"

#include <QLabel>
#include <Qt>

void PreviewPanel::SetLabel(QLabel* label)
{
    label_ = label;
}

void PreviewPanel::Clear()
{
    pixmap_ = {};

    if (label_ == nullptr)
    {
        return;
    }

    label_->setText(QStringLiteral("No preview selected."));
    label_->setPixmap({});
}

void PreviewPanel::SetPixmap(const QPixmap& pixmap)
{
    pixmap_ = pixmap;
    Refresh();
}

void PreviewPanel::Refresh()
{
    if (label_ == nullptr || pixmap_.isNull())
    {
        return;
    }

    label_->setPixmap(
        pixmap_.scaled(
            label_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

bool PreviewPanel::HasPixmap() const
{
    return !pixmap_.isNull();
}
