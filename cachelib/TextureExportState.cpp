#include "TextureExportState.h"

#include "UUID.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr const char* Header =
        "# CacheExplorer export state v1";

    std::vector<std::string> SplitTabs(const std::string& line)
    {
        std::vector<std::string> fields;
        std::size_t start = 0;

        while (start <= line.size())
        {
            const std::size_t tab = line.find('\t', start);

            if (tab == std::string::npos)
            {
                fields.push_back(line.substr(start));
                break;
            }

            fields.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }

        return fields;
    }

    std::string EscapeField(const std::string& value)
    {
        std::string escaped;
        escaped.reserve(value.size());

        for (const char character : value)
        {
            switch (character)
            {
                case '\\':
                    escaped += "\\\\";
                    break;

                case '\t':
                    escaped += "\\t";
                    break;

                case '\r':
                    escaped += "\\r";
                    break;

                case '\n':
                    escaped += "\\n";
                    break;

                default:
                    escaped += character;
                    break;
            }
        }

        return escaped;
    }

    std::string UnescapeField(const std::string& value)
    {
        std::string unescaped;
        unescaped.reserve(value.size());

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] != '\\' || index + 1 >= value.size())
            {
                unescaped += value[index];
                continue;
            }

            ++index;

            switch (value[index])
            {
                case '\\':
                    unescaped += '\\';
                    break;

                case 't':
                    unescaped += '\t';
                    break;

                case 'r':
                    unescaped += '\r';
                    break;

                case 'n':
                    unescaped += '\n';
                    break;

                default:
                    unescaped += value[index];
                    break;
            }
        }

        return unescaped;
    }

    template <typename Value>
    bool ParseInteger(const std::string& text, Value& value)
    {
        std::istringstream stream(text);
        Value parsed{};

        stream >> parsed;

        if (!stream || !stream.eof())
        {
            return false;
        }

        value = parsed;
        return true;
    }
}

bool TextureExportState::Load(
    const fs::path& stateFile,
    std::string& errorMessage)
{
    errorMessage.clear();
    mRecords.clear();

    std::ifstream input(stateFile);

    if (!input)
    {
        std::error_code existsError;

        if (fs::exists(stateFile, existsError) && !existsError)
        {
            errorMessage =
                "Could not read export state file: "
                + stateFile.string();
            return false;
        }

        return true;
    }

    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line))
    {
        ++lineNumber;

        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        const std::vector<std::string> fields = SplitTabs(line);

        if (fields.size() < 6)
        {
            std::ostringstream message;
            message
                << "Invalid export state line "
                << lineNumber
                << ".";
            errorMessage = message.str();
            return false;
        }

        const std::optional<UUID> uuid =
            UUID::FromString(fields[0]);

        Record record;

        if (!uuid ||
            !ParseInteger(fields[1], record.cacheIndex) ||
            !ParseInteger(fields[2], record.imageSize) ||
            !ParseInteger(fields[3], record.bodySize))
        {
            std::ostringstream message;
            message
                << "Invalid export state values on line "
                << lineNumber
                << ".";
            errorMessage = message.str();
            return false;
        }

        record.status = fields[4];
        record.message = UnescapeField(fields[5]);
        mRecords[uuid->ToString()] = record;
    }

    return true;
}

bool TextureExportState::Save(
    const fs::path& stateFile,
    std::string& errorMessage) const
{
    errorMessage.clear();

    const fs::path parentDirectory = stateFile.parent_path();

    if (!parentDirectory.empty())
    {
        std::error_code directoryError;
        fs::create_directories(parentDirectory, directoryError);

        if (directoryError)
        {
            errorMessage =
                "Could not create export state directory: "
                + parentDirectory.string();
            return false;
        }
    }

    std::ofstream output(stateFile);

    if (!output)
    {
        errorMessage =
            "Could not write export state file: "
            + stateFile.string();
        return false;
    }

    output
        << Header
        << "\n"
        << "# uuid\tcacheIndex\timageSize\tbodySize\tstatus\tmessage\n";

    for (const auto& item : mRecords)
    {
        output
            << item.first
            << "\t"
            << item.second.cacheIndex
            << "\t"
            << item.second.imageSize
            << "\t"
            << item.second.bodySize
            << "\t"
            << item.second.status
            << "\t"
            << EscapeField(item.second.message)
            << "\n";
    }

    if (!output)
    {
        errorMessage =
            "Could not finish writing export state file: "
            + stateFile.string();
        return false;
    }

    return true;
}

bool TextureExportState::IsKnownIncomplete(
    const CacheEntry& entry) const
{
    const auto record = mRecords.find(KeyFor(entry));

    if (record == mRecords.end())
    {
        return false;
    }

    return record->second.status == "incomplete" &&
        record->second.cacheIndex == entry.cacheIndex &&
        record->second.imageSize == entry.imageSize &&
        record->second.bodySize == entry.bodySize;
}

void TextureExportState::MarkIncomplete(
    const CacheEntry& entry,
    const std::string& message)
{
    Record record;
    record.cacheIndex = entry.cacheIndex;
    record.imageSize = entry.imageSize;
    record.bodySize = entry.bodySize;
    record.status = "incomplete";
    record.message = message;

    mRecords[KeyFor(entry)] = record;
}

void TextureExportState::MarkSucceeded(const CacheEntry& entry)
{
    mRecords.erase(KeyFor(entry));
}

std::string TextureExportState::KeyFor(const CacheEntry& entry)
{
    return entry.uuid.ToString();
}
