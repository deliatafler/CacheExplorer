#pragma once

#include "AuroraCacheKey.h"

#include <filesystem>
#include <memory>

enum class AuroraCredentialError
{
    None,
    UnsupportedPlatform,
    NotFound,
    ReadError,
    InvalidKey
};

enum class AuroraCredentialSource
{
    None,
    NativeStore,
    ConfigFile
};

struct AuroraCredentialResult
{
    AuroraCredentialError error = AuroraCredentialError::None;
    AuroraCredentialSource source = AuroraCredentialSource::None;

    bool Succeeded() const
    {
        return error == AuroraCredentialError::None;
    }
};

class AuroraCacheKeyProvider
{
public:
    virtual ~AuroraCacheKeyProvider() = default;

    virtual AuroraCredentialResult Load(
        AuroraCacheKey& output) const = 0;
};

const char* AuroraCredentialErrorMessage(AuroraCredentialError error);
const char* AuroraCredentialSourceName(AuroraCredentialSource source);

std::shared_ptr<const AuroraCacheKeyProvider>
CreateAuroraCacheKeyProvider();

std::filesystem::path DefaultAuroraCacheKeyFile();

AuroraCredentialError LoadAuroraCacheKeyFile(
    const std::filesystem::path& path,
    AuroraCacheKey& output);
