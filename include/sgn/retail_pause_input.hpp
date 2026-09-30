#pragma once
#include "sgn/retail_frontend_launch.hpp"
#include "sgn/retail_pause_lifecycle.hpp"
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Borrow only the two previously unrepresented DWORDs of each physical input
// record. The caller supplies their values; this module establishes no polling,
// button mapping, reset rule or persistent input owner.
struct PauseInputWordsView
{
    const std::uint32_t &word_0; // 62D580 + physical_slot*1DC
    const std::uint32_t &word_4; // 62D584 + physical_slot*1DC
};
struct PauseInputWorld
{
    const std::uint32_t &scene;                                 // 612BE8; exact1 is the only accepted scene
    std::span<const FrontendInputBinding, 4> physical_bindings; // existing+18/+1C
    std::span<const PauseInputWordsView, 4> physical_words;
    std::uint32_t &previous_pause; // 612950, distinct caller-owned word
    PauseLifecycleWorld lifecycle; // same existing HUD/controls/vehicles/audio
};
// Full45EE60..45EEFA (155bytes/47instructions). Typed void, no EAX status.
// All accessed words are distinct, valid borrowed storage, without concurrent
// or external callback mutation. The nested lifecycle contract applies only
// if its respective child is reached. A scene other than1 accesses only scene.
// +18 is skipped only for exactFFFF; +1C is never read. First press/release
// match returns, including refused entry, nonzero frame_pause and suppressed
// resume. Identity publication follows the real entry child even if inhibited.
// Volatile preserves source reads/stores; it does not synchronize threads.
void update_pause_input_45ee60(PauseInputWorld);
} // namespace sgn::retail
