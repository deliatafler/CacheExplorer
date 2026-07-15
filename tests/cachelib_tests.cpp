#include "Structures.h"
#include "TextureCacheDatabase.h"
#include "TextureSelection.h"
#include "UUID.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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
}

int main()
{
    TestDatabaseFiltersEntriesAndPreservesCacheIndex();
    TestTextureSelectionUsesFilteredDatabaseOrder();

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
