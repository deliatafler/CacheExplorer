#pragma once

class QComboBox;
class QSortFilterProxyModel;

enum class GallerySortMode
{
    NewestFirst,
    OldestFirst,
    LargestBody,
    LargestImage,
    CacheIndex,
    Uuid
};

void ConfigureGallerySortControl(QComboBox& comboBox);
GallerySortMode CurrentGallerySortMode(const QComboBox& comboBox);
void ApplyGallerySort(
    QSortFilterProxyModel& proxyModel,
    GallerySortMode sortMode);
