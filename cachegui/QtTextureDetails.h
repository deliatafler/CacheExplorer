#pragma once

#include <cstdint>

#include <QString>

struct TextureCatalogEntry;

QString TextureDetailsText(
    const TextureCatalogEntry& entry,
    std::uint32_t width = 0,
    std::uint32_t height = 0);
