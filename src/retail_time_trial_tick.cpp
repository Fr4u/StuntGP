#include "sgn/retail_time_trial_tick.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
template <class T> T read(const ReplayBank &bank, std::uint32_t at)
{
    const auto bytes = bank.bytes();
    if (at > bytes.size() || sizeof(T) > bytes.size() - at)
        throw FormatError("trial replay read outside arena");
    T value;
    std::memcpy(&value, bytes.data() + at, sizeof(T));
    return value;
}
template <class T> void write(ReplayBank &bank, std::uint32_t at, T value)
{
    auto bytes = bank.bytes();
    if (at > bytes.size() || sizeof(T) > bytes.size() - at)
        throw FormatError("trial replay write outside arena");
    std::memcpy(bytes.data() + at, &value, sizeof(T));
}
void validate_vehicles(const TimeTrialGhostWorld &world)
{
    for (const auto &role : world.ghosts.roles)
    {
        if (role.vehicle >= 6)
            throw FormatError("trial ghost vehicle identity");
        (void)ReplayBank::base(role.replay);
    }
}
void notice(TimeTrialGhostWorld &world, unsigned vehicle, std::string_view text, std::int32_t duration,
            std::int32_t audience, std::int32_t priority)
{
    if (vehicle >= 6)
        throw FormatError("trial notice vehicle identity");
    contact::player::post_notice(world.controls, std::bit_cast<std::int32_t>(world.clock.phase), world.tick, duration,
                                 text, vehicle, audience, priority);
}
} // namespace
std::uint8_t replay_checksum(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint8_t sum = 0;
    for (const auto value : bytes)
        sum = static_cast<std::uint8_t>(sum + value);
    return sum;
}
std::uint32_t update_replay_length(ReplayBank &bank, ReplayHandle handle)
{
    const auto length = std::uint32_t(bank.count(handle)) * 44u + 0xacu;
    write(bank, ReplayBank::base(handle) + 0x3c, length);
    return length;
}
void finalize_replay_header(ReplayBank &bank, ReplayHandle handle, const LevelCarSlot &car)
{
    // 419A60..419B2B. The name copy follows the two +3C stores and may overwrite
    // that word. It is not corrected afterwards: source write order is retained.
    const auto end = std::find(car.name.begin(), car.name.end(), '\0');
    if (end == car.name.end())
        throw FormatError("trial car name has no terminator");
    const auto at = ReplayBank::base(handle);
    stop_replay(bank, handle);
    const auto payload = std::uint32_t(bank.count(handle)) * 44u;
    if (payload > bank.bytes().size() - at - 0x80u)
        throw FormatError("trial checksum beyond replay arena");
    const auto length = update_replay_length(bank, handle);
    write(bank, at + 0x14, 1u);
    write(bank, at + 0x28, std::uint32_t(replay_checksum(bank.bytes().subspan(at + 0x80, payload))));
    write(bank, at + 0x3c, length);
    std::memcpy(bank.bytes().data() + at + 0x2c, car.name.data(), static_cast<std::size_t>(end - car.name.begin()) + 1);
    write(bank, at + 0x20, static_cast<std::uint16_t>(car.model));
    write(bank, at + 0x22, static_cast<std::uint16_t>(car.settings.config));
    write(bank, at + 0x26, static_cast<std::uint16_t>(car.settings.tyre));
    write(bank, at + 0x24, static_cast<std::uint16_t>(car.settings.livery));
}
void update_reference_ghost(TimeTrialGhostWorld &world, unsigned vehicle)
{
    if (vehicle >= 6)
        throw FormatError("trial reference vehicle identity");
    if (world.selection.availability[1] != 1 || world.ghosts.reference_completed == 1)
        return;
    auto &role = world.ghosts.roles[0];
    const auto at = ReplayBank::base(role.replay);
    const auto delta =
        std::bit_cast<std::uint32_t>(world.race[vehicle].split_timer) - read<std::uint32_t>(world.replays, at + 0x1c);
    if ((role.active == 1 || std::bit_cast<std::int32_t>(delta) < 0) && world.ghosts.reference_completed == 0 &&
        world.progression[vehicle].record_improved == 1)
    {
        role.active = 1;
        world.ghosts.reference_completed = 1;
        world.progression[vehicle].record_improved = 0;
        notice(world, vehicle, world.reference_beaten_text, 300, 0, 100);
    }
    if (role.active == 0 && delta < (read<std::uint32_t>(world.replays, at + 0x1c) * 10u) / 100u)
    {
        role.active = 1;
        notice(world, vehicle, world.reference_text, 300, 0, 100);
        world.selection.availability[0] = 0;
    }
}
void update_personal_ghost(TimeTrialGhostWorld &world, unsigned vehicle)
{
    if (vehicle >= 6)
        throw FormatError("trial personal vehicle identity");
    if (world.selection.availability[0] != 1 || world.selection.availability[2] == 1)
        return;
    const auto at = ReplayBank::base(world.ghosts.roles[1].replay);
    if (std::bit_cast<std::uint32_t>(world.race[vehicle].split_timer) < read<std::uint32_t>(world.replays, at + 0x1c))
    {
        world.selection.availability[2] = 1;
        notice(world, vehicle, world.personal_beaten_text, 300, 0, 90);
    }
}
void retain_completed_trial(TimeTrialGhostWorld &world)
{
    // 443480 reads literal car0 throughout, independent of current controller.
    const auto recording = world.race[0].recording;
    if (!recording || world.replays.status(*recording) != 2)
        return;
    stop_replay(world.replays, *recording);
    const auto selected = select_replacement_ghost(world.ghosts);
    if (!selected)
        throw FormatError("trial replacement has no source candidate");
    auto &role = world.ghosts.roles[*selected];
    const auto ticks = std::bit_cast<std::uint32_t>(world.race[0].split_timer);
    if (ticks >= role.lap_ticks)
        return;
    if (role.vehicle >= 6)
        throw FormatError("trial retained ghost vehicle identity");
    const auto destination = ReplayBank::base(role.replay), source = ReplayBank::base(*recording);
    std::memmove(world.replays.bytes().data() + destination, world.replays.bytes().data() + source, ReplayBank::stride);
    role.lap_ticks = ticks;
    role.active = 1;
    write(world.replays, destination + 0x1c, ticks);
    finalize_replay_header(world.replays, role.replay, world.cars[role.vehicle]);
    update_reference_ghost(world, 0);
    update_personal_ghost(world, 0);
}
void ensure_active_ghosts(TimeTrialGhostWorld &world)
{
    validate_vehicles(world);
    for (const auto &role : world.ghosts.roles)
    {
        if (role.active == 1 && world.replays.status(role.replay) == 0)
            begin_playback(world.replays, role.replay, world.race[role.vehicle], world.physical[role.vehicle]);
        world.race[role.vehicle].active = std::bit_cast<std::int32_t>(role.active);
    }
}
void restart_active_ghosts(TimeTrialGhostWorld &world)
{
    validate_vehicles(world);
    for (const auto &role : world.ghosts.roles)
    {
        if (role.active == 1)
            begin_playback(world.replays, role.replay, world.race[role.vehicle], world.physical[role.vehicle]);
        world.race[role.vehicle].active = std::bit_cast<std::int32_t>(role.active);
    }
}
void tick_time_trial_ghosts(TimeTrialGhostWorld &world)
{
    validate_vehicles(world);
    if (world.controls.current >= 4 || world.controls.controllers[world.controls.current].vehicle >= 6 ||
        world.current_slot >= 6)
        throw FormatError("trial current vehicle identity");
    const auto vehicle = world.controls.controllers[world.controls.current].vehicle;
    auto &race = world.race[vehicle];
    if (world.start.announcement_ticks != 0 && --world.start.announcement_ticks == 0)
        notice(world, world.current_slot, world.countdown_text, 180, 2, 0);
    if (race.progress_crossing == 1)
    {
        retain_completed_trial(world);
        if (!race.recording)
            throw FormatError("trial crossing has no recording");
        begin_recording(world.replays, *race.recording, race);
        select_active_ghosts(world.ghosts);
        restart_active_ghosts(world);
        race.race_index = 1;
        if (world.start.reference_at_start == 1 && race.lap_index < 1)
        {
            notice(world, vehicle, world.reference_text, 300, 0, 100);
            world.selection.availability[3] = 1;
            hold_ghost_collisions(world.ghosts, world.race);
            return;
        }
    }
    else
    {
        select_active_ghosts(world.ghosts);
        ensure_active_ghosts(world);
    }
    hold_ghost_collisions(world.ghosts, world.race);
}
} // namespace sgn::retail
