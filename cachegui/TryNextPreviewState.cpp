#include "TryNextPreviewState.h"

void TryNextPreviewState::Start(int firstProxyRow)
{
    active_ = true;
    nextProxyRow_ = firstProxyRow;
    attempts_ = 0;
}

void TryNextPreviewState::Stop()
{
    active_ = false;
}

bool TryNextPreviewState::IsActive() const
{
    return active_;
}

bool TryNextPreviewState::IsExhausted(int proxyRowCount) const
{
    return nextProxyRow_ >= proxyRowCount ||
        attempts_ >= MaximumAttemptsPerClick;
}

int TryNextPreviewState::TakeNextProxyRow()
{
    const int proxyRow = nextProxyRow_;
    ++nextProxyRow_;
    ++attempts_;
    return proxyRow;
}

int TryNextPreviewState::Attempts() const
{
    return attempts_;
}
