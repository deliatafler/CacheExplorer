#pragma once

#include <QSortFilterProxyModel>

class QComboBox;

enum class GalleryPreviewFilter
{
    All,
    Previewable,
    Unknown,
    NoPreview,
    LoadFailed
};

void ConfigureGalleryPreviewFilterControl(QComboBox& comboBox);
GalleryPreviewFilter CurrentGalleryPreviewFilter(const QComboBox& comboBox);

class GalleryFilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit GalleryFilterProxyModel(QObject* parent = nullptr);

    void SetGalleryMode(bool galleryMode);
    void SetPreviewFilter(GalleryPreviewFilter filter);

protected:
    bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    bool galleryMode_ = false;
    GalleryPreviewFilter filter_ = GalleryPreviewFilter::All;
};
