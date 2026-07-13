#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>

class UUID
{
public:
    static constexpr std::size_t SIZE = 16;

    UUID() = default;

    static UUID Read(std::istream& stream);

    static std::optional<UUID> FromString(const std::string& value);

    std::string ToString() const;

    bool IsNull() const;

    const std::array<std::uint8_t, SIZE>& Bytes() const
    {
        return mBytes;
    }

    bool operator==(const UUID& other) const;
    bool operator!=(const UUID& other) const;

private:
    std::array<std::uint8_t, SIZE> mBytes{};
};

static_assert(sizeof(UUID) == 16, "UUID must be exactly 16 bytes.");
