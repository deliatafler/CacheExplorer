#pragma once

#include <cstddef>
#include <cstdint>

enum class AuroraDecryptResult
{
    Succeeded,
    AuthenticationFailed,
    CryptoError
};

bool AuroraHkdfSha256(
    const std::uint8_t* key,
    std::size_t keySize,
    const std::uint8_t* salt,
    std::size_t saltSize,
    const std::uint8_t* info,
    std::size_t infoSize,
    std::uint8_t* output,
    std::size_t outputSize);

bool AuroraHmacSha256(
    const std::uint8_t* key,
    std::size_t keySize,
    const std::uint8_t* data,
    std::size_t dataSize,
    std::uint8_t* output,
    std::size_t outputSize);

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
    std::uint8_t* plaintext);
