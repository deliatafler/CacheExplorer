#pragma once

class QPushButton;
class QLineEdit;
class QListView;
class QTableView;

struct MainActionState
{
    bool busy = false;
    bool hasSelection = false;
    bool previewWorkerActive = false;
    bool tryNextActive = false;
    bool databaseOpen = false;
    bool galleryMode = false;
};

void ApplyMainActionState(
    const MainActionState& state,
    QPushButton& tryNextButton,
    QPushButton& exportButton,
    QPushButton& viewToggleButton);

void ApplyBusyState(
    bool busy,
    QPushButton& openButton,
    QPushButton& browseButton,
    QLineEdit& pathEdit,
    QTableView& table,
    QListView& galleryView);
