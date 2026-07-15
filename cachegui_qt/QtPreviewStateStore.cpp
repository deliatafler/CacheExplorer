#include "QtPreviewStateStore.h"

#include "PreviewCache.h"
#include "QtHelpers.h"
#include "TextureCacheDatabase.h"
#include "UUID.h"

#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

namespace
{
    constexpr const char* Header =
        "# CacheExplorer Qt preview state v1";

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

    std::string EscapeField(const QString& value)
    {
        std::string escaped;
        const std::string text = value.toUtf8().toStdString();
        escaped.reserve(text.size());

        for (const char character : text)
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

    QString UnescapeField(const std::string& value)
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

        return QString::fromUtf8(
            unescaped.data(),
            static_cast<int>(unescaped.size()));
    }

    template <typename Value>
    bool ParseInteger(const std::string& text, Value& value)
    {
        Value parsed{};
        const char* begin = text.data();
        const char* end = begin + text.size();
        const auto result = std::from_chars(begin, end, parsed);

        if (result.ec != std::errc{} || result.ptr != end)
        {
            return false;
        }

        value = parsed;
        return true;
    }

    const CacheEntry* FindByCacheIndex(
        const TextureCacheDatabase& database,
        std::uint32_t cacheIndex)
    {
        for (const CacheEntry& entry : database.Entries())
        {
            if (entry.cacheIndex == cacheIndex)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    const char* StateName(PreviewState state)
    {
        switch (state)
        {
            case PreviewState::Unavailable:
                return "unavailable";

            case PreviewState::LoadFailed:
                return "load-failed";

            case PreviewState::Unknown:
            case PreviewState::Checking:
            case PreviewState::Previewable:
                break;
        }

        return "";
    }

    bool ParseState(const std::string& value, PreviewState& state)
    {
        if (value == "unavailable")
        {
            state = PreviewState::Unavailable;
            return true;
        }

        if (value == "load-failed")
        {
            state = PreviewState::LoadFailed;
            return true;
        }

        return false;
    }
}

std::filesystem::path PreviewStateFilePath(
    const std::filesystem::path& cacheDirectory)
{
    QString baseDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);

    if (baseDirectory.isEmpty())
    {
        baseDirectory =
            QStandardPaths::writableLocation(
                QStandardPaths::CacheLocation);
    }

    if (baseDirectory.isEmpty())
    {
        baseDirectory =
            QStandardPaths::writableLocation(
                QStandardPaths::TempLocation);
    }

    QDir directory(baseDirectory);
    directory.mkpath(QStringLiteral("preview-state"));

    const QByteArray cacheKey =
        QCryptographicHash::hash(
            PathToQString(cacheDirectory).toUtf8(),
            QCryptographicHash::Sha256).toHex();

    return PathFromQString(
        directory.filePath(
            QStringLiteral("preview-state/%1.tsv")
                .arg(QString::fromLatin1(cacheKey))));
}

bool LoadPreviewState(
    const std::filesystem::path& stateFile,
    const TextureCacheDatabase& database,
    PreviewCache& previewCache,
    QString& errorMessage)
{
    errorMessage.clear();

    std::ifstream input(stateFile);

    if (!input)
    {
        std::error_code existsError;

        if (std::filesystem::exists(stateFile, existsError) &&
            !existsError)
        {
            errorMessage =
                QStringLiteral("Could not read preview state file: ")
                + PathToQString(stateFile);
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
            errorMessage =
                QStringLiteral("Invalid preview state line %1.")
                    .arg(static_cast<qulonglong>(lineNumber));
            return false;
        }

        const std::optional<UUID> uuid =
            UUID::FromString(fields[0]);

        std::uint32_t cacheIndex = 0;
        std::int32_t imageSize = 0;
        std::int32_t bodySize = 0;
        PreviewState state = PreviewState::Unknown;

        if (!uuid ||
            !ParseInteger(fields[1], cacheIndex) ||
            !ParseInteger(fields[2], imageSize) ||
            !ParseInteger(fields[3], bodySize) ||
            !ParseState(fields[4], state))
        {
            errorMessage =
                QStringLiteral("Invalid preview state values on line %1.")
                    .arg(static_cast<qulonglong>(lineNumber));
            return false;
        }

        const CacheEntry* entry = database.Find(*uuid);

        if (entry == nullptr ||
            entry->cacheIndex != cacheIndex ||
            entry->imageSize != imageSize ||
            entry->bodySize != bodySize)
        {
            continue;
        }

        const QString message = UnescapeField(fields[5]);

        if (state == PreviewState::Unavailable)
        {
            previewCache.SetUnavailable(*entry, message);
        }
        else if (state == PreviewState::LoadFailed)
        {
            previewCache.SetLoadFailed(*entry, message);
        }
    }

    return true;
}

bool SavePreviewState(
    const std::filesystem::path& stateFile,
    const TextureCacheDatabase& database,
    const PreviewCache& previewCache,
    QString& errorMessage)
{
    errorMessage.clear();

    const std::filesystem::path parentDirectory =
        stateFile.parent_path();

    if (!parentDirectory.empty())
    {
        std::error_code directoryError;
        std::filesystem::create_directories(
            parentDirectory,
            directoryError);

        if (directoryError)
        {
            errorMessage =
                QStringLiteral("Could not create preview state directory: ")
                + PathToQString(parentDirectory);
            return false;
        }
    }

    std::ofstream output(stateFile);

    if (!output)
    {
        errorMessage =
            QStringLiteral("Could not write preview state file: ")
            + PathToQString(stateFile);
        return false;
    }

    output
        << Header
        << "\n"
        << "# uuid\tcacheIndex\timageSize\tbodySize\tstate\tmessage\n";

    for (const StoredPreviewRecord& record :
         previewCache.TerminalRecords())
    {
        const CacheEntry* entry =
            FindByCacheIndex(database, record.cacheIndex);

        if (entry == nullptr)
        {
            continue;
        }

        output
            << entry->uuid.ToString()
            << "\t"
            << entry->cacheIndex
            << "\t"
            << entry->imageSize
            << "\t"
            << entry->bodySize
            << "\t"
            << StateName(record.state)
            << "\t"
            << EscapeField(record.message)
            << "\n";
    }

    if (!output)
    {
        errorMessage =
            QStringLiteral("Could not finish writing preview state file: ")
            + PathToQString(stateFile);
        return false;
    }

    return true;
}
