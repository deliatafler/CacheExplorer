#pragma once

class QComboBox;
class QSortFilterProxyModel;
class QString;

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
QString GallerySortInProgressStatus(GallerySortMode sortMode);
QString GallerySortCompleteStatus(GallerySortMode sortMode);
void ApplyGallerySort(
    QSortFilterProxyModel& proxyModel,
    GallerySortMode sortMode);
