#include "QtActionState.h"

#include <QPushButton>

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
