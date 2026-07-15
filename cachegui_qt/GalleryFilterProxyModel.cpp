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
            case GalleryPreviewFilter::All:
            case GalleryPreviewFilter::CachedComplete:
            case GalleryPreviewFilter::Unknown:
            case GalleryPreviewFilter::NoPreview:
            case GalleryPreviewFilter::LoadFailed:
                return static_cast<GalleryPreviewFilter>(value);
        }

        return GalleryPreviewFilter::All;
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
    if (!galleryMode_ || filter_ == GalleryPreviewFilter::All)
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
        case GalleryPreviewFilter::All:
            return true;

        case GalleryPreviewFilter::CachedComplete:
            return sourceIndex
                .data(CacheEntryTableModel::CachedCompleteRole)
                .toBool();

        case GalleryPreviewFilter::Unknown:
            return previewState == PreviewState::Unknown ||
                previewState == PreviewState::Checking;

        case GalleryPreviewFilter::NoPreview:
            return previewState == PreviewState::Unavailable;

        case GalleryPreviewFilter::LoadFailed:
            return previewState == PreviewState::LoadFailed;
    }

    return true;
}

void ConfigureGalleryPreviewFilterControl(QComboBox& comboBox)
{
    comboBox.addItem(
        QStringLiteral("All"),
        ToComboValue(GalleryPreviewFilter::All));
    comboBox.addItem(
        QStringLiteral("Cached complete"),
        ToComboValue(GalleryPreviewFilter::CachedComplete));
    comboBox.addItem(
        QStringLiteral("Unknown"),
        ToComboValue(GalleryPreviewFilter::Unknown));
    comboBox.addItem(
        QStringLiteral("No preview"),
        ToComboValue(GalleryPreviewFilter::NoPreview));
    comboBox.addItem(
        QStringLiteral("Load failed"),
        ToComboValue(GalleryPreviewFilter::LoadFailed));
}

GalleryPreviewFilter CurrentGalleryPreviewFilter(const QComboBox& comboBox)
{
    return FromComboValue(comboBox.currentData().toInt());
}
