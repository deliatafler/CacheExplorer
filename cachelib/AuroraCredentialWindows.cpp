#include "AuroraCredentialInternal.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include <openssl/crypto.h>

#include <windows.h>
#include <wincred.h>

namespace
{
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

    std::filesystem::path RoamingAppData()
    {
        DWORD required = GetEnvironmentVariableW(
            L"APPDATA",
            nullptr,
            0);

        if (required == 0)
        {
            return {};
        }

        std::vector<wchar_t> value(required);
        const DWORD copied = GetEnvironmentVariableW(
            L"APPDATA",
            value.data(),
            required);

        if (copied == 0 || copied >= required)
        {
            return {};
        }

        return std::filesystem::path(value.data());
    }
}

AuroraCredentialError LoadAuroraNativeCredential(
    AuroraCacheKey& output)
{
    output = {};

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
}

std::filesystem::path DefaultAuroraCacheKeyFile()
{
    const std::filesystem::path appData = RoamingAppData();
    return appData.empty()
        ? std::filesystem::path()
        : appData / "Aurora" / "AuroraViewer" /
            "config" / "cache.key";
}
