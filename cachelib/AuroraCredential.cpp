#include "AuroraCacheKey.h"

#include <algorithm>
#include <array>

#include <openssl/crypto.h>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

namespace
{
#ifdef _WIN32
    constexpr wchar_t WindowsCredentialTarget[] =
        L"disk cache key v1.Aurora Viewer cache";

    class CredentialHandle
    {
    public:
        explicit CredentialHandle(PCREDENTIALW credential)
            : credential_(credential)
        {
        }

        ~CredentialHandle()
        {
            if (credential_ != nullptr)
            {
                CredFree(credential_);
            }
        }

        CredentialHandle(const CredentialHandle&) = delete;
        CredentialHandle& operator=(const CredentialHandle&) = delete;

    private:
        PCREDENTIALW credential_ = nullptr;
    };
#endif
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

AuroraCredentialError LoadAuroraCacheKey(
    AuroraCacheKey& output)
{
    output = {};

#ifdef _WIN32
    PCREDENTIALW rawCredential = nullptr;
    if (!CredReadW(
            WindowsCredentialTarget,
            CRED_TYPE_GENERIC,
            0,
            &rawCredential))
    {
        return GetLastError() == ERROR_NOT_FOUND
            ? AuroraCredentialError::NotFound
            : AuroraCredentialError::ReadError;
    }

    CredentialHandle credential(rawCredential);

    if (rawCredential == nullptr ||
        rawCredential->CredentialBlob == nullptr ||
        rawCredential->CredentialBlobSize != AuroraCacheKey::Size)
    {
        return AuroraCredentialError::InvalidKey;
    }

    std::array<std::uint8_t, AuroraCacheKey::Size> bytes{};
    std::copy_n(
        rawCredential->CredentialBlob,
        bytes.size(),
        bytes.begin());

    output = AuroraCacheKey::FromBytes(bytes);
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return AuroraCredentialError::None;
#else
    return AuroraCredentialError::UnsupportedPlatform;
#endif
}
