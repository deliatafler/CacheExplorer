#pragma once

#include <QByteArray>

struct QtWindowState
{
    QByteArray windowGeometry;
    QByteArray contentSplitterState;
};

QtWindowState LoadQtWindowState();

void SaveQtWindowState(const QtWindowState& state);
