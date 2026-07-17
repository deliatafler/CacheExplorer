#pragma once

#include <cstdint>

#include <QString>

struct CacheEntry;

QString TextureDetailsText(
    const CacheEntry& entry,
    std::uint32_t width = 0,
    std::uint32_t height = 0);
