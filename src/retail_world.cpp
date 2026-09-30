#include "sgn/retail_world.hpp"
#include <string>
#include <stdexcept>

namespace sgn::retail
{
std::optional<unsigned> select_replacement_ghost(const TimeTrialGhosts &state) noexcept
{
    std::uint32_t largest = 0;
    std::optional<unsigned> result;
    for (unsigned i = 2; i < 4; ++i)
        if (state.roles[i].lap_ticks > largest)
        {
            largest = state.roles[i].lap_ticks;
            result = i;
        }
    return result;
}
void select_active_ghosts(TimeTrialGhosts &state) noexcept
{
    if (state.roles[0].active == 1)
    {
        if (state.reference_completed == 1)
            state.roles[0].active = 0;
        else
            for (unsigned i = 1; i < 4; ++i)
                state.roles[i].active = 0;
        return;
    }
    if (state.roles[1].active != 1)
        return;
    unsigned count = 0;
    std::uint32_t largest = 0;
    std::optional<unsigned> slowest;
    for (unsigned i = 1; i < 4; ++i)
    {
        const auto ticks = state.roles[i].lap_ticks;
        if (ticks == 0 || ticks >= 7200)
            continue;
        state.roles[i].active = 1;
        if (ticks > largest)
        {
            largest = ticks;
            slowest = i;
        }
        ++count;
    }
    if (slowest && count > 2)
        state.roles[*slowest].active = 0;
}
void hold_ghost_collisions(const TimeTrialGhosts &ghosts, std::span<VehicleRaceState, 6> cars)
{
    for (const auto &role : ghosts.roles)
        if (role.active == 1 && role.vehicle >= cars.size())
            throw std::invalid_argument("ghost vehicle outside six source slots");
    for (const auto &role : ghosts.roles)
        if (role.active == 1)
            cars[role.vehicle].collision_hold = 60;
}
void update_ranking(std::span<VehicleRaceState, 6> cars, RaceRanking &order,
                    const std::array<std::string_view, 6> &names, std::string_view leader_text,
                    RankingAnnouncements &output)
{
    order.slots.fill(std::nullopt);
    order.count = 0;
    for (unsigned i = 0; i < 6; ++i)
        if (cars[i].active == 1)
            order.slots[order.count++] = i;
    // Preserve the original pairwise swaps, including ties and unordered floats.
    for (unsigned i = 0; i < order.count; ++i)
        for (unsigned j = 0; j < order.count; ++j)
            if (cars[*order.slots[i]].progress > cars[*order.slots[j]].progress)
                std::swap(order.slots[i], order.slots[j]);
    for (unsigned rank = 0; rank < order.count; ++rank)
    {
        const unsigned leader = *order.slots[0];
        if (rank == 0 && cars[leader].place != 0)
        {
            // The original sprintf("%s %s") uses a 32-byte local buffer.
            // Valid source strings fit it; reject overflow rather than emulate it.
            if (names[leader].find('\0') != std::string_view::npos ||
                leader_text.find('\0') != std::string_view::npos || names[leader].size() > 30 ||
                leader_text.size() > 30 - names[leader].size())
                throw std::invalid_argument("ranking message outside source buffer");
            std::string message(names[leader]);
            message += ' ';
            message += leader_text;
            output.leader(leader, message);
        }
        cars[*order.slots[rank]].place = rank;
    }
}
namespace
{
unsigned current(const WorldTraversal &world)
{
    if (world.current_slot >= 6)
        throw std::out_of_range("source current vehicle outside six slots");
    return world.current_slot;
}
bool playback(const VehicleRaceState &race, const WorldSteps &steps)
{
    return race.recording && steps.replay_status(*race.recording) == 1;
}
} // namespace
void world_contacts(WorldTraversal &world, WorldSteps &steps)
{
    world.current_slot = 0;
    for (unsigned remaining = 6; remaining; --remaining)
    {
        const auto i = current(world);
        if (world.race[i].active == 1 && !playback(world.race[i], steps))
        {
            const float height = world.physics[i].position.y;
            // The first x87 test also takes the branch for unordered input.
            if (!(height >= -5000.f) || height > 60000.f || (world.game_mode == 3 && height < 160.f))
            {
                world.race[i].respawn_requested = 1;
                world.race[i].unsafe_contact_ticks = 0;
            }
            steps.contact(i);
        }
        ++world.current_slot;
    }
}
void reset_air_factors(std::span<VehicleState, 6> cars) noexcept
{
    for (auto &car : cars)
        car.air_factor = 1;
}
void world_tick(WorldTraversal &world, WorldSteps &steps)
{
    world.current_slot = 0;
    world_contacts(world, steps);
    reset_air_factors(world.physics);
    world.current_slot = 0;
    for (unsigned remaining = 6; remaining; --remaining)
    {
        if (world.race[current(world)].active == 1)
        {
            steps.advance_replay(world.current_slot);
            const auto i = current(world);
            if (playback(world.race[i], steps))
                steps.playback_pose(i);
            else
                steps.vehicle_step(i);
        }
        ++world.current_slot;
    }
    steps.ranking();
    world.current_slot = 0;
}
} // namespace sgn::retail
