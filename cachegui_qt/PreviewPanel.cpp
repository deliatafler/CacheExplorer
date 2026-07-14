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

    ApplyState(PreviewPanelState::Empty);
    label_->setText(QStringLiteral("No preview selected."));
    label_->setPixmap({});
}

void PreviewPanel::SetMessage(const QString& message)
{
    pixmap_ = {};

    if (label_ == nullptr)
    {
        return;
    }

    ApplyState(PreviewPanelState::Message);
    label_->setText(message);
    label_->setPixmap({});
}

void PreviewPanel::SetPixmap(const QPixmap& pixmap)
{
    pixmap_ = pixmap;
    ApplyState(PreviewPanelState::Image);
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

void PreviewPanel::ApplyState(PreviewPanelState state)
{
    state_ = state;

    if (label_ == nullptr)
    {
        return;
    }

    switch (state_)
    {
        case PreviewPanelState::Empty:
            label_->setStyleSheet(
                QStringLiteral("QLabel { background: #202020; color: #a8a8a8; }"));
            break;

        case PreviewPanelState::Message:
            label_->setStyleSheet(
                QStringLiteral("QLabel { background: #241f1f; color: #f0c7c7; }"));
            break;

        case PreviewPanelState::Image:
            label_->setStyleSheet(
                QStringLiteral("QLabel { background: #202020; color: #d0d0d0; }"));
            break;
    }
}
