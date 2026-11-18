#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class AuroraCacheKey
{
public:
    static constexpr std::size_t Size = 32;

    AuroraCacheKey() = default;
    ~AuroraCacheKey();

    AuroraCacheKey(const AuroraCacheKey&) = delete;
    AuroraCacheKey& operator=(const AuroraCacheKey&) = delete;

    AuroraCacheKey(AuroraCacheKey&& other) noexcept;
    AuroraCacheKey& operator=(AuroraCacheKey&& other) noexcept;

    static AuroraCacheKey FromBytes(
        const std::array<std::uint8_t, Size>& bytes);

    bool IsValid() const
    {
        return valid_;
    }

    const std::array<std::uint8_t, Size>& Bytes() const
    {
        return bytes_;
    }

private:
    void Clear() noexcept;

    std::array<std::uint8_t, Size> bytes_{};
    bool valid_ = false;
};
