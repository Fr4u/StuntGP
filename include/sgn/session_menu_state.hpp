#pragma once
#include "sgn/retail_pause_menu.hpp"
#include <array>
#include <cstdint>

namespace sgn::retail
{
// Mutable source-pointer identities. No executable/native pointer cast and
// no label cache. Each future label view must read its own slot at its actual
// child boundary; reading the right slot before the left text child is wrong.
struct SessionPauseMeterLabelSlots
{
    std::uint32_t left{0x004857b0u};  // 4851E0, initial minus target
    std::uint32_t right{0x004857b4u}; // 4851E4, initial plus target
};

// Exact immutable initial target bytes, not extra mutable source owners.
// Future resolver domain: these two source identities only until additional
// live target owners are proved. Reject an unsupported accessed identity;
// never return a fallback label. No claim of global target immutability.
namespace session_pause_meter_initial_text
{
inline constexpr std::array<std::uint8_t, 4> minus{0x2d, 0, 0, 0}; // 4857B0
inline constexpr std::array<std::uint8_t, 4> plus{0x2b, 0, 0, 0};  // 4857B4
} // namespace session_pause_meter_initial_text

// Source scalars, eight mutable rows and two mutable pointer slots:220 source
// bytes. Once per shared RaceSessionPlatform world, never on menu entry,
// view construction, level detach, restart or presentation replay.
struct SessionMenuState
{
    SessionMenuState() = default;
    SessionMenuState(const SessionMenuState &) = delete;
    SessionMenuState &operator=(const SessionMenuState &) = delete;
    SessionMenuState(SessionMenuState &&) = delete;
    SessionMenuState &operator=(SessionMenuState &&) = delete;
    std::uint32_t release_count{};                               // 61B748; generic442350 does not reset
    std::uint32_t selected_row{};                                // 61B74C
    std::uint32_t highlight{};                                   // 61B750; source invocation phase, not frame time
    std::uint32_t overlay_ready{};                               // 61B754
    std::uint32_t requested_language{1u};                        // 4835E0; distinct from612944 loaded text
    std::array<PauseMenuRow, 8> rows{initial_pause_menu_rows()}; // 485120..4851DF
    SessionPauseMeterLabelSlots meter_labels;
    // NEW HOST (R24): pause GRAPHICS row/panel state. Not a source byte; the
    // 220 source bytes above are unchanged.
    PauseGraphicsHostState host_graphics_r24;
};
} // namespace sgn::retail
