#pragma once
#include <cstdint>

namespace sgn::retail
{
struct PauseGateWorld
{
    const std::uint32_t &frame_pause; // 61294C, existing Session owner
    std::uint32_t &age;               // 612954, supplied by caller
    const std::uint32_t &activity;    // 612970, supplied by caller
    std::uint32_t &phase_ticks;       // 60F2C4, existing shared countdown
    std::uint32_t &inhibit;           // 60F2C0, existing startup/lifecycle owner
    const std::uint32_t &other_gate;  // 60D620, meaning/producer unresolved
};
// Full4423D0..442437. Six distinct, valid uint32 objects must remain alive and
// free of concurrent external mutation during this synchronous call. No owner,
// input polling, pause entry/resume or outer-loop scheduling is supplied here.
[[nodiscard]] std::uint32_t advance_pause_gate_4423d0(PauseGateWorld) noexcept;
} // namespace sgn::retail
