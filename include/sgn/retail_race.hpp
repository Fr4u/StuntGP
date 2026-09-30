#pragma once
#include "sgn/vehicle.hpp"
#include "sgn/vehicle_replay_handle.hpp"
#include <array>
#include <optional>
#include <span>

namespace sgn::retail
{
// Race/lifecycle-owned fields of each original vehicle. Physical state remains
// in VehicleState. These values must be shared with input, AI and camera adapters.
struct VehicleRaceState
{
    std::int32_t active{};                                              // 1BA8
    std::int32_t control_enabled{}, control_kind{};                     // 2278,227C
    std::uint32_t saved_camera_mode{};                                  // 22C8
    std::int32_t race_index{}, previous_progress{}, current_progress{}; // 23A4/A8/AC
    float progress{}, previous_fraction{};                              // 23B0,23B8
    std::int32_t lap_index{}, lap_timer{}, split_timer{}, race_timer{}; // 23BC/C4/C8/CC
    std::uint32_t stunt_score{}, best_lap_ticks{}, total_ticks{};       // 23D0/D4/D8
    std::array<std::int32_t, 24> lap_times{};                           // 23DC
    std::uint32_t spline_index{}, progress_crossing{};                  // 24A4/A8
    std::uint32_t place{};                                              // 24AC
    VehicleVector respawn_position{}, respawn_heading{}, route_right{}; // 24B0/C0/D0
    std::array<std::int32_t, 3> route_indices{};                        // 2644/264C/2654
    std::array<float, 3> route_fractions{};                             // 2648/2650/2658
    // 24E0 remains VehicleState.energy_exempt; never duplicate it here.
    std::uint32_t finish_state{}, stopped{};                                            // 24E4/E8
    std::uint32_t timing26a8{}, event7678{}, automatic_drive{}, start_countdown{};      // 26A8,7678,767C,7688
    std::uint32_t notice_active{}, notice_ticks{};                                      // 76F0/F4
    std::uint32_t respawn_requested{}, collision_hold{}, input_hold{}, pending_reset{}; // 763C/7640/7644/7680
    std::uint32_t opposing_direction_ticks{}, unsafe_contact_ticks{};                   // 7634/7638
    std::optional<ReplayHandle> recording{};                                            // 7268; native bank identity
};
struct RaceClockState
{
    std::uint32_t game_mode{}, phase{}, countdown_pending{}, rolling_state{}; // 60F2B0/B4/C8/CC
    std::uint32_t race_mode{}, remaining_ticks{}, lap_count{};                // 60EFF0/F4/F8
    float course_length{};                                                    // 60F010
};
void reset_race_record(VehicleRaceState &, VehicleState &) noexcept;       // 0041D300
void enable_vehicle_control(VehicleRaceState &) noexcept;                  // 004436E0
void disable_vehicle_control(VehicleRaceState &, VehicleState &) noexcept; // 00443700
void enable_all_controls(std::span<VehicleRaceState, 6>) noexcept;         // 00443750
void begin_lap_timers(std::span<VehicleRaceState, 6>) noexcept;            // 0041DE40
void arm_start_countdown(std::span<VehicleRaceState, 6>) noexcept;         // 00444C00
void check_rolling_start(RaceClockState &, const VehicleRaceState &current,
                         std::span<VehicleRaceState, 6>) noexcept; // 00443A30
void clear_vehicle_notice(VehicleRaceState &) noexcept;            // 00443E10
void reset_route_tracking(VehicleRaceState &) noexcept;            // 00405350; first two routes only
} // namespace sgn::retail
