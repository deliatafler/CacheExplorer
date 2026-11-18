#include "AuroraSealedFile.h"

#include "AuroraCrypto.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>

#include <openssl/crypto.h>

namespace
{
    constexpr char FileKeyInfo[] = "aurora cache file v1";
    constexpr std::size_t SaltOffset = 4;
    constexpr std::size_t SaltSize = 16;
    constexpr std::size_t NonceSize = 12;

    std::array<std::uint8_t, NonceSize> ChunkNonce(
        std::uint64_t chunk)
    {
        std::array<std::uint8_t, NonceSize> nonce{};

        for (std::size_t i = 0; i < 8; ++i)
        {
            nonce[NonceSize - 1 - i] =
                static_cast<std::uint8_t>(
                    (chunk >> (i * 8)) & 0xff);
        }

        return nonce;
    }

    AuroraFormatError MissingOrReadError(
        const std::filesystem::path& filename)
    {
        std::error_code error;
        const bool exists =
            std::filesystem::exists(filename, error);

        return !error && !exists
            ? AuroraFormatError::FileNotFound
            : AuroraFormatError::ReadError;
    }
}

AuroraFormatError OpenAuroraSealedBytes(
    const std::vector<std::uint8_t>& sealedBytes,
    const AuroraCacheKey& key,
    std::vector<std::uint8_t>& plaintext)
{
    plaintext.clear();

    if (!key.IsValid())
    {
        return AuroraFormatError::CryptoError;
    }

    AuroraSealedFileInfo info;
    const AuroraFormatError inspectError =
        InspectAuroraSealedBytes(sealedBytes, info);

    if (inspectError != AuroraFormatError::None)
    {
        return inspectError;
    }

    if (info.plaintextBytes >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max()))
    {
        return AuroraFormatError::SizeMismatch;
    }

    std::array<std::uint8_t, AuroraCacheKey::Size> fileKey{};
    const bool derived = AuroraHkdfSha256(
        key.Bytes().data(),
        key.Bytes().size(),
        sealedBytes.data() + SaltOffset,
        SaltSize,
        reinterpret_cast<const std::uint8_t*>(FileKeyInfo),
        sizeof(FileKeyInfo) - 1,
        fileKey.data(),
        fileKey.size());

    if (!derived)
    {
        OPENSSL_cleanse(fileKey.data(), fileKey.size());
        return AuroraFormatError::CryptoError;
    }

    plaintext.resize(
        static_cast<std::size_t>(info.plaintextBytes));

    std::size_t readOffset = AuroraSealedHeaderSize;
    std::size_t writeOffset = 0;
    std::uint64_t bytesRemaining = info.plaintextBytes;
    std::uint8_t emptyPlaintext = 0;

    for (std::uint64_t chunk = 0;
         chunk < info.chunkCount;
         ++chunk)
    {
        const std::size_t chunkBytes =
            static_cast<std::size_t>(
                std::min<std::uint64_t>(
                    bytesRemaining,
                    AuroraSealedChunkSize));
        const std::array<std::uint8_t, NonceSize> nonce =
            ChunkNonce(chunk);
        std::uint8_t* destination = chunkBytes > 0
            ? plaintext.data() + writeOffset
            : &emptyPlaintext;

        const AuroraDecryptResult decryptResult =
            AuroraAes256GcmDecrypt(
                fileKey.data(),
                fileKey.size(),
                nonce.data(),
                nonce.size(),
                sealedBytes.data(),
                AuroraSealedHeaderSize,
                sealedBytes.data() + readOffset,
                chunkBytes,
                sealedBytes.data() + readOffset + chunkBytes,
                AuroraSealedTagSize,
                destination);

        if (decryptResult != AuroraDecryptResult::Succeeded)
        {
            OPENSSL_cleanse(fileKey.data(), fileKey.size());
            if (!plaintext.empty())
            {
                OPENSSL_cleanse(
                    plaintext.data(),
                    plaintext.size());
            }
            plaintext.clear();
            return decryptResult ==
                    AuroraDecryptResult::AuthenticationFailed
                ? AuroraFormatError::AuthenticationFailed
                : AuroraFormatError::CryptoError;
        }

        readOffset += chunkBytes + AuroraSealedTagSize;
        writeOffset += chunkBytes;
        bytesRemaining -= chunkBytes;
    }

    OPENSSL_cleanse(fileKey.data(), fileKey.size());
    return AuroraFormatError::None;
}

AuroraFormatError OpenAuroraSealedFile(
    const std::filesystem::path& filename,
    const AuroraCacheKey& key,
    std::vector<std::uint8_t>& plaintext)
{
    plaintext.clear();

    std::ifstream file(
        filename,
        std::ios::binary | std::ios::ate);

    if (!file)
    {
        return MissingOrReadError(filename);
    }

    const std::streampos end = file.tellg();
    if (end < 0 ||
        static_cast<std::uint64_t>(end) >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max()) ||
        static_cast<std::uint64_t>(end) >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::streamsize>::max()))
    {
        return AuroraFormatError::ReadError;
    }

    std::vector<std::uint8_t> sealedBytes(
        static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);

    if (!sealedBytes.empty())
    {
        file.read(
            reinterpret_cast<char*>(sealedBytes.data()),
            static_cast<std::streamsize>(sealedBytes.size()));
    }

    if (!file)
    {
        return AuroraFormatError::ReadError;
    }

    return OpenAuroraSealedBytes(
        sealedBytes,
        key,
        plaintext);
}
