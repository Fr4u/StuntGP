#pragma once
#include "sgn/source_application_literals.hpp"
#include <cstdint>
#include <span>
namespace sgn::retail
{
class RaceSessionPlatform;
// Actual46DD90 argument order: local first, live App literal second.
// Full C-locale route, exact-byte non-C route. Non-C mismatch explicitly
// requires the unimplemented46AA6C/NLS route; never folds it as ASCII.
std::int32_t compare_source_level_kind_46dd90(const SourceApplicationLiteralRef &,
                                              std::span<const std::uint8_t> written_local,
                                              std::uint32_t app_literal_address);
std::int32_t compare_source_level_kind_46dd90(RaceSessionPlatform &, std::span<const std::uint8_t> written_local,
                                              std::uint32_t app_literal_address);
} // namespace sgn::retail
