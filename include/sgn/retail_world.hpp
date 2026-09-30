#pragma once
#include "sgn/retail_race.hpp"
#include <string_view>

namespace sgn::retail
{
struct TimeTrialGhost
{
    std::uint32_t active{};
    std::uint32_t vehicle{};
    ReplayHandle replay{};
    std::uint32_t lap_ticks{};
};
struct TimeTrialGhosts
{
    std::array<TimeTrialGhost, 4> roles{}; // 0060D628..0060D664
    std::uint32_t reference_completed{};   // 0060F284
};
std::optional<unsigned> select_replacement_ghost(const TimeTrialGhosts &) noexcept;  // 00443300
void select_active_ghosts(TimeTrialGhosts &) noexcept;                               // 00443590
void hold_ghost_collisions(const TimeTrialGhosts &, std::span<VehicleRaceState, 6>); // 00443560

struct RaceRanking
{
    std::array<std::optional<unsigned>, 6> slots{}; // 00612D14
    unsigned count{};                               // 00612D10
};
// Mandatory consumer of the source 00442CF0 announcement. Runtime adaptation
// calls player::post_notice; there is no silent default sink.
struct RankingAnnouncements
{
    virtual ~RankingAnnouncements() = default;
    virtual void leader(unsigned vehicle, std::string_view message) = 0;
};
void update_ranking(std::span<VehicleRaceState, 6>, RaceRanking &, const std::array<std::string_view, 6> &names,
                    std::string_view leader_text, RankingAnnouncements &); // 0041E2D0

// Borrowed, shared state. current_slot may be six between the source phases;
// it is restored to zero after a successful world tick.
struct WorldTraversal
{
    std::span<VehicleState, 6> physics;
    std::span<VehicleRaceState, 6> race;
    unsigned &current_slot;
    std::uint32_t game_mode;
};
// Source dispatch boundary. Each callback is mandatory; the runtime must bind
// qualified implementations, not empty substitutes. Qualifying dispatch alone
// does not qualify the called physics, replay or ranking procedures.
struct WorldSteps
{
    virtual ~WorldSteps() = default;
    virtual std::uint16_t replay_status(ReplayHandle) const = 0;
    virtual void contact(unsigned vehicle) = 0;        // 00409B60, ECX=0
    virtual void advance_replay(unsigned vehicle) = 0; // 0041AF50, before status test
    virtual void playback_pose(unsigned vehicle) = 0;  // 00418970
    virtual void vehicle_step(unsigned vehicle) = 0;   // 00418830
    virtual void ranking() = 0;                        // 0041E2D0
};
void world_contacts(WorldTraversal &, WorldSteps &);         // complete 004189E0 dispatch
void reset_air_factors(std::span<VehicleState, 6>) noexcept; // complete 0040E340
void world_tick(WorldTraversal &, WorldSteps &);             // complete 00418A90 dispatch
} // namespace sgn::retail
