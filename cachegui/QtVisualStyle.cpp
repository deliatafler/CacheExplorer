#include "QtVisualStyle.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QString>

namespace
{
    QString CssColor(const QColor& color)
    {
        return color.name(QColor::HexRgb);
    }
}

void ApplyCacheExplorerVisualStyle(QApplication& application)
{
    const QPalette palette = application.palette();
    const QColor window = palette.color(QPalette::Window);
    const QColor base = palette.color(QPalette::Base);
    const QColor alternateBase = base.lightness() > 128
        ? base.darker(104)
        : base.lighter(130);
    const QColor text = palette.color(QPalette::Text);
    const QColor mutedText = palette.color(QPalette::PlaceholderText);
    const QColor border = palette.color(QPalette::Mid);
    const QColor highlight = palette.color(QPalette::Highlight);
    const QColor highlightedText = palette.color(QPalette::HighlightedText);

    application.setStyleSheet(
        QStringLiteral(
            "QWidget#CacheExplorerRoot { background: %1; }"
            "QFrame#ToolbarBand, QFrame#PreviewPane {"
            "  background: %2; border: 1px solid %3; border-radius: 6px;"
            "}"
            "QFrame#ToolbarBand QLabel { background: transparent; }"
            "QPushButton, QToolButton {"
            "  min-height: 30px; padding: 0 10px; border: 1px solid %3;"
            "  border-radius: 5px; background: %2; color: %4;"
            "}"
            "QPushButton:hover, QToolButton:hover {"
            "  border-color: %7; background: %5;"
            "}"
            "QPushButton:pressed, QToolButton:pressed { background: %1; }"
            "QPushButton:disabled, QToolButton:disabled { color: %6; }"
            "QPushButton[primary=\"true\"] {"
            "  background: %7; border-color: %7; color: %8; font-weight: 600;"
            "}"
            "QPushButton[primary=\"true\"]:hover {"
            "  background: %7; border-color: %4;"
            "}"
            "QPushButton#ViewToggle { padding: 0 14px; font-weight: 600; }"
            "QLineEdit, QComboBox {"
            "  min-height: 30px; padding: 0 8px; border: 1px solid %3;"
            "  border-radius: 5px; background: %2; color: %4;"
            "}"
            "QLineEdit:focus, QComboBox:focus { border: 2px solid %7; }"
            "QComboBox::drop-down { border: 0; width: 24px; }"
            "QLabel#SecondaryText, QLabel#GalleryCount, QLabel#GalleryActivity,"
            "QLabel#PreviewCaption, QLabel#PreviewDetails { color: %6; }"
            "QLabel#PreviewTitle { font-weight: 600; color: %4; }"
            "QLabel#StatusLabel { color: %6; padding: 2px 4px; }"
            "QTableView {"
            "  border: 1px solid %3; border-radius: 5px; background: %2;"
            "  alternate-background-color: %5; gridline-color: %3;"
            "}"
            "QTableView::item { padding: 4px; }"
            "QHeaderView::section {"
            "  background: %5; color: %4; border: 0;"
            "  border-right: 1px solid %3; border-bottom: 1px solid %3;"
            "  padding: 6px; font-weight: 600;"
            "}"
            "QListView#GalleryView { border: 0; background: %2; }"
            "QSplitter::handle { background: %3; width: 1px; }"
            "QSplitter::handle:hover { background: %7; }"
        )
            .arg(CssColor(window))
            .arg(CssColor(base))
            .arg(CssColor(border))
            .arg(CssColor(text))
            .arg(CssColor(alternateBase))
            .arg(CssColor(mutedText))
            .arg(CssColor(highlight))
            .arg(CssColor(highlightedText)));
}
