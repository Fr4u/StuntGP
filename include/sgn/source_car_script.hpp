#pragma once
#include "sgn/source_boot_config.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace sgn::retail
{
// The required remaining CRT child. It receives the live format address and
// integer argument, and writes the actual caller local. No built-in key table,
// std::format fallback or default success is provided. Output must include NUL.
struct SourceCarKeyCall
{
    std::array<char, 64> bytes;
    std::uint32_t initialized_prefix{};
};
class SourceCarKeyFormatter
{
  public:
    virtual ~SourceCarKeyFormatter() = default;
    // Full live CRT classification child:495FBC branch,495DB0 table or4682CB.
    // Return source bitmask result, not a cached/default ASCII assumption.
    virtual std::uint32_t classify_number_4631f4(RaceSessionPlatform &, std::uint8_t character, std::uint32_t mask) = 0;
    virtual void format_car_key_46280c(RaceSessionPlatform &, std::uint32_t format_address, std::uint32_t car_number,
                                       SourceCarKeyCall &) = 0;
};
// Full six-record loops through RET, using the existing bank script and cars.
// Concrete Mechanic adapter supplies live numeric classification and %d.
// Bank-backed462890 preserves reached BYTE/DWORD reads within owned payloads.
// Source589624 is read live after each key search.
void configure_source_cars_42c9b0(RaceSessionPlatform &, SourceBootSearchBoundary, SourceCarKeyFormatter &);
void configure_source_replays_42cb00(RaceSessionPlatform &, SourceBootSearchBoundary, SourceCarKeyFormatter &);
void configure_source_tyres_42ca20(RaceSessionPlatform &, SourceBootSearchBoundary, SourceCarKeyFormatter &);
void configure_source_liveries_42ca90(RaceSessionPlatform &, SourceBootSearchBoundary, SourceCarKeyFormatter &);
void configure_source_ai_42cb70(RaceSessionPlatform &, SourceBootSearchBoundary, SourceCarKeyFormatter &);
} // namespace sgn::retail
