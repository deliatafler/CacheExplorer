#include "QtGalleryStatus.h"

#include <QString>

QString GalleryEntryCountText(
    int visibleEntries,
    int totalEntries)
{
    if (totalEntries <= 0)
    {
        return QStringLiteral("0 entries");
    }

    if (visibleEntries == totalEntries)
    {
        return QStringLiteral("%1 entries")
            .arg(totalEntries);
    }

    return QStringLiteral("%1 of %2 entries")
        .arg(visibleEntries)
        .arg(totalEntries);
}

QString GalleryFilterUpdatedStatus(
    int visibleEntries,
    int totalEntries)
{
    if (totalEntries <= 0)
    {
        return QStringLiteral("Gallery filter updated: no entries loaded.");
    }

    if (visibleEntries <= 0)
    {
        return QStringLiteral("Gallery filter updated: no matching entries.");
    }

    return QStringLiteral("Gallery filter updated: showing %1.")
        .arg(GalleryEntryCountText(visibleEntries, totalEntries));
}
