#include "PreviewWorkerState.h"

#include <chrono>
#include <utility>

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

void PreviewWorkerState::StartDecode(
    std::uint64_t requestId,
    const std::filesystem::path& cacheDirectory,
    const CacheEntry& entry)
{
    Start(
        requestId,
        std::async(
            std::launch::async,
            [requestId, cacheDirectory, entry]()
            {
                return DecodePreview(requestId, cacheDirectory, entry);
            }));
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
