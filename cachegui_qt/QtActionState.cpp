#include "QtActionState.h"

#include <QApplication>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTableView>

void ApplyMainActionState(
    const MainActionState& state,
    QPushButton& previewButton,
    QPushButton& tryNextButton,
    QPushButton& exportButton,
    QPushButton& viewToggleButton)
{
    const bool previewIdle = !state.previewWorkerActive;
    const bool actionIdle = !state.busy && !state.tryNextActive && previewIdle;

    previewButton.setEnabled(actionIdle && state.hasSelection);
    tryNextButton.setEnabled(actionIdle && state.databaseOpen);
    exportButton.setEnabled(actionIdle && state.hasSelection);
    viewToggleButton.setEnabled(!state.busy && state.databaseOpen);
}

void ApplyBusyState(
    bool busy,
    QPushButton& openButton,
    QPushButton& browseButton,
    QLineEdit& pathEdit,
    QTableView& table,
    QListView& galleryView)
{
    openButton.setEnabled(!busy);
    browseButton.setEnabled(!busy);
    pathEdit.setEnabled(!busy);
    table.setEnabled(!busy);
    galleryView.setEnabled(!busy);

    if (busy)
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents();
    }
    else
    {
        QApplication::restoreOverrideCursor();
    }
}
