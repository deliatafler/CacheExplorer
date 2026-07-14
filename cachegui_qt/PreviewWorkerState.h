#pragma once

#include "PreviewDecodeWorker.h"

#include <cstdint>
#include <filesystem>
#include <future>

class PreviewWorkerState
{
public:
    bool IsActive() const;
    std::uint64_t ActiveRequestId() const;

    void Start(
        std::uint64_t requestId,
        std::future<PreviewDecodeResult>&& future);
    void StartDecode(
        std::uint64_t requestId,
        const std::filesystem::path& cacheDirectory,
        const CacheEntry& entry);

    bool IsReady() const;
    PreviewDecodeResult TakeResult();
    void Clear();

private:
    bool active_ = false;
    std::uint64_t activeRequestId_ = 0;
    std::future<PreviewDecodeResult> future_;
};
