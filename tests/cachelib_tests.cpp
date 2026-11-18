#include "AuroraCacheFormat.h"
#include "AuroraCacheKey.h"
#include "AuroraCacheProbe.h"
#include "AuroraSealedFile.h"
#include "AuroraTextureCacheSource.h"
#include "AuroraTextureKey.h"
#include "Structures.h"
#include "LegacyTextureCacheSource.h"
#include "TextureCacheDatabase.h"
#include "TextureCacheChanges.h"
#include "TextureExportState.h"
#include "TextureRebuilder.h"
#include "TextureSelection.h"
#include "UUID.h"
#include "ViewerTextureCacheSource.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    int gFailures = 0;

    void Expect(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++gFailures;
        }
    }

    UUID MakeUuid(const char* value)
    {
        const std::optional<UUID> uuid = UUID::FromString(value);

        if (!uuid)
        {
            std::cerr << "FAIL: invalid test UUID: " << value << '\n';
            ++gFailures;
            return {};
        }

        return *uuid;
    }

    fs::path MakeTempDirectory()
    {
        const auto stamp =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();

        fs::path directory =
            fs::temp_directory_path() /
            ("cacheexplorer-cachelib-tests-" +
             std::to_string(stamp));

        std::error_code error;
        fs::create_directories(directory, error);

        Expect(!error, "create temporary test directory");

        return directory;
    }

    void WriteTextureEntries(
        const fs::path& cacheDirectory,
        const std::vector<Entry>& entries)
    {
        EntriesInfo header{};
        header.version = 1.0f;
        header.addressSize = 600;
        header.entryCount =
            static_cast<std::uint32_t>(entries.size());

        const char encoder[] = "cachelib-tests";
        std::memcpy(
            header.encoderVersion,
            encoder,
            sizeof(encoder));

        std::ofstream file(
            cacheDirectory / "texture.entries",
            std::ios::binary);

        Expect(file.good(), "open synthetic texture.entries");

        file.write(
            reinterpret_cast<const char*>(&header),
            static_cast<std::streamsize>(sizeof(header)));

        for (const Entry& entry : entries)
        {
            file.write(
                reinterpret_cast<const char*>(&entry),
                static_cast<std::streamsize>(sizeof(entry)));
        }

        Expect(file.good(), "write synthetic texture.entries");
    }

    void WriteBytes(
        const fs::path& filename,
        const std::vector<std::uint8_t>& bytes)
    {
        const fs::path parent = filename.parent_path();

        if (!parent.empty())
        {
            std::error_code error;
            fs::create_directories(parent, error);
            Expect(!error, "create parent directory for binary fixture");
        }

        std::ofstream file(filename, std::ios::binary);
        Expect(file.good(), "open binary fixture for writing");

        if (!bytes.empty())
        {
            file.write(
                reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }

        Expect(file.good(), "write binary fixture");
    }

    void WriteTextureCache(
        const fs::path& cacheDirectory,
        const std::vector<std::uint8_t>& bytes)
    {
        WriteBytes(cacheDirectory / "texture.cache", bytes);
    }

    void WriteBodyFile(
        const fs::path& cacheDirectory,
        const CacheEntry& entry,
        const std::vector<std::uint8_t>& bytes)
    {
        WriteBytes(
            TextureRebuilder::BodyFilePath(cacheDirectory, entry),
            bytes);
    }

    void SetLittleEndian32(
        std::vector<std::uint8_t>& bytes,
        std::size_t offset,
        std::uint32_t value)
    {
        bytes[offset] =
            static_cast<std::uint8_t>(value & 0xff);
        bytes[offset + 1] =
            static_cast<std::uint8_t>((value >> 8) & 0xff);
        bytes[offset + 2] =
            static_cast<std::uint8_t>((value >> 16) & 0xff);
        bytes[offset + 3] =
            static_cast<std::uint8_t>((value >> 24) & 0xff);
    }

    void SetLittleEndian64(
        std::vector<std::uint8_t>& bytes,
        std::size_t offset,
        std::uint64_t value)
    {
        for (std::size_t i = 0; i < 8; ++i)
        {
            bytes[offset + i] =
                static_cast<std::uint8_t>(
                    (value >> (i * 8)) & 0xff);
        }
    }

    std::uint64_t TestBc7LevelBytes(
        std::uint32_t width,
        std::uint32_t height)
    {
        return
            ((static_cast<std::uint64_t>(width) + 3) / 4) *
            ((static_cast<std::uint64_t>(height) + 3) / 4) *
            16;
    }

    std::vector<std::uint8_t> MakeAuroraSealedFixture(
        std::uint64_t plaintextBytes)
    {
        const std::uint64_t chunks =
            std::max<std::uint64_t>(
                1,
                (plaintextBytes / AuroraSealedChunkSize) +
                    (plaintextBytes % AuroraSealedChunkSize != 0 ? 1 : 0));
        const std::uint64_t sealedBytes =
            AuroraSealedHeaderSize +
            plaintextBytes +
            chunks * AuroraSealedTagSize;

        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(sealedBytes),
            0x5a);

        bytes[0] = 'A';
        bytes[1] = 'U';
        bytes[2] = 'S';
        bytes[3] = 'L';
        SetLittleEndian64(bytes, 20, plaintextBytes);
        return bytes;
    }

    std::vector<std::uint8_t> MakeAuroraTranscodedFixture()
    {
        const std::vector<std::pair<std::uint32_t, std::uint32_t>> levels = {
            {16, 8},
            {8, 4},
            {4, 2},
            {2, 1},
            {1, 1},
        };

        std::uint64_t dataBytes = 0;
        for (const auto& level : levels)
        {
            dataBytes +=
                TestBc7LevelBytes(
                    level.first,
                    level.second);
        }

        std::vector<std::uint8_t> bytes(
            32 + levels.size() * 8 +
                static_cast<std::size_t>(dataBytes),
            0x7c);

        bytes[0] = 'A';
        bytes[1] = 'U';
        bytes[2] = 'T';
        bytes[3] = 'X';
        bytes[4] = 6;
        bytes[5] = 1;
        bytes[6] = 2;
        bytes[7] = 2;
        bytes[8] = 1;
        bytes[9] = 4;
        bytes[10] = 5;
        bytes[11] =
            static_cast<std::uint8_t>(levels.size());

        SetLittleEndian32(bytes, 12, 64);
        SetLittleEndian32(bytes, 16, 32);
        SetLittleEndian32(bytes, 20, 0);
        SetLittleEndian32(bytes, 24, 0);
        SetLittleEndian32(bytes, 28, 0);

        for (std::size_t i = 0; i < levels.size(); ++i)
        {
            SetLittleEndian32(
                bytes,
                32 + i * 8,
                levels[i].first);
            SetLittleEndian32(
                bytes,
                32 + i * 8 + 4,
                levels[i].second);
        }

        return bytes;
    }

    std::vector<std::uint8_t> BytesFromHex(
        const std::string& hex)
    {
        auto value = [](char character) -> int
        {
            if (character >= '0' && character <= '9')
            {
                return character - '0';
            }

            if (character >= 'a' && character <= 'f')
            {
                return character - 'a' + 10;
            }

            return -1;
        };

        if (hex.size() % 2 != 0)
        {
            Expect(false, "hex fixture has an even length");
            return {};
        }

        std::vector<std::uint8_t> bytes;
        bytes.reserve(hex.size() / 2);

        for (std::size_t i = 0; i < hex.size(); i += 2)
        {
            const int high = value(hex[i]);
            const int low = value(hex[i + 1]);

            if (high < 0 || low < 0)
            {
                Expect(false, "hex fixture contains valid digits");
                return {};
            }

            bytes.push_back(
                static_cast<std::uint8_t>((high << 4) | low));
        }

        return bytes;
    }

    std::array<std::uint8_t, AuroraCacheKey::Size>
    KnownAuroraKeyBytes()
    {
        std::array<std::uint8_t, AuroraCacheKey::Size> bytes{};

        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            bytes[i] = static_cast<std::uint8_t>(i);
        }

        return bytes;
    }

    std::vector<std::uint8_t> KnownAuroraSealedFixture()
    {
        return BytesFromHex(
            "4155534ca0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
            "1c00000000000000"
            "ba0f2f3d05a734eeab389cb73f4782880e433213ed9570c4"
            "37fb36e9"
            "fc0ecf5a9610f2b4f41fa7fdb03a8417");
    }

    std::vector<std::uint8_t> KnownAuroraPlaintext()
    {
        return BytesFromHex(
            "43616368654578706c6f726572204175726f72612066697874757265");
    }

    Entry MakeEntry(
        UUID uuid,
        std::int32_t imageSize,
        std::int32_t bodySize,
        std::uint32_t timestamp)
    {
        Entry entry{};
        entry.uuid = uuid;
        entry.imageSize = imageSize;
        entry.bodySize = bodySize;
        entry.timestamp = timestamp;
        return entry;
    }

    void TestDatabaseFiltersEntriesAndPreservesCacheIndex()
    {
        const UUID first =
            MakeUuid("11111111-2222-3333-4444-555555555555");
        const UUID skipped =
            MakeUuid("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        const UUID second =
            MakeUuid("66666666-7777-8888-9999-000000000000");

        const fs::path directory = MakeTempDirectory();

        WriteTextureEntries(
            directory,
            {
                MakeEntry(first, 700, 100, 1000),
                MakeEntry(skipped, -1, 0, 1001),
                MakeEntry(second, 600, 0, 1002),
                MakeEntry(skipped, 4096, 4096, 1003),
            });

        TextureCacheDatabase database;
        const CacheError error = database.Open(directory);

        Expect(
            error == CacheError::None,
            "open synthetic cache database");
        Expect(database.IsOpen(), "database is open");
        Expect(
            database.Header().entryCount == 4,
            "header keeps disk entry count");
        Expect(
            database.Entries().size() == 2,
            "database filters unusable entries");

        if (database.Entries().size() == 2)
        {
            const CacheEntry& firstEntry = database.Entries()[0];
            const CacheEntry& secondEntry = database.Entries()[1];

            Expect(
                firstEntry.uuid == first,
                "first usable entry UUID preserved");
            Expect(
                firstEntry.cacheIndex == 0,
                "first usable entry cacheIndex is raw record index");
            Expect(
                firstEntry.imageSize == 700,
                "first usable entry image size preserved");
            Expect(
                firstEntry.bodySize == 100,
                "first usable entry body size preserved");

            Expect(
                secondEntry.uuid == second,
                "second usable entry UUID preserved");
            Expect(
                secondEntry.cacheIndex == 2,
                "second usable entry cacheIndex skips filtered slot");
            Expect(
                secondEntry.imageSize == 600,
                "header-only usable entry is retained");
            Expect(
                secondEntry.bodySize == 0,
                "header-only usable entry body size preserved");
        }

        const CacheEntry* found = database.Find(second);
        Expect(found != nullptr, "find existing UUID");
        if (found != nullptr)
        {
            Expect(
                found->cacheIndex == 2,
                "find returns entry with preserved cacheIndex");
        }

        Expect(
            database.Find(skipped) == nullptr,
            "find ignores filtered entries");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove temporary test directory");
    }

    void TestCachedTextureCompletenessHelpers()
    {
        CacheEntry completeWithBody{};
        completeWithBody.imageSize = 700;
        completeWithBody.bodySize = 100;

        Expect(
            CachedTextureByteCount(completeWithBody) == 700,
            "cached byte count combines header and body bytes");
        Expect(
            HasCompleteCachedTexture(completeWithBody),
            "complete body-backed entry is cached complete");

        CacheEntry partial{};
        partial.imageSize = 4096;
        partial.bodySize = 512;

        Expect(
            CachedTextureByteCount(partial) == 1112,
            "partial cached byte count uses one header block plus body");
        Expect(
            !HasCompleteCachedTexture(partial),
            "partial cached entry is not cached complete");

        CacheEntry headerOnly{};
        headerOnly.imageSize = 600;
        headerOnly.bodySize = 0;

        Expect(
            CachedTextureByteCount(headerOnly) == 600,
            "header-only cached byte count handles body size zero");
        Expect(
            HasCompleteCachedTexture(headerOnly),
            "header-only entry can be cached complete");

        CacheEntry invalid{};
        invalid.imageSize = -1;
        invalid.bodySize = 0;

        Expect(
            CachedTextureByteCount(invalid) == 0,
            "invalid cached byte count is zero");
        Expect(
            !HasCompleteCachedTexture(invalid),
            "invalid entry is not cached complete");
    }

    void TestTextureCacheChangesFindsAddedAndUpdatedEntries()
    {
        CacheEntry unchanged{};
        unchanged.uuid =
            MakeUuid("11111111-2222-3333-4444-555555555555");
        unchanged.cacheIndex = 10;
        unchanged.imageSize = 700;
        unchanged.bodySize = 100;
        unchanged.timestamp = 1000;

        CacheEntry updated = unchanged;
        updated.uuid =
            MakeUuid("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        updated.cacheIndex = 20;

        CacheEntry removed = unchanged;
        removed.uuid =
            MakeUuid("99999999-8888-7777-6666-555555555555");
        removed.cacheIndex = 30;

        CacheEntry currentUpdate = updated;
        currentUpdate.bodySize = 200;
        currentUpdate.timestamp = 1001;

        CacheEntry added = unchanged;
        added.uuid =
            MakeUuid("66666666-7777-8888-9999-000000000000");
        added.cacheIndex = 40;

        const std::vector<std::uint32_t> changes = FindChangedCacheIndices(
            {unchanged, updated, removed},
            {unchanged, currentUpdate, added});

        Expect(
            changes == std::vector<std::uint32_t>{20, 40},
            "cache changes retain current order for updated and added entries");

        CacheEntry moved = unchanged;
        moved.cacheIndex = 11;
        Expect(
            FindChangedCacheIndices({unchanged}, {moved}) ==
                std::vector<std::uint32_t>{11},
            "cache changes treat a moved UUID as updated metadata");

        Expect(
            FindChangedCacheIndices({}, {}).empty(),
            "cache changes handle empty snapshots");
    }

    void TestTextureSelectionUsesFilteredDatabaseOrder()
    {
        const UUID first =
            MakeUuid("01234567-89ab-cdef-0123-456789abcdef");
        const UUID second =
            MakeUuid("fedcba98-7654-3210-fedc-ba9876543210");
        const UUID missing =
            MakeUuid("99999999-9999-9999-9999-999999999999");

        const fs::path directory = MakeTempDirectory();

        WriteTextureEntries(
            directory,
            {
                MakeEntry(first, 700, 100, 2000),
                MakeEntry(missing, -1, 0, 2001),
                MakeEntry(second, 601, 0, 2002),
            });

        TextureCacheDatabase database;
        Expect(
            database.Open(directory) == CacheError::None,
            "open synthetic cache for selection tests");

        const std::vector<const CacheEntry*> all =
            TextureSelection::All(database);
        Expect(all.size() == 2, "All selects all filtered entries");

        if (all.size() == 2)
        {
            Expect(
                all[0]->cacheIndex == 0,
                "All keeps filtered database order first");
            Expect(
                all[1]->cacheIndex == 2,
                "All keeps filtered database order second");
        }

        const std::vector<const CacheEntry*> range =
            TextureSelection::Range(database, 1, 5);
        Expect(range.size() == 1, "Range clamps to available entries");
        if (range.size() == 1)
        {
            Expect(
                range[0]->uuid == second,
                "Range selects by filtered vector index");
        }

        const std::vector<const CacheEntry*> emptyRange =
            TextureSelection::Range(database, 10, 1);
        Expect(emptyRange.empty(), "Range past end is empty");

        const std::vector<const CacheEntry*> byUuid =
            TextureSelection::ByUuid(database, {missing, second, first});
        Expect(
            byUuid.size() == 2,
            "ByUuid skips missing or filtered UUIDs");

        if (byUuid.size() == 2)
        {
            Expect(
                byUuid[0]->uuid == second,
                "ByUuid preserves requested UUID order");
            Expect(
                byUuid[1]->uuid == first,
                "ByUuid can return earlier database entries later");
        }

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove selection test directory");
    }

    void TestLegacyTextureCacheSourceProvidesNeutralCatalogAndPayload()
    {
        const UUID first =
            MakeUuid("11111111-2222-3333-4444-555555555555");
        const UUID second =
            MakeUuid("66666666-7777-8888-9999-000000000000");
        const UUID missing =
            MakeUuid("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

        const fs::path directory = MakeTempDirectory();

        WriteTextureEntries(
            directory,
            {
                MakeEntry(first, 700, 100, 3000),
                MakeEntry(missing, -1, 0, 3001),
                MakeEntry(second, 600, 0, 3002),
            });

        std::vector<std::uint8_t> headerBytes(
            TextureRebuilder::HeaderBlockSize * 3,
            0);

        for (std::size_t index = 0;
             index < TextureRebuilder::HeaderBlockSize;
             ++index)
        {
            headerBytes[index] =
                static_cast<std::uint8_t>(index % 251);
        }

        WriteTextureCache(directory, headerBytes);

        CacheEntry bodyEntry{};
        bodyEntry.uuid = first;

        std::vector<std::uint8_t> bodyBytes(100, 0x7c);
        WriteBodyFile(directory, bodyEntry, bodyBytes);

        LegacyTextureCacheSource source;
        TexturePayload payload;
        payload.bytes = {1, 2, 3};

        Expect(
            source.ReadPayload(first.ToString(), payload) ==
                TexturePayloadError::SourceNotOpen,
            "legacy source rejects payload reads before open");
        Expect(
            payload.bytes.empty(),
            "failed source payload read clears prior output");

        Expect(
            source.Open(directory / "missing") ==
                TextureCacheOpenError::FileNotFound,
            "legacy source maps missing texture.entries");
        Expect(!source.IsOpen(), "failed source open remains closed");

        Expect(
            source.Open(directory) == TextureCacheOpenError::None,
            "open legacy cache through neutral source");
        Expect(source.IsOpen(), "neutral legacy source is open");
        Expect(
            source.Format() == TextureCacheFormat::LegacyViewer,
            "neutral source identifies legacy format");
        Expect(
            source.CacheDirectory() == directory,
            "neutral source retains cache directory");
        Expect(
            source.Entries().size() == 2,
            "neutral source preserves filtered legacy entry count");

        if (source.Entries().size() == 2)
        {
            const TextureCatalogEntry& firstEntry = source.Entries()[0];
            const TextureCatalogEntry& secondEntry = source.Entries()[1];

            Expect(
                firstEntry.sourceKey == first.ToString(),
                "legacy catalog uses canonical UUID source key");
            Expect(
                firstEntry.uuid.has_value() && *firstEntry.uuid == first,
                "legacy catalog exposes known UUID");
            Expect(
                firstEntry.cachedBytes == 700,
                "legacy catalog exposes reconstructed cached byte count");
            Expect(
                firstEntry.payloadBytes.has_value() &&
                    *firstEntry.payloadBytes == 100,
                "legacy catalog exposes payload-file byte count");
            Expect(
                firstEntry.sourceIndex.has_value() &&
                    *firstEntry.sourceIndex == 0,
                "legacy catalog exposes raw source sequence");
            Expect(
                firstEntry.expectedBytes.has_value() &&
                    *firstEntry.expectedBytes == 700,
                "legacy catalog exposes advertised texture size");
            Expect(
                firstEntry.HasCompletePayload(),
                "legacy catalog reports complete body-backed payload");
            Expect(
                firstEntry.timestamp == 3000,
                "legacy catalog preserves timestamp");

            Expect(
                secondEntry.sourceKey == second.ToString(),
                "legacy catalog preserves filtered database order");
            Expect(
                secondEntry.cachedBytes == 600,
                "legacy catalog handles header-only cached byte count");
            Expect(
                secondEntry.payloadBytes.has_value() &&
                    *secondEntry.payloadBytes == 0,
                "legacy catalog preserves a zero-byte payload file");
            Expect(
                secondEntry.sourceIndex.has_value() &&
                    *secondEntry.sourceIndex == 2,
                "legacy catalog source sequence skips filtered slots");
            Expect(
                secondEntry.HasCompletePayload(),
                "legacy catalog reports complete header-only payload");
        }

        const TextureCatalogEntry* found = source.FindByUuid(first);
        Expect(found != nullptr, "neutral source finds known legacy UUID");
        if (found != nullptr)
        {
            Expect(
                found->sourceKey == first.ToString(),
                "neutral UUID lookup returns matching catalog entry");
        }

        Expect(
            source.FindByUuid(missing) == nullptr,
            "neutral source does not expose filtered legacy UUID");

        Expect(
            source.ReadPayload(first.ToString(), payload) ==
                TexturePayloadError::None,
            "neutral source reconstructs legacy JPEG2000 payload");
        Expect(
            payload.format == TexturePayloadFormat::Jpeg2000,
            "legacy source labels reconstructed payload as JPEG2000");
        Expect(
            payload.bytes.size() == 700,
            "legacy source returns exact header and body byte count");
        if (payload.bytes.size() == 700)
        {
            Expect(
                payload.bytes[599] ==
                    static_cast<std::uint8_t>(599 % 251),
                "legacy source preserves final header byte");
            Expect(
                payload.bytes[600] == 0x7c &&
                    payload.bytes.back() == 0x7c,
                "legacy source appends exact body bytes");
        }

        std::unique_ptr<TexturePayloadRequest> request =
            source.CreatePayloadRequest(first.ToString());
        Expect(
            request != nullptr,
            "legacy source creates an immutable payload request");
        source.Close();
        Expect(
            request != nullptr &&
                request->Read(payload) == TexturePayloadError::None,
            "payload request survives closing its source");
        Expect(
            payload.bytes.size() == 700,
            "detached payload request retains copied legacy metadata");

        Expect(
            source.Open(directory) == TextureCacheOpenError::None,
            "reopen legacy source after detached payload request");

        ViewerTextureCacheSource detectedSource;
        Expect(
            detectedSource.Open(directory) == TextureCacheOpenError::None &&
                detectedSource.Format() ==
                    TextureCacheFormat::LegacyViewer &&
                detectedSource.LegacySource() != nullptr,
            "auto-detecting source selects the legacy adapter");
        detectedSource.Close();

        std::error_code bodyRemoveError;
        fs::remove(
            TextureRebuilder::BodyFilePath(directory, bodyEntry),
            bodyRemoveError);
        Expect(
            !bodyRemoveError,
            "remove body fixture before missing-data source test");
        Expect(
            source.ReadPayload(first.ToString(), payload) ==
                TexturePayloadError::MissingData,
            "neutral source reports a body removed after cataloging");
        Expect(
            std::string(TexturePayloadErrorMessage(
                TexturePayloadError::MissingData)) ==
                "The cached texture data is no longer available.",
            "neutral source provides a user-facing payload error");
        Expect(
            payload.bytes.empty(),
            "missing source payload clears partial reconstructed bytes");

        Expect(
            source.ReadPayload("not-a-source-key", payload) ==
                TexturePayloadError::EntryNotFound,
            "neutral source rejects unknown source key");
        Expect(
            payload.bytes.empty(),
            "unknown source key clears prior payload output");

        source.Close();
        Expect(!source.IsOpen(), "neutral source closes legacy database");
        Expect(
            source.Entries().empty(),
            "neutral source clears catalog entries on close");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove neutral source test directory");
    }

    void TestTextureRebuilderUsesRawCacheIndexAndExactBodySize()
    {
        const UUID uuid =
            MakeUuid("2aaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

        CacheEntry entry{};
        entry.uuid = uuid;
        entry.cacheIndex = 2;
        entry.imageSize = 605;
        entry.bodySize = 5;
        entry.timestamp = 3000;

        const fs::path directory = MakeTempDirectory();

        std::vector<std::uint8_t> textureCache(
            TextureRebuilder::HeaderBlockSize * 3,
            0);

        for (std::uint32_t block = 0; block < 3; ++block)
        {
            const std::uint8_t value =
                static_cast<std::uint8_t>(0x10 + block);

            const std::size_t offset =
                static_cast<std::size_t>(block) *
                TextureRebuilder::HeaderBlockSize;

            std::fill(
                textureCache.begin() + offset,
                textureCache.begin() + offset +
                    TextureRebuilder::HeaderBlockSize,
                value);
        }

        WriteTextureCache(directory, textureCache);
        WriteBodyFile(
            directory,
            entry,
            {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xff, 0xff});

        TextureRebuilder rebuilder;
        std::vector<std::uint8_t> rebuilt;

        const RebuildError error =
            rebuilder.Rebuild(directory, entry, rebuilt);

        Expect(
            error == RebuildError::None,
            "rebuild entry with body");
        Expect(
            rebuilt.size() == 605,
            "rebuilt data uses header bytes plus exact bodySize");

        if (rebuilt.size() == 605)
        {
            const bool headerIsRawCacheIndexBlock =
                std::all_of(
                    rebuilt.begin(),
                    rebuilt.begin() +
                        TextureRebuilder::HeaderBlockSize,
                    [](std::uint8_t byte)
                    {
                        return byte == 0x12;
                    });

            Expect(
                headerIsRawCacheIndexBlock,
                "rebuild reads header from raw cacheIndex block");

            const std::vector<std::uint8_t> body(
                rebuilt.begin() +
                    TextureRebuilder::HeaderBlockSize,
                rebuilt.end());

            Expect(
                body == std::vector<std::uint8_t>(
                    {0xa0, 0xa1, 0xa2, 0xa3, 0xa4}),
                "rebuild copies exactly bodySize bytes");
        }

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove rebuild test directory");
    }

    void TestTextureRebuilderTrimsSmallHeaderOnlyTexture()
    {
        CacheEntry entry{};
        entry.uuid = MakeUuid("3aaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        entry.cacheIndex = 0;
        entry.imageSize = 4;
        entry.bodySize = 0;
        entry.timestamp = 4000;

        const fs::path directory = MakeTempDirectory();

        std::vector<std::uint8_t> textureCache(
            TextureRebuilder::HeaderBlockSize,
            0xee);

        textureCache[0] = 1;
        textureCache[1] = 2;
        textureCache[2] = 3;
        textureCache[3] = 4;

        WriteTextureCache(directory, textureCache);

        TextureRebuilder rebuilder;
        std::vector<std::uint8_t> rebuilt;

        const RebuildError error =
            rebuilder.Rebuild(directory, entry, rebuilt);

        Expect(
            error == RebuildError::None,
            "rebuild small header-only entry");
        Expect(
            rebuilt == std::vector<std::uint8_t>({1, 2, 3, 4}),
            "small header-only rebuild trims padded header block");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove header-only rebuild test directory");
    }

    void TestTextureRebuilderRejectsUndersizedBodyFile()
    {
        CacheEntry entry{};
        entry.uuid = MakeUuid("4aaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        entry.cacheIndex = 0;
        entry.imageSize = 603;
        entry.bodySize = 3;
        entry.timestamp = 5000;

        const fs::path directory = MakeTempDirectory();

        WriteTextureCache(
            directory,
            std::vector<std::uint8_t>(
                TextureRebuilder::HeaderBlockSize,
                0x55));
        WriteBodyFile(directory, entry, {0x01, 0x02});

        TextureRebuilder rebuilder;
        std::vector<std::uint8_t> rebuilt;

        const RebuildError error =
            rebuilder.Rebuild(directory, entry, rebuilt);

        Expect(
            error == RebuildError::BodyReadError,
            "undersized body file is an error");
        Expect(
            rebuilt.empty(),
            "failed rebuild clears output data");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove undersized-body test directory");
    }

    void TestTextureExportStatePersistsIncompleteEntries()
    {
        CacheEntry entry{};
        entry.uuid = MakeUuid("5aaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
        entry.cacheIndex = 7;
        entry.imageSize = 4096;
        entry.bodySize = 512;
        entry.timestamp = 6000;

        const fs::path directory = MakeTempDirectory();
        const fs::path stateFile = directory / "state" / "export.tsv";

        TextureExportState state;
        state.MarkIncomplete(
            entry,
            "missing\tpacket\ntry later\\soon");

        Expect(
            state.EntryCount() == 1,
            "incomplete export state stores one record");
        Expect(
            state.IsKnownIncomplete(entry),
            "marked entry is known incomplete");

        std::string errorMessage;
        Expect(
            state.Save(stateFile, errorMessage),
            "save export state");
        Expect(
            errorMessage.empty(),
            "save export state leaves no error message");

        TextureExportState loaded;
        Expect(
            loaded.Load(stateFile, errorMessage),
            "load export state");
        Expect(
            errorMessage.empty(),
            "load export state leaves no error message");
        Expect(
            loaded.EntryCount() == 1,
            "loaded export state has one record");
        Expect(
            loaded.IsKnownIncomplete(entry),
            "loaded state recognizes unchanged incomplete entry");

        CacheEntry changedMetadata = entry;
        changedMetadata.bodySize += 1;

        Expect(
            !loaded.IsKnownIncomplete(changedMetadata),
            "known incomplete entry is invalidated by metadata change");

        loaded.MarkSucceeded(entry);

        Expect(
            loaded.EntryCount() == 0,
            "successful export removes incomplete state record");
        Expect(
            !loaded.IsKnownIncomplete(entry),
            "succeeded entry is no longer known incomplete");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove export state test directory");
    }

    void TestTextureExportStateLoadHandlesMissingFile()
    {
        const fs::path directory = MakeTempDirectory();
        const fs::path stateFile = directory / "missing.tsv";

        TextureExportState state;
        std::string errorMessage;

        Expect(
            state.Load(stateFile, errorMessage),
            "missing export state file is not an error");
        Expect(
            errorMessage.empty(),
            "missing export state leaves no error message");
        Expect(
            state.EntryCount() == 0,
            "missing export state loads no records");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove missing-state test directory");
    }

    void TestAuroraSealedEnvelopeInspection()
    {
        const fs::path directory = MakeTempDirectory();
        const fs::path validFile = directory / "valid.j2c";

        WriteBytes(
            validFile,
            MakeAuroraSealedFixture(
                AuroraSealedChunkSize + 1));

        AuroraSealedFileInfo info;
        Expect(
            InspectAuroraSealedFile(validFile, info) ==
                AuroraFormatError::None,
            "inspect a valid Aurora sealed envelope");
        Expect(
            info.plaintextBytes == AuroraSealedChunkSize + 1,
            "sealed envelope preserves plaintext length");
        Expect(
            info.chunkCount == 2,
            "sealed envelope counts chunks across the boundary");
        Expect(
            info.sealedBytes ==
                AuroraSealedHeaderSize +
                    AuroraSealedChunkSize + 1 +
                    2 * AuroraSealedTagSize,
            "sealed envelope validates the stored size");

        const fs::path emptyFile = directory / "empty.j2c";
        WriteBytes(emptyFile, MakeAuroraSealedFixture(0));
        Expect(
            InspectAuroraSealedFile(emptyFile, info) ==
                AuroraFormatError::None &&
                info.chunkCount == 1,
            "empty sealed envelope retains one authentication tag");

        std::vector<std::uint8_t> future =
            MakeAuroraSealedFixture(8);
        future[3] = 'X';
        const fs::path futureFile = directory / "future.j2c";
        WriteBytes(futureFile, future);
        Expect(
            InspectAuroraSealedFile(futureFile, info) ==
                AuroraFormatError::UnsupportedVersion,
            "unknown AUS sealed format fails as unsupported");

        std::vector<std::uint8_t> truncated =
            MakeAuroraSealedFixture(8);
        truncated.pop_back();
        const fs::path truncatedFile = directory / "truncated.j2c";
        WriteBytes(truncatedFile, truncated);
        Expect(
            InspectAuroraSealedFile(truncatedFile, info) ==
                AuroraFormatError::SizeMismatch,
            "replaced or truncated sealed file reports a size mismatch");

        std::vector<std::uint8_t> unrelated =
            MakeAuroraSealedFixture(8);
        unrelated[0] = 'X';
        const fs::path unrelatedFile = directory / "unrelated.j2c";
        WriteBytes(unrelatedFile, unrelated);
        Expect(
            InspectAuroraSealedFile(unrelatedFile, info) ==
                AuroraFormatError::InvalidFormat,
            "unrelated data is not an Aurora sealed envelope");

        Expect(
            InspectAuroraSealedFile(
                directory / "removed.j2c",
                info) == AuroraFormatError::FileNotFound,
            "a removed Aurora file is reported as a normal missing file");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove Aurora sealed-envelope test directory");
    }

    void TestAuroraKeyedTextureNames()
    {
        Expect(
            IsAuroraTextureKey(
                "0123456789abcdef0123456789abcdef"),
            "Aurora keyed texture name accepts 32 lowercase hex digits");
        Expect(
            !IsAuroraTextureKey(
                "0123456789ABCDEF0123456789ABCDEF"),
            "Aurora keyed texture name rejects uppercase hex digits");
        Expect(
            !IsAuroraTextureKey(
                "0123456789abcdef0123456789abcdeg"),
            "Aurora keyed texture name rejects non-hex digits");
        Expect(
            !IsAuroraTextureKey("0123456789abcdef"),
            "Aurora keyed texture name rejects the wrong length");
    }

    void TestAuroraKnownAnswerCrypto()
    {
        const std::array<std::uint8_t, AuroraCacheKey::Size> keyBytes =
            KnownAuroraKeyBytes();

        AuroraCacheKey key = AuroraCacheKey::FromBytes(keyBytes);
        const UUID uuid =
            MakeUuid("12345678-9abc-def0-0fed-cba987654321");
        const std::optional<std::string> keyedName =
            AuroraKeyedTextureName(key, uuid);

        Expect(
            keyedName.has_value() &&
                *keyedName == "1136455863d312914e3ab2909f07a316",
            "Aurora UUID name matches the independent HKDF/HMAC vector");

        const std::vector<std::uint8_t> sealed =
            KnownAuroraSealedFixture();
        const std::vector<std::uint8_t> expectedPlaintext =
            KnownAuroraPlaintext();

        std::vector<std::uint8_t> plaintext;
        Expect(
            OpenAuroraSealedBytes(sealed, key, plaintext) ==
                AuroraFormatError::None,
            "open the independent Aurora AES-GCM fixture");
        Expect(
            plaintext == expectedPlaintext,
            "Aurora sealed fixture decrypts to the expected plaintext");

        std::array<std::uint8_t, AuroraCacheKey::Size> wrongBytes{};
        wrongBytes.fill(0x5a);
        AuroraCacheKey wrongKey =
            AuroraCacheKey::FromBytes(wrongBytes);
        Expect(
            OpenAuroraSealedBytes(
                sealed,
                wrongKey,
                plaintext) == AuroraFormatError::AuthenticationFailed,
            "Aurora sealed fixture rejects another installation key");
        Expect(
            plaintext.empty(),
            "failed Aurora authentication leaves no plaintext");

        std::vector<std::uint8_t> changed = sealed;
        changed.back() ^= 1;
        Expect(
            OpenAuroraSealedBytes(
                changed,
                key,
                plaintext) == AuroraFormatError::AuthenticationFailed,
            "Aurora sealed fixture rejects a changed authentication tag");

        const fs::path directory = MakeTempDirectory();
        const fs::path filename = directory / "known-answer.j2c";
        WriteBytes(filename, sealed);
        Expect(
            OpenAuroraSealedFile(filename, key, plaintext) ==
                AuroraFormatError::None &&
                plaintext == expectedPlaintext,
            "open a sealed Aurora file from disk");
        Expect(
            OpenAuroraSealedFile(
                directory / "removed.j2c",
                key,
                plaintext) == AuroraFormatError::FileNotFound,
            "opening a removed sealed file is a cache miss");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove Aurora crypto test directory");
    }

    void TestAuroraTextureCacheSourceProvidesNeutralPayloads()
    {
        const fs::path directory = MakeTempDirectory();
        const fs::path cacheRoot = directory / "cache";
        const fs::path textureStore =
            cacheRoot / "store" / "tex";

        std::error_code createError;
        fs::create_directories(textureStore, createError);
        Expect(!createError, "create Aurora source test store");

        const std::string jpegKey =
            "1136455863d312914e3ab2909f07a316";
        const std::string bc7Key(32, 'b');
        const std::vector<std::uint8_t> sealed =
            KnownAuroraSealedFixture();
        const std::vector<std::uint8_t> plaintext =
            KnownAuroraPlaintext();

        WriteBytes(
            textureStore / (jpegKey + ".j2c"),
            sealed);
        WriteBytes(
            textureStore / (bc7Key + ".bc7"),
            sealed);

        const auto keyLoader =
            [keyBytes = KnownAuroraKeyBytes()](AuroraCacheKey& output)
            {
                output = AuroraCacheKey::FromBytes(keyBytes);
                return AuroraCredentialError::None;
            };

        AuroraTextureCacheSource source(keyLoader);
        Expect(
            source.Open(cacheRoot) == TextureCacheOpenError::None,
            "open a synthetic Aurora texture source");
        Expect(source.IsOpen(), "Aurora texture source is open");
        Expect(
            source.Format() == TextureCacheFormat::Aurora,
            "Aurora texture source reports its format");
        Expect(
            source.Entries().size() == 2,
            "Aurora texture source exposes keyed catalog entries");

        const UUID knownUuid =
            MakeUuid("12345678-9abc-def0-0fed-cba987654321");
        const TextureCatalogEntry* found =
            source.FindByUuid(knownUuid);
        Expect(
            found != nullptr && found->sourceKey == jpegKey,
            "Aurora Find UUID derives the keyed filename");
        Expect(
            found != nullptr && !found->uuid.has_value(),
            "Aurora catalog does not claim an enumerable UUID");
        Expect(
            found != nullptr &&
                found->HasCompletePayload() &&
                found->timestamp > 0,
            "complete Aurora J2C entry has size and timestamp metadata");

        TexturePayload payload;
        Expect(
            source.ReadPayload(jpegKey, payload) ==
                TexturePayloadError::None,
            "read a decrypted Aurora JPEG2000 payload");
        Expect(
            payload.format == TexturePayloadFormat::Jpeg2000 &&
                payload.bytes == plaintext,
            "Aurora J2C payload preserves plaintext bytes");

        Expect(
            source.ReadPayload(bc7Key, payload) ==
                TexturePayloadError::None,
            "read a decrypted Aurora BC7 container payload");
        Expect(
            payload.format == TexturePayloadFormat::Bc7 &&
                payload.bytes == plaintext,
            "Aurora BC7 payload is explicitly typed");

        ViewerTextureCacheSource detectedSource(keyLoader);
        Expect(
            detectedSource.Open(cacheRoot) ==
                    TextureCacheOpenError::None &&
                detectedSource.Format() == TextureCacheFormat::Aurora &&
                detectedSource.AuroraSource() != nullptr,
            "auto-detecting source selects the Aurora adapter");
        Expect(
            detectedSource.FindByUuid(knownUuid) != nullptr,
            "auto-detecting source delegates Aurora UUID lookup");
        detectedSource.Close();

        std::unique_ptr<TexturePayloadRequest> detached =
            source.CreatePayloadRequest(jpegKey);
        Expect(detached != nullptr, "create detached Aurora payload request");
        source.Close();
        Expect(
            detached &&
                detached->Read(payload) == TexturePayloadError::None &&
                payload.bytes == plaintext,
            "detached Aurora request survives source close");

        AuroraTextureCacheSource noCredential(
            [](AuroraCacheKey& output)
            {
                output = {};
                return AuroraCredentialError::NotFound;
            });
        Expect(
            noCredential.Open(cacheRoot) ==
                TextureCacheOpenError::CredentialsUnavailable &&
                noCredential.CredentialError() ==
                    AuroraCredentialError::NotFound,
            "Aurora source reports an unavailable credential clearly");

        std::array<std::uint8_t, AuroraCacheKey::Size> wrongBytes{};
        wrongBytes.fill(0x5a);
        AuroraTextureCacheSource wrongKey(
            [wrongBytes](AuroraCacheKey& output)
            {
                output = AuroraCacheKey::FromBytes(wrongBytes);
                return AuroraCredentialError::None;
            });
        Expect(
            wrongKey.Open(cacheRoot) == TextureCacheOpenError::None,
            "Aurora source can enumerate before authenticating payloads");
        Expect(
            wrongKey.ReadPayload(jpegKey, payload) ==
                TexturePayloadError::MissingData,
            "a cache file sealed by another installation is a cache miss");

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove Aurora source test directory");
    }

    void TestAuroraTranscodedTextureParsing()
    {
        const std::vector<std::uint8_t> valid =
            MakeAuroraTranscodedFixture();

        AuroraTranscodedTextureInfo info;
        Expect(
            ParseAuroraTranscodedTexture(valid, info) ==
                AuroraFormatError::None,
            "parse a valid Aurora AUTX fixture");
        Expect(
            info.version == 6 &&
                info.kind == AuroraTranscodedKind::Bc7,
            "AUTX parser preserves version and BC7 kind");
        Expect(
            info.imageWidth == 64 &&
                info.imageHeight == 32 &&
                info.finestDiscard == 2,
            "AUTX parser preserves image metadata");
        Expect(
            info.levels.size() == 5,
            "AUTX parser preserves mip count");

        if (info.levels.size() == 5)
        {
            Expect(
                info.levels.front().width == 16 &&
                    info.levels.front().height == 8 &&
                    info.levels.front().offset == 72 &&
                    info.levels.front().byteCount == 128,
                "AUTX parser calculates first BC7 level layout");
            Expect(
                info.levels.back().width == 1 &&
                    info.levels.back().height == 1 &&
                    info.levels.back().byteCount == 16,
                "AUTX parser calculates final BC7 level layout");
        }

        std::vector<std::uint8_t> future = valid;
        future[4] = 7;
        Expect(
            ParseAuroraTranscodedTexture(future, info) ==
                AuroraFormatError::UnsupportedVersion,
            "unknown AUTX version fails as unsupported");

        std::vector<std::uint8_t> old = valid;
        old[4] = 2;
        Expect(
            ParseAuroraTranscodedTexture(old, info) ==
                AuroraFormatError::UnsupportedVersion,
            "obsolete AUTX version fails as unsupported");

        std::vector<std::uint8_t> wrongChain = valid;
        SetLittleEndian32(wrongChain, 40, 7);
        Expect(
            ParseAuroraTranscodedTexture(wrongChain, info) ==
                AuroraFormatError::InvalidMetadata,
            "AUTX parser rejects a malformed mip chain");

        std::vector<std::uint8_t> oddTopLevel = valid;
        SetLittleEndian32(oddTopLevel, 32, 15);
        Expect(
            ParseAuroraTranscodedTexture(oddTopLevel, info) ==
                AuroraFormatError::InvalidMetadata,
            "AUTX parser rejects a non-block-aligned BC7 top level");

        std::vector<std::uint8_t> truncated = valid;
        truncated.pop_back();
        Expect(
            ParseAuroraTranscodedTexture(truncated, info) ==
                AuroraFormatError::SizeMismatch,
            "AUTX parser rejects a truncated BC7 payload");

        std::vector<std::uint8_t> keepRgba(
            valid.begin(),
            valid.begin() + 32);
        keepRgba[5] = 0;
        keepRgba[11] = 0;
        Expect(
            ParseAuroraTranscodedTexture(keepRgba, info) ==
                AuroraFormatError::None &&
                info.kind == AuroraTranscodedKind::KeepRgba &&
                info.levels.empty(),
            "AUTX parser recognizes the keep-RGBA marker");
    }

    void TestAuroraTextureStoreProbingAndEnumeration()
    {
        const fs::path directory = MakeTempDirectory();
        const fs::path cacheRoot = directory / "cache";
        const fs::path textureStore =
            cacheRoot / "store" / "tex";

        std::error_code createError;
        fs::create_directories(textureStore, createError);
        Expect(!createError, "create synthetic Aurora texture store");

        AuroraTextureStoreProbe probe;
        const TextureCacheOpenError rootProbeError =
            ProbeAuroraTextureStore(cacheRoot, probe);
        std::error_code equivalentError;
        const bool equivalentStore =
            fs::equivalent(
                probe.textureStore,
                textureStore,
                equivalentError);
        Expect(
            rootProbeError == TextureCacheOpenError::None &&
                !equivalentError &&
                equivalentStore,
            "probe Aurora cache root");
        Expect(
            ProbeAuroraTextureStore(
                cacheRoot / "store",
                probe) == TextureCacheOpenError::None,
            "probe Aurora store directory");
        Expect(
            ProbeAuroraTextureStore(textureStore, probe) ==
                TextureCacheOpenError::None,
            "probe direct Aurora texture store");

        const fs::path customParent = directory / "custom";
        const fs::path customStore =
            customParent /
            "AuroraViewerCache" /
            "store" /
            "tex";
        fs::create_directories(customStore, createError);
        Expect(!createError, "create custom Aurora texture store");
        Expect(
            ProbeAuroraTextureStore(customParent, probe) ==
                TextureCacheOpenError::None &&
                probe.cacheRoot.filename() == "AuroraViewerCache",
            "probe user-selected Aurora cache parent");

        const fs::path legacyOnly = directory / "legacy";
        fs::create_directories(legacyOnly, createError);
        Expect(!createError, "create unrelated legacy cache directory");
        WriteBytes(legacyOnly / "texture.entries", {0, 1, 2, 3});
        Expect(
            ProbeAuroraTextureStore(legacyOnly, probe) ==
                TextureCacheOpenError::InvalidFormat,
            "Aurora probing does not weaken legacy path validation");
        Expect(
            ProbeAuroraTextureStore(
                directory / "missing",
                probe) == TextureCacheOpenError::FileNotFound,
            "missing Aurora cache root reports file not found");

        const std::string firstKey(32, 'a');
        const std::string secondKey(32, 'b');
        const std::string futureKey(32, 'c');
        const std::string replacedKey(32, 'd');

        WriteBytes(
            textureStore / (firstKey + ".j2c"),
            MakeAuroraSealedFixture(100));
        WriteBytes(
            textureStore / (firstKey + ".part"),
            MakeAuroraSealedFixture(50));
        WriteBytes(
            textureStore / (secondKey + ".bc7"),
            MakeAuroraSealedFixture(
                AuroraSealedChunkSize + 1));

        std::vector<std::uint8_t> future =
            MakeAuroraSealedFixture(10);
        future[3] = 'X';
        WriteBytes(
            textureStore / (futureKey + ".j2c"),
            future);

        std::vector<std::uint8_t> replaced =
            MakeAuroraSealedFixture(10);
        replaced.pop_back();
        WriteBytes(
            textureStore / (replacedKey + ".part"),
            replaced);

        WriteBytes(
            textureStore /
                "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeg.j2c",
            MakeAuroraSealedFixture(10));
        WriteBytes(
            textureStore / "notes.txt",
            {1, 2, 3});

        AuroraTextureStoreScan scan;
        Expect(
            EnumerateAuroraTextureStore(cacheRoot, scan) ==
                TextureCacheOpenError::None,
            "enumerate Aurora keyed texture files");
        Expect(
            scan.records.size() == 4,
            "enumeration groups candidate files by opaque source key");
        Expect(
            scan.cacheMissCount == 0,
            "lazy enumeration avoids opening candidate payload files");
        Expect(
            scan.ignoredFileCount == 2,
            "enumeration ignores unrelated and invalidly named files");

        if (scan.records.size() == 4)
        {
            const AuroraTextureRecord& first = scan.records[0];
            const AuroraTextureRecord& second = scan.records[1];

            Expect(
                first.sourceKey == firstKey &&
                    first.completeJpeg2000.has_value() &&
                    first.partialJpeg2000.has_value() &&
                    !first.transcodedBc7.has_value(),
                "enumeration groups complete and partial J2C files");
            Expect(
                first.completeJpeg2000->plaintextBytes == 100 &&
                    first.partialJpeg2000->plaintextBytes == 50,
                "enumeration derives J2C plaintext sizes without opening files");
            Expect(
                second.sourceKey == secondKey &&
                    second.transcodedBc7.has_value(),
                "enumeration records a sealed BC7 file");
            Expect(
                second.transcodedBc7->plaintextBytes ==
                    AuroraSealedChunkSize + 1,
                "enumeration derives multi-chunk plaintext size from sealed length");
        }

        std::error_code removeError;
        fs::remove_all(directory, removeError);
        Expect(!removeError, "remove Aurora probing test directory");
    }
}

int main()
{
    TestDatabaseFiltersEntriesAndPreservesCacheIndex();
    TestCachedTextureCompletenessHelpers();
    TestTextureCacheChangesFindsAddedAndUpdatedEntries();
    TestTextureSelectionUsesFilteredDatabaseOrder();
    TestLegacyTextureCacheSourceProvidesNeutralCatalogAndPayload();
    TestTextureRebuilderUsesRawCacheIndexAndExactBodySize();
    TestTextureRebuilderTrimsSmallHeaderOnlyTexture();
    TestTextureRebuilderRejectsUndersizedBodyFile();
    TestTextureExportStatePersistsIncompleteEntries();
    TestTextureExportStateLoadHandlesMissingFile();
    TestAuroraSealedEnvelopeInspection();
    TestAuroraKeyedTextureNames();
    TestAuroraKnownAnswerCrypto();
    TestAuroraTextureCacheSourceProvidesNeutralPayloads();
    TestAuroraTranscodedTextureParsing();
    TestAuroraTextureStoreProbingAndEnumeration();

    if (gFailures != 0)
    {
        std::cerr
            << gFailures
            << " cachelib test failure(s)\n";
        return 1;
    }

    std::cout << "cachelib tests passed\n";
    return 0;
}
