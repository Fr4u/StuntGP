#include "sgn/retail_level_selection.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include <algorithm>
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint16_t word(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint16_t>(b[at] | (std::uint16_t(b[at + 1]) << 8));
}
std::uint32_t dword(std::span<const std::uint8_t> b, std::size_t at)
{
    return word(b, at) | (std::uint32_t(word(b, at + 2)) << 16);
}
auto recording(ReplayBank &bank, ReplayHandle handle)
{
    return bank.bytes().subspan(ReplayBank::base(handle), ReplayBank::stride);
}
void copy_name(const SelectionRecord &from, LevelCarSlot &to)
{
    const auto end = std::find(from.name.begin(), from.name.end(), '\0');
    if (end == from.name.end())
        throw FormatError("selection name outside recovered bounded string domain");
    std::copy(from.name.begin(), end + 1, to.name.begin()); // preserve bytes after NUL
}
void apply_skill(const SelectionRecord &from, LevelCarSlot &to)
{
    if (from.ai_skill != 0)
        to.settings.ai_skill = from.ai_skill;
    if (from.ai_skill != 1000)
        to.settings.ai_mode = from.ai_mode;
}
// 482C08..482CB7. Last pair intentionally aliases Car4/Car4sh.
constexpr std::array<std::string_view, 22> models{
    "DUMMY", "Car1",  "Car2",  "Car3",  "Car4",  "Car5",  "Car6",  "Car7",  "Car8",  "Car9",  "Car10",
    "Car11", "Car12", "Car13", "Car14", "Car15", "Car16", "Car17", "Car18", "Car19", "Car20", "Car4"};
