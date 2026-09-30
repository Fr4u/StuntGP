#pragma once
#include "sgn/source_boot_config.hpp"
#include <span>

namespace sgn::retail
{
// Synchronous borrowed needle: either the actual caller-local/boot-config
// bytes, or a live App literal address. Never retains or copies script data.
struct SourceScriptNeedle
{
    std::span<const char> local;
    std::uint32_t app_address{};
    bool from_app{};
};
std::uint8_t read_source_script_byte(RaceSessionPlatform &, SourceBootSearchBoundary, std::uint32_t source_site);
SourceBootSearchBoundary find_source_script_462890(RaceSessionPlatform &, SourceBootSearchBoundary, SourceScriptNeedle);
std::uint32_t find_source_script_key_42c580(RaceSessionPlatform &, SourceBootSearchBoundary input, SourceScriptNeedle,
                                            SourceBootSearchBoundary *cursor);
std::uint32_t select_source_level_key_42d0ee(RaceSessionPlatform &, SourceBootSearchBoundary input,
                                             SourceBootSearchBoundary &cursor);
SourceBootSearchBoundary find_source_level_end_42d0fc(RaceSessionPlatform &, SourceBootSearchBoundary input);
} // namespace sgn::retail
