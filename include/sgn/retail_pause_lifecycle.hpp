#pragma once
#include "sgn/retail_pause_hud.hpp"
#include "sgn/retail_pause_music.hpp"
#include "sgn/vehicle_audio.hpp"

namespace sgn::retail
{
// Stable native identity for source61B744: token0 is null; token1..4 denotes
// physical input slot0..3 (retail62D580+i*1DC). It is never a controller ID,
// vehicle ID or host pointer. This value type creates no persistent owner.
struct PausePhysicalInputIdentity
{
    std::uint32_t token{};
};
struct PauseLifecycleWorld
{
    const std::uint32_t &inhibit;               // 60F2C0, only exact1 refuses entry
    std::uint32_t &frame_pause;                 // 61294C
    std::uint32_t &age;                         // 612954, caller-owned, shared with gate/fade
    std::uint32_t &status;                      // 61295C, caller-owned retained word
    std::uint32_t &second_pause;                // 612958
    PausePhysicalInputIdentity &physical_input; // 61B744, caller-owned
    RaceHudView hud;
    std::span<ControllerOverlayTransition, 4> overlays;
    std::span<const VehicleState, 6> vehicles;
    VehicleAudioWorld &vehicle_audio;
    LevelAudioState &level_audio;
    PauseMusicWorld music;
};
// Actual operands consumed by442350; no native audio service is borrowed.
struct PauseResumeWorld
{
    std::uint32_t &frame_pause;
    std::uint32_t &status;
    std::uint32_t &second_pause;
    PausePhysicalInputIdentity &physical_input;
    RaceHudView hud;
    std::span<ControllerOverlayTransition, 4> overlays;
    std::span<const VehicleState, 6> vehicles;
    LevelAudioState &level_audio;
    PauseMusicWorld music;
};
void resume_pause_442350(PauseResumeWorld);
// Full4422C0..442341 and442350..4423CA. Synchronous distinct borrowed storage;
// no concurrent or platform-callback mutation. All views refer to the same
// existing controls, physical vehicles and audio owners. vehicle_audio cars[i]
// has id i and refers to vehicles[i]; its driver uses hud.controls/level_audio;
// music.output_a refers to level_audio.output_a. Resume resolves each vehicle
// at its reached field access, retaining earlier stores on failure. Entry
// requires four mappings0..5. Refused entry reads only inhibit, and performs
// no validation or access of unrelated state. Both APIs return typed void.
void enter_pause_4422c0(PauseLifecycleWorld);
void resume_pause_442350(PauseLifecycleWorld);
} // namespace sgn::retail
