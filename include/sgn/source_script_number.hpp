#pragma once
#include "sgn/source_boot_config.hpp"
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Full42C6A0 with the actual output word and optional source cursor slot.
// Output is written at42C6C8 BEFORE the literal-space scan/cursor publication.
// A scan failure retains output and leaves the old cursor unchanged.
void parse_source_number_42c6a0(RaceSessionPlatform &, SourceBootSearchBoundary input,
                                std::uint32_t &actual_destination, SourceBootSearchBoundary *optional_cursor);
// Full4631F4 on the live written caller-local span; no leading42C6A0 trim,
// staging, initial string scan, std::atoi, or guessed ASCII classification.
// offset/read diagnostics refer to the local span, not an invented stack VA.
std::uint32_t parse_source_local_integer_4631f4(RaceSessionPlatform &,
                                                std::span<const std::uint8_t> actual_written_local,
                                                std::uint32_t offset);
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t parse_source_bank_integer_4631f4(RaceSessionPlatform &, SourceBootSearchBoundary);
}
