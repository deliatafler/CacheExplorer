#pragma once

class QListView;
class QPushButton;
class QStackedWidget;
class QTableView;

bool ToggleGalleryMode(bool galleryMode);

void ApplyViewMode(
    bool galleryMode,
    QStackedWidget& viewStack,
    QTableView& table,
    QListView& galleryView,
    QPushButton& viewToggleButton);
