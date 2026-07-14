#pragma once

class QPushButton;

struct MainActionState
{
    bool busy = false;
    bool hasSelection = false;
    bool previewWorkerActive = false;
    bool tryNextActive = false;
    bool databaseOpen = false;
};

void ApplyMainActionState(
    const MainActionState& state,
    QPushButton& previewButton,
    QPushButton& tryNextButton,
    QPushButton& exportButton,
    QPushButton& viewToggleButton);
