#include "GalleryFilterProxyModel.h"

#include "CacheEntryTableModel.h"
#include "PreviewCache.h"

#include <QComboBox>
#include <QString>
#include <QtGlobal>

namespace
{
    int ToComboValue(GalleryPreviewFilter filter)
    {
        return static_cast<int>(filter);
    }

    GalleryPreviewFilter FromComboValue(int value)
    {
        switch (static_cast<GalleryPreviewFilter>(value))
        {
            case GalleryPreviewFilter::Everything:
            case GalleryPreviewFilter::ImagesOnly:
            case GalleryPreviewFilter::RecentChanges:
                return static_cast<GalleryPreviewFilter>(value);
        }

        return GalleryPreviewFilter::Everything;
    }
}

GalleryFilterProxyModel::GalleryFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void GalleryFilterProxyModel::SetGalleryMode(bool galleryMode)
{
    if (galleryMode_ == galleryMode)
    {
        return;
    }

    BeginFilterUpdate();
    galleryMode_ = galleryMode;
    EndFilterUpdate();
}

void GalleryFilterProxyModel::SetPreviewFilter(GalleryPreviewFilter filter)
{
    if (filter_ == filter)
    {
        return;
    }

    BeginFilterUpdate();
    filter_ = filter;
    EndFilterUpdate();
}

void GalleryFilterProxyModel::SetRecentCacheIndices(
    const std::vector<std::uint32_t>& cacheIndices)
{
    recentCacheIndices_.clear();
    recentCacheIndices_.insert(cacheIndices.begin(), cacheIndices.end());

    if (galleryMode_ && filter_ == GalleryPreviewFilter::RecentChanges)
    {
        BeginFilterUpdate();
        EndFilterUpdate();
    }
}

bool GalleryFilterProxyModel::RefreshForPreviewStateChange()
{
    if (!galleryMode_ || filter_ != GalleryPreviewFilter::ImagesOnly)
    {
        return false;
    }

    BeginFilterUpdate();
    EndFilterUpdate();
    return true;
}

void GalleryFilterProxyModel::BeginFilterUpdate()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
}

void GalleryFilterProxyModel::EndFilterUpdate()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

bool GalleryFilterProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (!galleryMode_ || filter_ == GalleryPreviewFilter::Everything)
    {
        return true;
    }

    if (sourceModel() == nullptr)
    {
        return true;
    }

    const QModelIndex sourceIndex =
        sourceModel()->index(sourceRow, 0, sourceParent);
    const int previewStateValue =
        sourceIndex.data(CacheEntryTableModel::PreviewStateRole).toInt();
    const PreviewState previewState =
        static_cast<PreviewState>(previewStateValue);

    switch (filter_)
    {
        case GalleryPreviewFilter::Everything:
            return true;

        case GalleryPreviewFilter::ImagesOnly:
            return previewState == PreviewState::Unknown ||
                previewState == PreviewState::Checking ||
                previewState == PreviewState::Previewable;

        case GalleryPreviewFilter::RecentChanges:
        {
            const std::uint32_t cacheIndex = sourceIndex.data(
                CacheEntryTableModel::CacheIndexRole).toUInt();
            return recentCacheIndices_.find(cacheIndex) !=
                recentCacheIndices_.end();
        }
    }

    return true;
}

void ConfigureGalleryPreviewFilterControl(QComboBox& comboBox)
{
    comboBox.addItem(
        QStringLiteral("Everything"),
        ToComboValue(GalleryPreviewFilter::Everything));
    comboBox.addItem(
        QStringLiteral("Images only"),
        ToComboValue(GalleryPreviewFilter::ImagesOnly));
    comboBox.addItem(
        QStringLiteral("Recent changes"),
        ToComboValue(GalleryPreviewFilter::RecentChanges));
}

GalleryPreviewFilter CurrentGalleryPreviewFilter(const QComboBox& comboBox)
{
    return FromComboValue(comboBox.currentData().toInt());
}
