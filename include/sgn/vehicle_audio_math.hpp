#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace sgn::retail::audio_detail
{
inline std::uint32_t integer(double value)
{ // 462860, low32 of signed64 truncation
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw std::domain_error("audio conversion outside recovered finite int64 domain");
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
inline std::int32_t signed_word(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}
} // namespace sgn::retail::audio_detail
