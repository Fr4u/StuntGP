#pragma once
#include <cstdint>

namespace sgn::retail
{
// Three previously unowned process words, once in RaceSessionPlatform's world.
// All three are zero in the pinned PE. No per-frame/level/monitor reset.
struct SourceSchedulerState final
{
    SourceSchedulerState() = default;
    SourceSchedulerState(const SourceSchedulerState &) = delete;
    SourceSchedulerState &operator=(const SourceSchedulerState &) = delete;
    SourceSchedulerState(SourceSchedulerState &&) = delete;
    SourceSchedulerState &operator=(SourceSchedulerState &&) = delete;
    std::uint32_t fixed_mode_a{}; // 60F2AC: DISABLE_FRAMERATE_CORRECTION,42D2E0 parser
    std::uint32_t fixed_mode_b{}; // 60DF9C: explicit zero at442166
    std::uint32_t frequency{};    // 612D0C: lowDWORD QueryPerformanceFrequency,43485D
};
static_assert(sizeof(SourceSchedulerState) == 3 * sizeof(std::uint32_t));
} // namespace sgn::retail
