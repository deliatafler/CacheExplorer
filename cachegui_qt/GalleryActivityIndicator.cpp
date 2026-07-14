#include "GalleryActivityIndicator.h"

#include <algorithm>

#include <QLabel>
#include <QString>

void GalleryActivityIndicator::SetLabel(QLabel* label)
{
    label_ = label;
}

void GalleryActivityIndicator::Update(const GalleryActivityState& state)
{
    if (label_ == nullptr)
    {
        return;
    }

    const bool showLabel =
        state.galleryMode &&
        state.databaseOpen &&
        (state.workerActive ||
            state.searchPending ||
            state.refreshPending ||
            state.hasQueuedEntries);

    label_->setVisible(showLabel);

    if (!showLabel)
    {
        label_->clear();
        return;
    }

    if (state.searchPending)
    {
        label_->setText(QStringLiteral("Finding visible thumbnails..."));
        return;
    }

    if (state.refreshPending)
    {
        label_->setText(QStringLiteral("Refreshing visible thumbnails..."));
        return;
    }

    if (state.queueTotal > 0)
    {
        const std::size_t current =
            std::min(
                state.queueTotal,
                state.queueCompleted + (state.workerActive ? 1u : 0u));
        label_->setText(
            QStringLiteral("Loading thumbnails %1 / %2")
                .arg(static_cast<qulonglong>(current))
                .arg(static_cast<qulonglong>(state.queueTotal)));
        return;
    }

    label_->setText(QStringLiteral("Loading thumbnails..."));
}
