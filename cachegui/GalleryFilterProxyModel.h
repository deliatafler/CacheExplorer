#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <QSortFilterProxyModel>

class QComboBox;

enum class GalleryPreviewFilter
{
    Everything,
    ImagesOnly,
    RecentChanges
};

void ConfigureGalleryPreviewFilterControl(QComboBox& comboBox);
GalleryPreviewFilter CurrentGalleryPreviewFilter(const QComboBox& comboBox);

class GalleryFilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit GalleryFilterProxyModel(QObject* parent = nullptr);

    void SetGalleryMode(bool galleryMode);
    void SetPreviewFilter(GalleryPreviewFilter filter);
    void SetRecentCacheIndices(
        const std::vector<std::uint32_t>& cacheIndices);
    bool RefreshForPreviewStateChange();

protected:
    bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    void BeginFilterUpdate();
    void EndFilterUpdate();

    bool galleryMode_ = true;
    GalleryPreviewFilter filter_ = GalleryPreviewFilter::Everything;
    std::unordered_set<std::uint32_t> recentCacheIndices_;
};
