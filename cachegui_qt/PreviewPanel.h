#pragma once

#include <QPixmap>
#include <QString>

class QLabel;

enum class PreviewPanelState
{
    Empty,
    Message,
    Image
};

class PreviewPanel
{
public:
    void SetLabel(QLabel* label);
    void Clear();
    void SetMessage(const QString& message);
    void SetPixmap(const QPixmap& pixmap);
    void Refresh();
    bool HasPixmap() const;

private:
    void ApplyState(PreviewPanelState state);

    QLabel* label_ = nullptr;
    QPixmap pixmap_;
    PreviewPanelState state_ = PreviewPanelState::Empty;
};
