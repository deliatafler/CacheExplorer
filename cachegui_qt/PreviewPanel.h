#pragma once

#include <QPixmap>

class QLabel;

class PreviewPanel
{
public:
    void SetLabel(QLabel* label);
    void Clear();
    void SetPixmap(const QPixmap& pixmap);
    void Refresh();
    bool HasPixmap() const;

private:
    QLabel* label_ = nullptr;
    QPixmap pixmap_;
};
