#pragma once
#include "sgn/retail_tick_clock.hpp"
#include <cstdint>
#include <optional>

namespace sgn::retail
{
enum class SourceOuterStatus
{
    completed,
    force_feedback_required,
    scene_child_required
};
struct SourceOuterOutcome
{
    SourceOuterStatus status{SourceOuterStatus::completed};
    std::uint32_t blocked_address{}, physical_slot{0xffffffffu};
    std::uint64_t entered_iterations{};
};
namespace detail
{
// This is the actual production ordering algorithm. The production Calls type
// is private and binds real children. A receiving recorder checks ONLY this
// control flow; it is never a substitute implementation of a game child.
template <class Calls> SourceOuterOutcome source_outer_body_442810(Calls &calls)
{
    SourceOuterOutcome out;
    calls.numeric_429510();
    calls.camera_4023f0();
    if (const auto stopped = calls.input_45f210_40dad0())
    {
        out.status = SourceOuterStatus::force_feedback_required;
        out.blocked_address = 0x410a10;
        out.physical_slot = *stopped;
        return out;
    }
    calls.clock_442490();
    while (calls.clock().scheduled < calls.clock().observed)
    {
        calls.iteration_442838();
        ++out.entered_iterations;
    }
    calls.audio_446110_462280();
    if (const auto stopped = calls.transition_441dc0())
    {
        out.status = SourceOuterStatus::scene_child_required;
        out.blocked_address = *stopped;
    }
    return out;
}
} // namespace detail
} // namespace sgn::retail
