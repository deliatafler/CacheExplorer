#pragma once

#include <QSortFilterProxyModel>

class QComboBox;

enum class GalleryPreviewFilter
{
    Everything,
    ImagesOnly
};

void ConfigureGalleryPreviewFilterControl(QComboBox& comboBox);
GalleryPreviewFilter CurrentGalleryPreviewFilter(const QComboBox& comboBox);

class GalleryFilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit GalleryFilterProxyModel(QObject* parent = nullptr);

    void SetGalleryMode(bool galleryMode);
    void SetPreviewFilter(GalleryPreviewFilter filter);
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
};
