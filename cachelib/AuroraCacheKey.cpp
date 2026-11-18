#include "AuroraTextureKey.h"

#include "AuroraCrypto.h"

#include <utility>

#include <openssl/crypto.h>

namespace
{
    constexpr char NamesSalt[] = "aurora cache names";
    constexpr char NamesInfo[] = "aurora cache names v1";
}

AuroraCacheKey::~AuroraCacheKey()
{
    Clear();
}

AuroraCacheKey::AuroraCacheKey(AuroraCacheKey&& other) noexcept
    : bytes_(other.bytes_),
      valid_(other.valid_)
{
    other.Clear();
}

AuroraCacheKey& AuroraCacheKey::operator=(
    AuroraCacheKey&& other) noexcept
{
    if (this != &other)
    {
        Clear();
        bytes_ = other.bytes_;
        valid_ = other.valid_;
        other.Clear();
    }

    return *this;
}

AuroraCacheKey AuroraCacheKey::FromBytes(
    const std::array<std::uint8_t, Size>& bytes)
{
    AuroraCacheKey key;
    key.bytes_ = bytes;
    key.valid_ = true;
    return key;
}

void AuroraCacheKey::Clear() noexcept
{
    OPENSSL_cleanse(bytes_.data(), bytes_.size());
    valid_ = false;
}

std::optional<std::string> AuroraKeyedTextureName(
    const AuroraCacheKey& key,
    const UUID& uuid)
{
    if (!key.IsValid())
    {
        return std::nullopt;
    }

    std::array<std::uint8_t, 32> namesKey{};
    const bool derived = AuroraHkdfSha256(
        key.Bytes().data(),
        key.Bytes().size(),
        reinterpret_cast<const std::uint8_t*>(NamesSalt),
        sizeof(NamesSalt) - 1,
        reinterpret_cast<const std::uint8_t*>(NamesInfo),
        sizeof(NamesInfo) - 1,
        namesKey.data(),
        namesKey.size());

    if (!derived)
    {
        OPENSSL_cleanse(namesKey.data(), namesKey.size());
        return std::nullopt;
    }

    std::array<std::uint8_t, 32> digest{};
    const bool signedName = AuroraHmacSha256(
        namesKey.data(),
        namesKey.size(),
        uuid.Bytes().data(),
        uuid.Bytes().size(),
        digest.data(),
        digest.size());

    OPENSSL_cleanse(namesKey.data(), namesKey.size());

    if (!signedName)
    {
        OPENSSL_cleanse(digest.data(), digest.size());
        return std::nullopt;
    }

    constexpr char HexDigits[] = "0123456789abcdef";
    std::string name;
    name.reserve(32);

    for (std::size_t i = 0; i < 16; ++i)
    {
        name.push_back(HexDigits[digest[i] >> 4]);
        name.push_back(HexDigits[digest[i] & 0x0f]);
    }

    OPENSSL_cleanse(digest.data(), digest.size());
    return name;
}
