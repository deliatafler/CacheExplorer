#include "UUID.h"

#include <cctype>
#include <iomanip>
#include <istream>
#include <sstream>
#include <stdexcept>

namespace
{
    int HexValue(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }

        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }

        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }

        return -1;
    }
}

UUID UUID::Read(std::istream& stream)
{
    UUID uuid;

    stream.read(
        reinterpret_cast<char*>(uuid.mBytes.data()),
        static_cast<std::streamsize>(SIZE));

    if (!stream)
    {
        throw std::runtime_error("Failed to read UUID.");
    }

    return uuid;
}

std::optional<UUID> UUID::FromString(const std::string& value)
{
    std::string hexDigits;
    hexDigits.reserve(32);

    for (char character : value)
    {
        if (character == '-')
        {
            continue;
        }

        if (!std::isxdigit(
                static_cast<unsigned char>(character)))
        {
            return std::nullopt;
        }

        hexDigits.push_back(character);
    }

    if (hexDigits.size() != 32)
    {
        return std::nullopt;
    }

    UUID uuid;

    for (std::size_t i = 0; i < SIZE; ++i)
    {
        const int high = HexValue(hexDigits[i * 2]);
        const int low = HexValue(hexDigits[i * 2 + 1]);

        if (high < 0 || low < 0)
        {
            return std::nullopt;
        }

        uuid.mBytes[i] = static_cast<std::uint8_t>(
            (high << 4) | low);
    }

    return uuid;
}

std::string UUID::ToString() const
{
    std::ostringstream stream;

    stream << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < SIZE; ++i)
    {
        stream
            << std::setw(2)
            << static_cast<unsigned>(mBytes[i]);

        if (i == 3 || i == 5 || i == 7 || i == 9)
        {
            stream << '-';
        }
    }

    return stream.str();
}

bool UUID::IsNull() const
{
    for (std::uint8_t byte : mBytes)
    {
        if (byte != 0)
        {
            return false;
        }
    }

    return true;
}

bool UUID::operator==(const UUID& other) const
{
    return mBytes == other.mBytes;
}

bool UUID::operator!=(const UUID& other) const
{
    return !(*this == other);
}
