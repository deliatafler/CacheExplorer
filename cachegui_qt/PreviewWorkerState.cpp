#include "PreviewWorkerState.h"

#include <chrono>

bool PreviewWorkerState::IsActive() const
{
    return active_;
}

std::uint64_t PreviewWorkerState::ActiveRequestId() const
{
    return activeRequestId_;
}

void PreviewWorkerState::Start(
    std::uint64_t requestId,
    std::future<PreviewDecodeResult>&& future)
{
    active_ = true;
    activeRequestId_ = requestId;
    future_ = std::move(future);
}

bool PreviewWorkerState::IsReady() const
{
    return active_ &&
        future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

PreviewDecodeResult PreviewWorkerState::TakeResult()
{
    PreviewDecodeResult result = future_.get();
    Clear();
    return result;
}

void PreviewWorkerState::Clear()
{
    active_ = false;
    activeRequestId_ = 0;
}
