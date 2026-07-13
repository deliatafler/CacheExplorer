#pragma once

#include <cstdint>

#include "UUID.h"

#pragma pack(push, 1)

struct EntriesInfo
{
    float version = 0.0f;

    std::uint32_t addressSize = 0;

    char encoderVersion[32]{};

    std::uint32_t entryCount = 0;
};

struct Entry
{
    UUID uuid;

    std::int32_t imageSize = 0;

    std::int32_t bodySize = 0;

    std::uint32_t timestamp = 0;
};

#pragma pack(pop)

static_assert(
    sizeof(EntriesInfo) == 44,
    "EntriesInfo has an unexpected layout.");

static_assert(
    sizeof(Entry) == 28,
    "Entry has an unexpected layout.");
