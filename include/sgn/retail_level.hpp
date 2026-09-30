#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgn::retail
{
// The parser's source starts at the original selected block value. End is the
// relative 00589624 key-search boundary; value parsing may cross that boundary.
struct LevelBlock
{
    std::string_view text;
    std::size_t end{};
};
std::optional<LevelBlock> find_level_block(std::string_view script, std::string_view label);
std::vector<std::string> discover_level_blocks(std::string_view script); // new catalog, not a retail dispatcher
struct LevelSettings
{
    std::uint32_t game_type{};            // 0060df98, preserved when key missing
    std::uint32_t music_track{};          // 0060dfa4, preserved
    float ground_level{};                 // 0060dfa8
    std::array<char, 64> track_texture{}; // 0060dfac: bytes after NUL are preserved
    std::uint32_t texture_suffix{};       // 0060dfec, preserved for absent/short text
    std::uint32_t time_ticks{};           // 0060eff4
    std::uint32_t lap_count{};            // 0060eff8, preserved
    std::uint32_t lap_record_ticks{};     // 0060f000, preserved
    std::uint32_t rolling_start_offset{}; // 0060f014
    std::uint32_t camera_mode{};          // 0060f01c
    std::uint32_t overlay_demo{};         // 0060f024
    std::uint32_t demo_loop_ticks{};      // 0060f028
    std::string_view texture_name() const;
};
// Complete 0042cc20, no 004453a0/world overrides. Caller owns prior state;
// value-initializing it is a new seed policy, not recovered game initialization.
void apply_level_settings(LevelBlock, LevelSettings &);
struct LevelCarSettings
{
    std::uint32_t config{}, tyre{}, livery{}, replay{}, ai_skill{}, ai_mode{};
};
std::array<LevelCarSettings, 6> parse_level_car_settings(LevelBlock); // 0042c9b0/ca20/ca90/cb00/cb70
struct LevelAssetRequest
{
    std::string name;
    std::uint32_t type{};
};
struct LevelAssetBindings
{
    std::vector<LevelAssetRequest> requests; // original order; 2 sky,3 track,4..15 cars/shadows
    std::optional<std::string> track_name;
};
// Emitted mesh bindings of 0042c740 and optional 0042c880. No GPU/resource loads
// or sky animation effects are claimed. 0042d070 itself does not call 0042c880.
LevelAssetBindings parse_level_asset_bindings(LevelBlock, bool include_script_cars = false);
} // namespace sgn::retail
