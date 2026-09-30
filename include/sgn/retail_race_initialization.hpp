#pragma once
#include "sgn/audio_music_track.hpp"
#include "sgn/camera_race_start.hpp"
#include "sgn/retail_audio_initialize.hpp"
#include "sgn/retail_race_grid.hpp"
#include "sgn/retail_race_hud.hpp"
#include "sgn/retail_race_prepare.hpp"
#include "sgn/retail_vehicle_tuning.hpp"

namespace sgn::retail
{
struct RaceInitializationState
{
    std::array<std::uint32_t, 64> fields_62dcf0{}, fields_62d480{};
    std::uint32_t field_5898c8{}, field_60f2c0{}, start_delay_ticks{}; // last:60D618
};
// Borrow the actual current frame's projection owner. These references are
// outputs, not an additional persistent copy of world rendering state.
struct RaceFrameProjection
{
    CameraMatrix &matrix;
    float &inverse_depth_scale;
    float &depth_offset;
    CameraMatrix &shared_model_identity; // sole61C468, published by executed camera children
};
struct RaceInitializationWorld
{
    VehicleStorage &vehicles;
    const AssetStore &assets;
    VehicleResetWorld &reset;
    const SplineSet &splines;
    VehicleGridState &grid;
    VehicleLoadSelection &load_selection;
    RaceClockState &clock;
    AiAutomaticGlobals &automatic;
    RaceHudView hud;
    std::uint32_t &selected_camera_preset; // 60F020
    RaceFrameProjection frame_projection;
    RaceStartupCounters &counters;
    RaceInitializationState &state;
    AudioStartupWorld &audio;
    AudioMusicTrack &music;
};
// Immutable invocation snapshots of their level/selection owners. They do not
// become another persistent world configuration. AudioDriver.split_setting is
// the sole borrowed 576BE0 value used by both camera and audio initialization.
struct RaceInitializationParameters
{
    std::span<const LevelCarSlot, 6> cars;
    std::span<const SelectionRecord, 6> selection;
    std::span<const std::uint32_t, 4> selected_vehicles; // 60D754/758/75C/760
    std::int32_t control_kind{};                         // 60F018
    std::uint32_t scene_mode{}, music_track{};           // 612BE8,60DFA4
};
struct RaceInitializationObservation
{
    VehicleGridPayloadAudit first_grid, second_grid;
    std::array<bool, 4> camera_position_opaque{};
};
void validate_race_initialization_bindings(const RaceInitializationWorld &);
// Complete 40D500 source order with concrete game implementations. Candidate
// until the whole original caller comparison, including full audio, passes.
RaceInitializationObservation initialize_race(RaceInitializationWorld, const RaceInitializationParameters &);
} // namespace sgn::retail
