#pragma once
#include "sgn/retail_pause_menu.hpp"
#include <array>
#include <cstdint>

namespace sgn::retail
{
// A pair of raw DWORD owners, not Boolean buttons or a contiguous physical
// record. The original four record bases are separated by0x1DC bytes.
struct SessionPhysicalInputWords
{
    std::uint32_t word_0{}; // 62D580 + slot*1DC
    std::uint32_t word_4{}; // 62D584 + slot*1DC
};

// Minimal consumer storage:4 previous-pause bytes +32 raw-word bytes +32 edge
// bytes. This is NOT the complete14-byte current/previous producer bank.
// Keep frontend_launch.input_bindings as the sole+18/+1C owner. No polling,
// guessed key mapping, implicit45EC30 reset or raw-activity612970 copy here.
struct SessionMenuInputState
{
    SessionMenuInputState() = default;
    SessionMenuInputState(const SessionMenuInputState &) = delete;
    SessionMenuInputState &operator=(const SessionMenuInputState &) = delete;
    SessionMenuInputState(SessionMenuInputState &&) = delete;
    SessionMenuInputState &operator=(SessionMenuInputState &&) = delete;
    std::uint32_t previous_pause{}; // 612950; not frame_pause61294C
    std::array<SessionPhysicalInputWords, 4> raw_words{};
    // Accepted eight-byte paired native layout; map each source byte explicitly:
    // up1AA/1B1,down1AB/1B2,activate1AC/1B3,back1AE/1B5.
    // Never copy seven contiguous source bytes through this packed subset.
    std::array<PauseMenuInputEdges, 4> edges{};
};
} // namespace sgn::retail
