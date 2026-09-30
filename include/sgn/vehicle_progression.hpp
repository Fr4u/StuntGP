#pragma once
#include "sgn/retail_world.hpp"
#include "sgn/retail_replay.hpp"
#include "sgn/retail_spline.hpp"
#include "sgn/vehicle_ai_automatic.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include <string>

namespace sgn::retail
{
// Only previously unowned vehicle words. Race, physics and AI retain their
// existing owners, including the original alias at lap_score[25] == 24A4.
struct VehicleProgressionState
{
    float previous_projection{};                // 23B4
    std::uint32_t record_improved{};            // 23C0
    std::uint32_t lap_time_24{};                // 243C (after RaceState's 24 lap times)
    std::array<std::uint32_t, 25> lap_scores{}; // 2440..24A0
    std::uint32_t checkpoint_index{};           // 24EC
    std::array<std::uint32_t, 10> checkpoint_times{}, personal_checkpoint_times{},
        record_checkpoint_times{}; // 24F0/2518/2540
};
// Sole persistent owner. Initial level parsing transfers these values here;
// LevelSettings/LevelOverrideState are initialization records, not live copies.
struct ProgressionRecordState
{
    std::uint32_t previous_lap_ticks{}, lap_ticks{}, score{}; // 60EFFC/60F000/60F008
};
struct VehicleProgressionContext
{
    const RaceClockState &clock;
    ProgressionRecordState &records;
    const std::optional<SplineView> &start_spline; // borrow VehicleGridState.start_spline
    Vec3 unit_gravity;
    contact::player::Controls &controls;
    unsigned current;
    std::uint32_t tick;
    const std::uint32_t &record_enabled; // borrow TimeTrialSelectionState.availability[1]
    const TimeTrialGhosts &ghosts;       // role0.replay == source60D630
    const ReplayBank &replays;           // record threshold is its header+1C, not ghost.lap_ticks
    std::array<std::string_view, 6> names;
    std::string_view checkpoint_text, units_text, ahead_text, behind_text; // 612804/808/814/818
    VehicleStuntChainAudio &audio;
};
void tick_vehicle_race_timers(const VehicleState &, VehicleRaceState &, const RaceClockState &) noexcept; // 41DE70
void accept_vehicle_progress(VehicleRaceState &, std::int32_t) noexcept;                                  // 41D9C0
// Source copies unwritten stack W only into respawn_heading.w. Native retains
// that opaque lane; the returned observation permits an execution-based mask.
struct VehicleProgressionObservation
{
    bool heading_opaque{}, adjacent_storage_read{};
};
VehicleProgressionObservation project_vehicle_progress(VehicleRaceState &, VehicleProgressionState &, Vec3 position,
                                                       SplineView, const SplineView &global_grid,
                                                       Vec3 unit_gravity); // 41DED0
void score_vehicle_lap(const VehicleState &, VehicleRaceState &, VehicleProgressionState &,
                       ProgressionRecordState &);                                                          // 41DCE0
void record_vehicle_lap(VehicleRaceState &, VehicleProgressionState &, const VehicleProgressionContext &); // 41DC30
std::string format_progression_time(std::uint32_t); // 41D3F0 +4482A0
void announce_vehicle_checkpoint(const VehicleRaceState &, VehicleProgressionState &,
                                 const VehicleProgressionContext &); // 41D4B0
void check_vehicle_wrong_way(const VehicleState &, VehicleRaceState &, const VehicleProgressionState &,
                             const VehicleProgressionContext &); // 41E0F0
// Explicit intermediate domain: original TimeTrial mode4 and lap_count100.
// This executes every reachable callee; other modes/finite races are rejected
// before mutation until full41DA70/41D5C0/41D8A0/41D8F0 are reconstructed.
void complete_time_trial_lap(const VehicleState &, VehicleRaceState &, VehicleProgressionState &, AiAutomaticMemory &,
                             const VehicleProgressionContext &); // 41DD60 in stated domain
void check_time_trial_checkpoints(const VehicleRaceState &, VehicleProgressionState &,
                                  const VehicleProgressionContext &); // 41D760 in stated domain
VehicleProgressionObservation tick_time_trial_progression(const VehicleState &, VehicleRaceState &,
                                                          VehicleProgressionState &, AiAutomaticMemory &,
                                                          const VehicleProgressionContext &); // 41E210 in stated domain
} // namespace sgn::retail
