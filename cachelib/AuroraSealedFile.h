#pragma once

#include "AuroraCacheFormat.h"
#include "AuroraCacheKey.h"

#include <cstdint>
#include <filesystem>
#include <vector>

AuroraFormatError OpenAuroraSealedBytes(
    const std::vector<std::uint8_t>& sealedBytes,
    const AuroraCacheKey& key,
    std::vector<std::uint8_t>& plaintext);

AuroraFormatError OpenAuroraSealedFile(
    const std::filesystem::path& filename,
    const AuroraCacheKey& key,
    std::vector<std::uint8_t>& plaintext);