constexpr std::array<std::string_view, 22> shadows{"DUMMY",   "Car1sh",  "Car2sh",  "Car3sh",  "Car4sh",  "Car5sh",
                                                   "Car6sh",  "Car7sh",  "Car8sh",  "Car9sh",  "Car10sh", "Car11sh",
                                                   "Car12sh", "Car13sh", "Car14sh", "Car15sh", "Car16sh", "Car17sh",
                                                   "Car18sh", "Car19sh", "Car20sh", "Car4sh"};
} // namespace
void randomize_vehicle_liveries(VehicleTextureSelection &state, VehicleRandom &random)
{
    state.used_liveries.fill(0);
    unsigned physical = 0, attempts = 0;
    while (physical < 6)
    {
        const auto draw = vehicle_random_integer(random, 10000);
        auto index = static_cast<unsigned>(draw < 0 ? -draw : draw) % 6;
        ++attempts;
        if (attempts > 8)
            while (state.used_liveries[index] != 0)
                index = (index + 1) % 6;
        if (state.used_liveries[index] != 0)
            continue;
        state.used_liveries[index] = 1;
        state.random_liveries[physical++] = static_cast<std::int8_t>(index + 2);
        attempts = 0;
    }
}
void reset_time_trial_ghosts(TimeTrialGhosts &ghosts, TimeTrialSelectionState &state)
{
    state.availability.fill(0);
    constexpr std::array<unsigned, 4> vehicles{4, 3, 1, 2};
    for (unsigned i = 0; i < 4; ++i)
        ghosts.roles[i] = {0, vehicles[i], ReplayHandle{i + 1}, 0xffffffffu};
}
void load_track_replay(LevelSelectionContext &c)
{
    c.time_trial.track = {};
    if (c.level_name.empty() || c.level_name.size() >= 16 || c.level_name.find_first_of("/\\:") != c.level_name.npos ||
        c.level_name.find('\0') != c.level_name.npos || c.level_name.find("..") != c.level_name.npos)
        throw FormatError("level replay name outside recovered bounded domain");
    const auto key = canonical("replays\\" + std::string(c.level_name) + ".rpl");
    const auto names = c.assets.resources(key);
    if (std::find(names.begin(), names.end(), key) == names.end())
        return;
    const auto input = c.assets.resource(key);
    if (input.empty())
        return; // original exists probe returns the byte count
    // Qualified authored files contain a whole header and fit one recording.
    // The retail 0x20000 cap can reach another header; partial/oversized records
    // need separate evidence and must not introduce mixed pointer/offset bytes.
    if (input.size() < ReplayBank::header_bytes || input.size() > ReplayBank::stride)
        throw FormatError("replay import outside recovered complete-header single-slot domain");
    constexpr std::uint32_t original_arena = ReplayBank::source_base;
    const auto cursor = dword(input, 0x10);
    if (cursor < original_arena || cursor >= original_arena + ReplayBank::size)
        throw FormatError("serialized replay cursor outside original arena");
    // Validate this existing authored-file domain before changing the bank.
    // Preserve ALL file bytes, including stale header+10 source address.
    // begin_playback later supplies the actual slot cursor at41AB14.
    std::copy(input.begin(), input.end(), c.replays.bytes().begin());
    c.time_trial.track = {dword(c.replays.bytes(), 0x3c), {}, ReplayHandle{0}};
}
void prepare_time_trial_selection(LevelSelectionContext &c)
{
    auto &prototype = c.selection[0];
    if (prototype.livery == 0 || prototype.livery == 8)
    {
        const auto random = vehicle_random_integer(c.random, 5);
        prototype.livery = static_cast<std::uint32_t>(random < 0 ? -random : random) + 2;
    }
    prototype.ai_skill = 1;
    for (unsigned i = 1; i < 3; ++i)
    {
        c.selection[i] = prototype;
        c.selection[i].control = 2;
    }
    if (const auto selected = c.time_trial.selected.replay)
    {
        const auto header = recording(c.replays, *selected);
        // 443010 does not copy selected bytes and reads lap time from slot 2.
        c.time_trial.availability[0] = 1;
        auto &ghost = c.ghosts.roles[1];
        ghost.vehicle = 3;
        ghost.replay = {2};
        ghost.lap_ticks = dword(recording(c.replays, {2}), 0x1c);
        c.time_trial.selected = {};
        c.selection[3] = prototype;
        c.selection[3].model = word(header, 0x20);
        c.selection[3].config = word(header, 0x22);
        c.selection[3].tyre = word(header, 0x26);
        c.selection[3].livery = word(header, 0x24);
    }
    else
        c.selection[3] = prototype;
    load_track_replay(c);
    unsigned unused = 4;
    if (const auto loaded = c.time_trial.track.replay)
    {
        const auto header = recording(c.replays, *loaded);
        c.time_trial.availability[1] = 1;
        std::memmove(recording(c.replays, {1}).data(), header.data(), ReplayBank::stride);
        auto &ghost = c.ghosts.roles[0];
        ghost.vehicle = 4;
        ghost.replay = {1};
        ghost.lap_ticks = dword(recording(c.replays, {1}), 0x1c);
        c.time_trial.track = {};
        c.selection[4] = prototype;
        c.selection[4].model = word(header, 0x20);
        c.selection[4].config = word(header, 0x22);
        c.selection[4].tyre = word(header, 0x26);
        c.selection[4].livery = 1;
        unused = 5;
    }
    for (unsigned i = unused; i < 6; ++i)
        c.selection[i] = {};
}
void prepare_time_trial_level(LevelSelectionContext &c, LevelOverrideState &level)
{
    reset_time_trial_ghosts(c.ghosts, c.time_trial);
    prepare_time_trial_selection(c);
    c.time_trial.split_selection = 0;
    level.split = 0;
    c.time_trial.result_b = {};
    c.time_trial.result_a = {};
    c.ghosts.reference_completed = c.time_trial.previous_completed;
}
void prepare_level_cars(LevelSelectionContext &c)
{
    c.vehicles.clear_vehicle_storage();
    const auto selected =
        std::count_if(c.selection.begin(), c.selection.end(), [](const auto &r) { return r.kind == 2; });
    const bool low_memory = c.memory_bytes <= 0x400000 && selected == 6;
    std::optional<unsigned> previous;
    bool copied = false;
    std::uint32_t resource_type = 4;
    for (unsigned i = 0; i < 6; ++i)
    {
        auto &from = c.selection[i];
        auto &to = c.cars[i];
        if (from.kind != 2)
            continue;
        if (from.model >= models.size())
            throw FormatError("selected model outside recovered table");
        copy_name(from, to);
        if (from.tyre != 0)
            to.settings.tyre = from.tyre;
        if (from.config != 0)
            to.settings.config = from.config;
        to.model = from.model;
        if (from.livery != 8)
            to.settings.livery = from.livery;
        if (from.control == 0)
            from.ai_skill = 1;
        apply_skill(from, to);
        auto model = from.model;
        if (low_memory && previous && from.control != 0 && !copied)
        {
            copied = true;
            to = c.cars[*previous];
            apply_skill(from, to);
            if (from.tyre != 0)
                to.settings.tyre = from.tyre;
            copy_name(from, to);
            model = c.selection[*previous].model;
        }
        c.bindings.requests.push_back({std::string(models[model]), resource_type++});
        c.bindings.requests.push_back({std::string(low_memory ? shadows[7] : shadows[model]), resource_type++});
        if (low_memory && !previous && from.control != 0)
            previous = i;
    }
}
void LevelSelectionDependencies::prepare_time_trial(LevelOverrideState &state)
{
    prepare_time_trial_level(context_, state);
}
void LevelSelectionDependencies::prepare_cars(LevelOverrideState &)
{
    prepare_level_cars(context_);
}
} // namespace sgn::retail
