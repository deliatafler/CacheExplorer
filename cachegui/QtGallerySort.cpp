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

    QString GallerySortName(GallerySortMode sortMode)
    {
        switch (sortMode)
        {
            case GallerySortMode::NewestFirst:
                return QStringLiteral("newest");

            case GallerySortMode::OldestFirst:
                return QStringLiteral("oldest");

            case GallerySortMode::LargestBody:
                return QStringLiteral("largest body");

            case GallerySortMode::LargestImage:
                return QStringLiteral("largest image");

            case GallerySortMode::CacheIndex:
                return QStringLiteral("cache index");

            case GallerySortMode::Uuid:
                return QStringLiteral("UUID");
        }

        return QStringLiteral("selected order");
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

QString GallerySortInProgressStatus(GallerySortMode sortMode)
{
    return QStringLiteral("Sorting gallery by %1...")
        .arg(GallerySortName(sortMode));
}

QString GallerySortCompleteStatus(GallerySortMode sortMode)
{
    return QStringLiteral("Gallery sorted by %1.")
        .arg(GallerySortName(sortMode));
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
