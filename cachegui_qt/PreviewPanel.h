#pragma once

#include <QPixmap>
#include <QString>

class QLabel;

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
    QLabel* label_ = nullptr;
    QPixmap pixmap_;
};
