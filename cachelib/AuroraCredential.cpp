#include "AuroraCredential.h"

#include "AuroraCredentialInternal.h"

#include <array>
#include <fstream>
#include <memory>
#include <system_error>

#include <openssl/crypto.h>

namespace
{
    class DefaultAuroraCacheKeyProvider final
        : public AuroraCacheKeyProvider
    {
    public:
        AuroraCredentialResult Load(
            AuroraCacheKey& output) const override
        {
            output = {};

            const AuroraCredentialError nativeError =
                LoadAuroraNativeCredential(output);

            if (nativeError == AuroraCredentialError::None)
            {
                return {
                    AuroraCredentialError::None,
                    AuroraCredentialSource::NativeStore};
            }

            if (nativeError == AuroraCredentialError::InvalidKey)
            {
                return {nativeError, AuroraCredentialSource::None};
            }

            const AuroraCredentialError fileError =
                LoadAuroraCacheKeyFile(
                    DefaultAuroraCacheKeyFile(),
                    output);

            if (fileError == AuroraCredentialError::None)
            {
                return {
                    AuroraCredentialError::None,
                    AuroraCredentialSource::ConfigFile};
            }

            if (fileError != AuroraCredentialError::NotFound)
            {
                return {fileError, AuroraCredentialSource::None};
            }

            return {nativeError, AuroraCredentialSource::None};
        }
    };
}

const char* AuroraCredentialErrorMessage(AuroraCredentialError error)
{
    switch (error)
    {
        case AuroraCredentialError::None:
            return "No error.";

        case AuroraCredentialError::UnsupportedPlatform:
            return "Aurora credential access is not supported on this platform yet.";

        case AuroraCredentialError::NotFound:
            return "The Aurora cache key was not found in the credential store.";

        case AuroraCredentialError::ReadError:
            return "The Aurora cache key could not be read from the credential store.";

        case AuroraCredentialError::InvalidKey:
            return "The Aurora cache key in the credential store is malformed.";
    }

    return "Unknown Aurora credential error.";
}

const char* AuroraCredentialSourceName(
    AuroraCredentialSource source)
{
    switch (source)
    {
        case AuroraCredentialSource::None:
            return "Unavailable";

        case AuroraCredentialSource::NativeStore:
            return "System credential store";

        case AuroraCredentialSource::ConfigFile:
            return "Aurora configuration key file";
    }

    return "Unknown";
}

std::shared_ptr<const AuroraCacheKeyProvider>
CreateAuroraCacheKeyProvider()
{
    return std::make_shared<DefaultAuroraCacheKeyProvider>();
}

AuroraCredentialError LoadAuroraCacheKeyFile(
    const std::filesystem::path& path,
    AuroraCacheKey& output)
{
    output = {};

    if (path.empty())
    {
        return AuroraCredentialError::NotFound;
    }

    std::error_code error;
    const bool isFile = std::filesystem::is_regular_file(path, error);

    if (error)
    {
        return error.default_error_condition() ==
                std::make_error_condition(
                    std::errc::no_such_file_or_directory)
            ? AuroraCredentialError::NotFound
            : AuroraCredentialError::ReadError;
    }

    if (!isFile)
    {
        return AuroraCredentialError::NotFound;
    }

    const std::uintmax_t size =
        std::filesystem::file_size(path, error);

    if (error)
    {
        return AuroraCredentialError::ReadError;
    }

    if (size != AuroraCacheKey::Size)
    {
        return AuroraCredentialError::InvalidKey;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return AuroraCredentialError::ReadError;
    }

    std::array<std::uint8_t, AuroraCacheKey::Size> bytes{};
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));

    if (input.gcount() !=
            static_cast<std::streamsize>(bytes.size()) ||
        input.bad())
    {
        OPENSSL_cleanse(bytes.data(), bytes.size());
        return AuroraCredentialError::ReadError;
    }

    output = AuroraCacheKey::FromBytes(bytes);
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return AuroraCredentialError::None;
}
