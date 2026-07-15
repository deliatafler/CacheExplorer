#include "QtTryNextPreview.h"

#include "CacheEntryTableModel.h"
#include "QtHelpers.h"

#include <QModelIndex>
#include <QSortFilterProxyModel>

int FirstTryNextProxyRow(const QModelIndex& selectedIndex)
{
    if (!selectedIndex.isValid())
    {
        return 0;
    }

    return selectedIndex.row() + 1;
}

const CacheEntry* TakeNextTryNextEntry(
    TryNextPreviewState& state,
    QSortFilterProxyModel& proxyModel,
    CacheEntryTableModel& tableModel)
{
    const QModelIndex proxyIndex =
        proxyModel.index(state.TakeNextProxyRow(), 0);
    const QModelIndex sourceIndex =
        proxyModel.mapToSource(proxyIndex);

    return tableModel.EntryAt(sourceIndex.row());
}

QString TryNextSearchStartedStatus()
{
    return QStringLiteral("Looking for the next previewable texture...");
}

QString TryNextSearchExhaustedStatus(int attempts)
{
    return QStringLiteral("No previewable texture found in the next %1 entries.")
        .arg(attempts);
}

QString TryingPreviewStatus(const CacheEntry& entry)
{
    return QStringLiteral("Trying preview %1...")
        .arg(ToQString(entry.uuid.ToString()));
}
