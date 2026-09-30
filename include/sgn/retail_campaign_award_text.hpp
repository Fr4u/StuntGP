#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace sgn::retail
{
// Bounded46280C sprintf call shape used at44C660/44C68E: one signed%d in
// a borrowed frontend-language format. Raw source bytes, no encoding conversion.
// The14 authored points/cash formats have this shape. This is not a general
// printf replacement or a language loader. Retain all destination bytes afterNUL.
// Host format/capacity guards precede any mutation of the actual260-byte owner.
std::uint32_t format_campaign_award_46280c(std::span<std::byte, 260> destination, std::string_view format,
                                           std::uint32_t value);
} // namespace sgn::retail
