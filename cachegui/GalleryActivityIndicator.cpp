#include "GalleryActivityIndicator.h"

#include <algorithm>

#include <QLabel>
#include <QString>

QString GalleryActivityText(const GalleryActivityState& state)
{
    if (state.searchPending)
    {
        return QStringLiteral("Finding visible thumbnails...");
    }

    if (state.refreshPending)
    {
        return QStringLiteral("Refreshing visible thumbnails...");
    }

    if (state.queueTotal > 0)
    {
        const std::size_t current =
            std::min(
                state.queueTotal,
                state.queueCompleted + (state.workerActive ? 1u : 0u));
        QString text = QStringLiteral("Loading thumbnails %1 / %2")
            .arg(static_cast<qulonglong>(current))
            .arg(static_cast<qulonglong>(state.queueTotal));

        if (state.measuredCompleted > 0 && state.previewsPerSecond > 0.0)
        {
            text += QStringLiteral(" (%1/s)")
                .arg(state.previewsPerSecond, 0, 'f', 1);
        }

        return text;
    }

    return QStringLiteral("Loading thumbnails...");
}

QString GalleryActivityTooltip(const GalleryActivityState& state)
{
    if (state.measuredCompleted == 0)
    {
        return {};
    }

    return QStringLiteral(
        "This cache: %1 checked, %2 previews, %3 unavailable, %4 per second")
        .arg(static_cast<qulonglong>(state.measuredCompleted))
        .arg(static_cast<qulonglong>(state.measuredSucceeded))
        .arg(static_cast<qulonglong>(state.measuredUnavailable))
        .arg(state.previewsPerSecond, 0, 'f', 1);
}

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

    const bool reserveLabelSpace =
        state.galleryMode &&
        state.databaseOpen;

    const bool showActivity =
        reserveLabelSpace &&
        (state.workerActive ||
            state.searchPending ||
            state.refreshPending ||
            state.hasQueuedEntries);

    label_->setVisible(reserveLabelSpace);
    label_->setToolTip(GalleryActivityTooltip(state));

    if (!reserveLabelSpace)
    {
        label_->clear();
        return;
    }

    if (!showActivity)
    {
        label_->clear();
        return;
    }

    label_->setText(GalleryActivityText(state));
}
