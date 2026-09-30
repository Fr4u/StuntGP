#pragma once
// Shared bit reinterpretation helpers (code organisation only, no new behaviour).
// Each function is exactly one std::bit_cast between two 32-bit types; the name says the
// direction, the parameter type says the source:
//     as_float(W)            word bits -> float        (was real, fl, scalar, literal, value, ...)
//     as_word(float)         float bits -> word        (was bits(float), word(float), u, ...)
//     as_signed(W)           word bits -> int32        (was signed_word, sw, si, s32, sign, ...)
//     as_unsigned(int32)     int32 bits -> word        (was bits(int32), word(int32), u32, ...)
// The four names are deliberately not overloads of each other, so a call can never pick a
// different conversion than the per-file helper it replaces.
//
// Two variants with the same names and results:
//   sgn::bit_reinterpret::               plain inline functions (the per-file copies were plain
//                                         functions);
//   sgn::bit_reinterpret::compile_time:: constexpr, usable in constant expressions (the per-file
//                                         copies were constexpr).
// They are kept apart on purpose: MSVC constant-folds a constexpr call with a constant argument
// already in the front end (e.g. double(as_float(0x3E2AAAAB)) becomes one double literal), so
// swapping one variant for the other changes the emitted instructions although every value is
// the same. Adopt the variant that matches the local helper being replaced.
// A file adopts them with `using sgn::bit_reinterpret::as_float;` (etc.) at the place of its
// former local helper.
#include <bit>
#include <cstdint>

namespace sgn::bit_reinterpret
{
[[nodiscard]] inline float as_float(std::uint32_t word)
{
    return std::bit_cast<float>(word);
}
[[nodiscard]] inline std::uint32_t as_word(float value)
{
    return std::bit_cast<std::uint32_t>(value);
}
[[nodiscard]] inline std::int32_t as_signed(std::uint32_t word)
{
    return std::bit_cast<std::int32_t>(word);
}
[[nodiscard]] inline std::uint32_t as_unsigned(std::int32_t value)
{
    return std::bit_cast<std::uint32_t>(value);
}

namespace compile_time
{
[[nodiscard]] constexpr float as_float(std::uint32_t word)
{
    return std::bit_cast<float>(word);
}
[[nodiscard]] constexpr std::uint32_t as_word(float value)
{
    return std::bit_cast<std::uint32_t>(value);
}
[[nodiscard]] constexpr std::int32_t as_signed(std::uint32_t word)
{
    return std::bit_cast<std::int32_t>(word);
}
[[nodiscard]] constexpr std::uint32_t as_unsigned(std::int32_t value)
{
    return std::bit_cast<std::uint32_t>(value);
}
} // namespace compile_time
} // namespace sgn::bit_reinterpret
