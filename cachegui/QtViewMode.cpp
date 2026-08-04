#include "QtViewMode.h"

#include <QListView>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QWidget>

bool ToggleGalleryMode(bool galleryMode)
{
    return !galleryMode;
}

void ApplyViewMode(
    bool galleryMode,
    QStackedWidget& viewStack,
    QTableView& table,
    QListView& galleryView,
    QPushButton& viewToggleButton)
{
    viewStack.setCurrentWidget(
        galleryMode
            ? static_cast<QWidget*>(&galleryView)
            : static_cast<QWidget*>(&table));
    viewToggleButton.setText(
        galleryMode ? QStringLiteral("Table") : QStringLiteral("Gallery"));
    viewToggleButton.setIcon(
        viewToggleButton.style()->standardIcon(
            galleryMode
                ? QStyle::SP_FileDialogDetailedView
                : QStyle::SP_FileDialogListView));
    const QString targetView = galleryMode
        ? QStringLiteral("Table")
        : QStringLiteral("Gallery");
    viewToggleButton.setAccessibleName(
        QStringLiteral("Switch to %1 view").arg(targetView));
    viewToggleButton.setToolTip(
        QStringLiteral("Switch to %1 view").arg(targetView));
}
