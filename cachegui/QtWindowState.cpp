#include "QtWindowState.h"

#include <QSettings>

QtWindowState LoadQtWindowState()
{
    QSettings settings;
    return {
        settings.value(QStringLiteral("ui/windowGeometry")).toByteArray(),
        settings.value(QStringLiteral("ui/contentSplitterState")).toByteArray()};
}

void SaveQtWindowState(const QtWindowState& state)
{
    QSettings settings;
    settings.setValue(
        QStringLiteral("ui/windowGeometry"),
        state.windowGeometry);
    settings.setValue(
        QStringLiteral("ui/contentSplitterState"),
        state.contentSplitterState);
}
