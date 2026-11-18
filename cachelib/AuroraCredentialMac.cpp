#include "AuroraCredentialInternal.h"

#include <array>
#include <cstdlib>

#include <openssl/crypto.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

namespace
{
    class CfObject
    {
    public:
        explicit CfObject(CFTypeRef value = nullptr)
            : value_(value)
        {
        }

        ~CfObject()
        {
            if (value_ != nullptr)
            {
                CFRelease(value_);
            }
        }

        CfObject(const CfObject&) = delete;
        CfObject& operator=(const CfObject&) = delete;

        CFTypeRef Get() const
        {
            return value_;
        }

    private:
        CFTypeRef value_ = nullptr;
    };

    CFStringRef MakeString(const char* value)
    {
        return CFStringCreateWithCString(
            kCFAllocatorDefault,
            value,
            kCFStringEncodingUTF8);
    }
}

AuroraCredentialError LoadAuroraNativeCredential(
    AuroraCacheKey& output)
{
    output = {};

    CfObject service(MakeString(AuroraCredentialService));
    CfObject account(MakeString(AuroraCredentialAccount));
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CfObject query(dictionary);

    if (service.Get() == nullptr ||
        account.Get() == nullptr ||
        query.Get() == nullptr)
    {
        return AuroraCredentialError::ReadError;
    }

    CFDictionarySetValue(dictionary, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(dictionary, kSecAttrService, service.Get());
    CFDictionarySetValue(dictionary, kSecAttrAccount, account.Get());
    CFDictionarySetValue(dictionary, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(dictionary, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef rawResult = nullptr;
    const OSStatus status = SecItemCopyMatching(
        dictionary,
        &rawResult);
    CfObject result(rawResult);

    if (status == errSecItemNotFound)
    {
        return AuroraCredentialError::NotFound;
    }

    if (status != errSecSuccess)
    {
        return AuroraCredentialError::ReadError;
    }

    if (result.Get() == nullptr ||
        CFGetTypeID(result.Get()) != CFDataGetTypeID())
    {
        return AuroraCredentialError::InvalidKey;
    }

    const auto data = static_cast<CFDataRef>(result.Get());
    if (CFDataGetLength(data) !=
        static_cast<CFIndex>(AuroraCacheKey::Size))
    {
        return AuroraCredentialError::InvalidKey;
    }

    std::array<std::uint8_t, AuroraCacheKey::Size> bytes{};
    CFDataGetBytes(
        data,
        CFRangeMake(0, static_cast<CFIndex>(bytes.size())),
        bytes.data());

    output = AuroraCacheKey::FromBytes(bytes);
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return AuroraCredentialError::None;
}

std::filesystem::path DefaultAuroraCacheKeyFile()
{
    const char* home = std::getenv("HOME");
    return home == nullptr || *home == '\0'
        ? std::filesystem::path()
        : std::filesystem::path(home) / "Library" /
            "Application Support" / "org.Aurora.AuroraViewer" /
            "cache.key";
}
