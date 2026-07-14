#include "QtGallerySort.h"

#include <QComboBox>
#include <QSortFilterProxyModel>
#include <QString>
#include <Qt>

namespace
{
    int ToComboValue(GallerySortMode sortMode)
    {
        return static_cast<int>(sortMode);
    }

    GallerySortMode FromComboValue(int value)
    {
        switch (static_cast<GallerySortMode>(value))
        {
            case GallerySortMode::NewestFirst:
            case GallerySortMode::OldestFirst:
            case GallerySortMode::LargestBody:
            case GallerySortMode::LargestImage:
            case GallerySortMode::CacheIndex:
            case GallerySortMode::Uuid:
                return static_cast<GallerySortMode>(value);
        }

        return GallerySortMode::NewestFirst;
    }
}

void ConfigureGallerySortControl(QComboBox& comboBox)
{
    comboBox.addItem(
        QStringLiteral("Newest"),
        ToComboValue(GallerySortMode::NewestFirst));
    comboBox.addItem(
        QStringLiteral("Oldest"),
        ToComboValue(GallerySortMode::OldestFirst));
    comboBox.addItem(
        QStringLiteral("Largest body"),
        ToComboValue(GallerySortMode::LargestBody));
    comboBox.addItem(
        QStringLiteral("Largest image"),
        ToComboValue(GallerySortMode::LargestImage));
    comboBox.addItem(
        QStringLiteral("Cache index"),
        ToComboValue(GallerySortMode::CacheIndex));
    comboBox.addItem(
        QStringLiteral("UUID"),
        ToComboValue(GallerySortMode::Uuid));
}

GallerySortMode CurrentGallerySortMode(const QComboBox& comboBox)
{
    return FromComboValue(comboBox.currentData().toInt());
}

void ApplyGallerySort(
    QSortFilterProxyModel& proxyModel,
    GallerySortMode sortMode)
{
    switch (sortMode)
    {
        case GallerySortMode::NewestFirst:
            proxyModel.sort(4, Qt::DescendingOrder);
            return;

        case GallerySortMode::OldestFirst:
            proxyModel.sort(4, Qt::AscendingOrder);
            return;

        case GallerySortMode::LargestBody:
            proxyModel.sort(2, Qt::DescendingOrder);
            return;

        case GallerySortMode::LargestImage:
            proxyModel.sort(1, Qt::DescendingOrder);
            return;

        case GallerySortMode::CacheIndex:
            proxyModel.sort(3, Qt::AscendingOrder);
            return;

        case GallerySortMode::Uuid:
            proxyModel.sort(0, Qt::AscendingOrder);
            return;
    }
}
