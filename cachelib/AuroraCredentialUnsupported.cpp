#include "AuroraCredentialInternal.h"

AuroraCredentialError LoadAuroraNativeCredential(
    AuroraCacheKey& output)
{
    output = {};
    return AuroraCredentialError::UnsupportedPlatform;
}

std::filesystem::path DefaultAuroraCacheKeyFile()
{
    return {};
}
