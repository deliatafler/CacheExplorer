#pragma once

#include "TextureCacheSource.h"

#include <optional>

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

void ConfigureGallerySortControl(
    QComboBox& comboBox,
    std::optional<TextureCacheFormat> format = std::nullopt);
bool IsGallerySortModeAvailable(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format = std::nullopt);
QString GallerySortControlLabel(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format = std::nullopt);
GallerySortMode CurrentGallerySortMode(const QComboBox& comboBox);
QString GallerySortInProgressStatus(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format = std::nullopt);
QString GallerySortCompleteStatus(
    GallerySortMode sortMode,
    std::optional<TextureCacheFormat> format = std::nullopt);
void ApplyGallerySort(
    QSortFilterProxyModel& proxyModel,
    GallerySortMode sortMode);
