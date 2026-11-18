#include "QtGallerySort.h"

#include <QComboBox>
#include <QSignalBlocker>
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

    QString GallerySortName(
        GallerySortMode sortMode,
        std::optional<TextureCacheFormat> format)
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
                return format == TextureCacheFormat::Aurora
                    ? QStringLiteral("largest cache item")
                    : QStringLiteral("largest image");

            case GallerySortMode::CacheIndex:
                return QStringLiteral("cache index");

            case GallerySortMode::Uuid:
                return format == TextureCacheFormat::Aurora
                    ? QStringLiteral("cache key")
                    : QStringLiteral("UUID");
        }

        return QStringLiteral("selected order");
    }
}

void ConfigureGallerySortControl(
    QComboBox& comboBox,
    std::optional<TextureCacheFormat> format)
{
    const QSignalBlocker blocker(comboBox);
    const int previousValue = comboBox.currentData().isValid()
        ? comboBox.currentData().toInt()
        : ToComboValue(GallerySortMode::NewestFirst);

    comboBox.clear();
    const GallerySortMode modes[] = {
        GallerySortMode::NewestFirst,
        GallerySortMode::OldestFirst,
        GallerySortMode::LargestBody,
        GallerySortMode::LargestImage,
        GallerySortMode::CacheIndex,
        GallerySortMode::Uuid};

    for (const GallerySortMode mode : modes)
    {
        if (!IsGallerySortModeAvailable(mode, format))
        {
            continue;
        }

        comboBox.addItem(
            GallerySortControlLabel(mode, format),
            ToComboValue(mode));
    }

    const int restoredIndex = comboBox.findData(previousValue);
    comboBox.setCurrentIndex(restoredIndex >= 0 ? restoredIndex : 0);
}

bool IsGallerySortModeAvailable(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format)
{
    if (format != TextureCacheFormat::Aurora)
    {
        return true;
    }

    return sortMode == GallerySortMode::NewestFirst ||
        sortMode == GallerySortMode::OldestFirst ||
        sortMode == GallerySortMode::LargestImage ||
        sortMode == GallerySortMode::Uuid;
}

QString GallerySortControlLabel(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format)
{
    switch (sortMode)
    {
        case GallerySortMode::NewestFirst:
            return QStringLiteral("Newest");

        case GallerySortMode::OldestFirst:
            return QStringLiteral("Oldest");

        case GallerySortMode::LargestBody:
            return QStringLiteral("Largest body");

        case GallerySortMode::LargestImage:
            return format == TextureCacheFormat::Aurora
                ? QStringLiteral("Largest cache item")
                : QStringLiteral("Largest image");

        case GallerySortMode::CacheIndex:
            return QStringLiteral("Cache index");

        case GallerySortMode::Uuid:
            return format == TextureCacheFormat::Aurora
                ? QStringLiteral("Cache key")
                : QStringLiteral("UUID");
    }

    return QStringLiteral("Selected order");
}

GallerySortMode CurrentGallerySortMode(const QComboBox& comboBox)
{
    return FromComboValue(comboBox.currentData().toInt());
}

QString GallerySortInProgressStatus(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format)
{
    return QStringLiteral("Sorting gallery by %1...")
        .arg(GallerySortName(sortMode, format));
}

QString GallerySortCompleteStatus(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format)
{
    return QStringLiteral("Gallery sorted by %1.")
        .arg(GallerySortName(sortMode, format));
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
