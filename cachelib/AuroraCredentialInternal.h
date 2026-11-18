#pragma once

#include "AuroraCredential.h"

inline constexpr char AuroraCredentialService[] =
    "Aurora Viewer cache";
inline constexpr char AuroraCredentialAccount[] =
    "disk cache key v1";

AuroraCredentialError LoadAuroraNativeCredential(
    AuroraCacheKey& output);
