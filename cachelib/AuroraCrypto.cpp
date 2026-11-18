#include "AuroraCrypto.h"

#include <array>
#include <limits>
#include <memory>

#include <openssl/evp.h>
#include <openssl/kdf.h>

namespace
{
    bool FitsOpenSslInt(std::size_t value)
    {
        return value <=
            static_cast<std::size_t>(
                std::numeric_limits<int>::max());
    }
}

bool AuroraHkdfSha256(
    const std::uint8_t* key,
    std::size_t keySize,
    const std::uint8_t* salt,
    std::size_t saltSize,
    const std::uint8_t* info,
    std::size_t infoSize,
    std::uint8_t* output,
    std::size_t outputSize)
{
    if (key == nullptr || output == nullptr ||
        (saltSize > 0 && salt == nullptr) ||
        (infoSize > 0 && info == nullptr) ||
        !FitsOpenSslInt(keySize) ||
        !FitsOpenSslInt(saltSize) ||
        !FitsOpenSslInt(infoSize))
    {
        return false;
    }

    using Context =
        std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

    Context context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr),
        &EVP_PKEY_CTX_free);

    if (!context ||
        EVP_PKEY_derive_init(context.get()) <= 0 ||
        EVP_PKEY_CTX_set_hkdf_md(
            context.get(),
            EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_salt(
            context.get(),
            salt,
            static_cast<int>(saltSize)) <= 0 ||
        EVP_PKEY_CTX_set1_hkdf_key(
            context.get(),
            key,
            static_cast<int>(keySize)) <= 0 ||
        EVP_PKEY_CTX_add1_hkdf_info(
            context.get(),
            info,
            static_cast<int>(infoSize)) <= 0)
    {
        return false;
    }

    std::size_t derivedSize = outputSize;
    return EVP_PKEY_derive(
               context.get(),
               output,
               &derivedSize) > 0 &&
        derivedSize == outputSize;
}

bool AuroraHmacSha256(
    const std::uint8_t* key,
    std::size_t keySize,
    const std::uint8_t* data,
    std::size_t dataSize,
    std::uint8_t* output,
    std::size_t outputSize)
{
    if (key == nullptr || output == nullptr ||
        (dataSize > 0 && data == nullptr))
    {
        return false;
    }

    using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

    Key hmacKey(
        EVP_PKEY_new_raw_private_key(
            EVP_PKEY_HMAC,
            nullptr,
            key,
            keySize),
        &EVP_PKEY_free);
    Context context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);

    if (!hmacKey || !context ||
        EVP_DigestSignInit(
            context.get(),
            nullptr,
            EVP_sha256(),
            nullptr,
            hmacKey.get()) <= 0 ||
        EVP_DigestSignUpdate(
            context.get(),
            data,
            dataSize) <= 0)
    {
        return false;
    }

    std::size_t digestSize = outputSize;
    return EVP_DigestSignFinal(
               context.get(),
               output,
               &digestSize) > 0 &&
        digestSize == outputSize;
}

AuroraDecryptResult AuroraAes256GcmDecrypt(
    const std::uint8_t* key,
    std::size_t keySize,
    const std::uint8_t* nonce,
    std::size_t nonceSize,
    const std::uint8_t* additionalData,
    std::size_t additionalDataSize,
    const std::uint8_t* ciphertext,
    std::size_t ciphertextSize,
    const std::uint8_t* tag,
    std::size_t tagSize,
    std::uint8_t* plaintext)
{
    if (key == nullptr || keySize != 32 ||
        nonce == nullptr || tag == nullptr ||
        additionalData == nullptr || plaintext == nullptr ||
        !FitsOpenSslInt(nonceSize) ||
        !FitsOpenSslInt(additionalDataSize) ||
        !FitsOpenSslInt(ciphertextSize) ||
        !FitsOpenSslInt(tagSize))
    {
        return AuroraDecryptResult::CryptoError;
    }

    using Context =
        std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

    Context context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!context ||
        EVP_DecryptInit_ex(
            context.get(),
            EVP_aes_256_gcm(),
            nullptr,
            nullptr,
            nullptr) <= 0 ||
        EVP_CIPHER_CTX_ctrl(
            context.get(),
            EVP_CTRL_GCM_SET_IVLEN,
            static_cast<int>(nonceSize),
            nullptr) <= 0 ||
        EVP_DecryptInit_ex(
            context.get(),
            nullptr,
            nullptr,
            key,
            nonce) <= 0)
    {
        return AuroraDecryptResult::CryptoError;
    }

    int written = 0;
    if (EVP_DecryptUpdate(
            context.get(),
            nullptr,
            &written,
            additionalData,
            static_cast<int>(additionalDataSize)) <= 0)
    {
        return AuroraDecryptResult::CryptoError;
    }

    if (ciphertextSize > 0 &&
        (ciphertext == nullptr ||
         EVP_DecryptUpdate(
             context.get(),
             plaintext,
             &written,
             ciphertext,
             static_cast<int>(ciphertextSize)) <= 0 ||
         written != static_cast<int>(ciphertextSize)))
    {
        return AuroraDecryptResult::CryptoError;
    }

    if (EVP_CIPHER_CTX_ctrl(
            context.get(),
            EVP_CTRL_GCM_SET_TAG,
            static_cast<int>(tagSize),
            const_cast<std::uint8_t*>(tag)) <= 0)
    {
        return AuroraDecryptResult::CryptoError;
    }

    int finalBytes = 0;
    return EVP_DecryptFinal_ex(
               context.get(),
               plaintext + ciphertextSize,
               &finalBytes) > 0
        ? AuroraDecryptResult::Succeeded
        : AuroraDecryptResult::AuthenticationFailed;
}
