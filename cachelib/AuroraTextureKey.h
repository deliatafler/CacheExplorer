#pragma once

#include "AuroraCacheKey.h"
#include "UUID.h"

#include <optional>
#include <string>

std::optional<std::string> AuroraKeyedTextureName(
    const AuroraCacheKey& key,
    const UUID& uuid);
