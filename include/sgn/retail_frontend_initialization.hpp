#pragma once
#include <cstdint>

namespace sgn::retail
{
struct FrontendSourceStorage;
struct FrontendLaunchState;

// Full 43C060 and its four literal leaves in the pinned D3D executable.
// Borrow the existing frontend and world owners; no platform service is needed.
void initialize_frontend_defaults_43c060(FrontendSourceStorage &, const FrontendLaunchState &);

// Full439E30: selector plus six source roster/result pointer pairs. Clear the
// borrowed views, never their destinations. Host-only slots0/2 are not source words.
void clear_frontend_race_return_439e30(FrontendSourceStorage &) noexcept;

// Literal source counter operations on the sole5FF8C0 owner. No audio call.
void initialize_frontend_feedback_gate_4408e0(FrontendSourceStorage &) noexcept;
void increment_frontend_feedback_gate_440900(FrontendSourceStorage &) noexcept;
void decrement_frontend_feedback_gate_440910(FrontendSourceStorage &) noexcept;

// Full437E80, clearing the actual counter and both80-word ranges in source order.
void clear_frontend_draw_lists_437e80(FrontendSourceStorage &) noexcept;

// Full436EB0: clear30 source DWORDs5DA2E8..5DA35F, no child calls.
void clear_frontend_words_436eb0(FrontendSourceStorage &) noexcept;

// Full43DDA0 and its tail43DEE0. State stores only; no native music stop.
void clear_frontend_navigation_tail_43dee0(FrontendSourceStorage &) noexcept;
void initialize_frontend_navigation_43dda0(FrontendSourceStorage &) noexcept;

// Host boundary reporting, not source return values. before4411C0 remains
// for the unchanged standalone descriptor prefix; outer initialization now
// completes437E60,436EB0 and43DDA0/43DEE0; stops43D10F before440AE0.
enum class FrontendInitializationStop : std::uint32_t
{
    before_4411c0 = 0x4411c0,
    before_436eb0 = 0x436eb0,
    before_43dda0 = 0x43dda0,
    before_440ae0 = 0x440ae0
};
inline constexpr std::uint32_t frontend_initialization_parent_callsite = 0x43d100;
inline constexpr std::uint32_t frontend_initialization_material_callsite = 0x437e65;
inline constexpr std::uint32_t frontend_initialization_after_drawing_callsite = 0x43d105;
inline constexpr std::uint32_t frontend_initialization_stop_callsite = 0x43d10f;
void initialize_frontend_drawing_437e60(FrontendSourceStorage &); // full437E60, including438190
[[nodiscard]] FrontendInitializationStop initialize_frontend_prefix_43d0b0(FrontendSourceStorage &,
                                                                           const FrontendLaunchState &);
} // namespace sgn::retail
