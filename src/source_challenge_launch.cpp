#include "sgn/source_challenge_launch.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/race_session_scene.hpp"
#include <cstring>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W read(FrontendState &f, W address, W size = 4)
{
    const auto b = f.source.source_bytes(address, size);
    W value = 0;
    for (W i = 0; i < size; ++i)
        value |= W(b[i]) << (8u * i);
    return value;
}
} // namespace
W copy_frontend_initials_43d690(FrontendState &f, W source_address)
{
    // Capture the represented pointer BEFORE source input reads.43D690 never
    // forces47966C, clears the fourth byte or scans the returned string.
    const auto target = read(f, 0x479668);
    const auto first = read(f, source_address, 2), third = read(f, source_address + 2u, 1);
    const auto two = f.source.source_bytes(target, 2);
    two[0] = static_cast<std::uint8_t>(first);
    two[1] = static_cast<std::uint8_t>(first >> 8u);
    f.source.source_bytes(target + 2u, 1)[0] = static_cast<std::uint8_t>(third);
    return target;
}
void prepare_frontend_challenge_43ae00(FrontendState &f, W roster_address, W result_address)
{
    f.shared_platform()->require_owner_thread();
    f.source.race_return_selector = 4; // 43AE0A, before both borrowed destinations
    f.source.race_return_bindings[4].roster = f.source.source_bytes(roster_address, 20);
    f.source.race_return_bindings[4].result = f.source.source_bytes(result_address, 8);
    // 43D420 is XOR EAX,EAX;RET. The full43A720 uses these same world owners.
    auto view = frontend_launch_world(f);
    clear_frontend_race_selection(view);
    const auto seconds = read(f, roster_address + 0x10u);
    auto &world = f.world();
    auto &options = world.options;
    auto &launch = world.frontend_launch;
    options.render_b = 1;
    options.laps = 1;
    options.race_mode = 1;
    launch.prepared = 1;
    const auto controller = read(f, 0x5e37a4);
    options.challenge_ticks = seconds * 60u;
    const auto track = read(f, roster_address + 8u);
    options.texture = 8;
    options.game_type = 3;
    launch.track_index = track - 1u;
    const auto music = read(f, 0x5fcb78u + track * 24u);
    options.split = 0;
    options.demo_override = 0;
    options.record_f008 = 0;
    const auto record = read(f, roster_address + 12u);
    options.music = music;
    options.start_delay = 4;
    options.record = 7200;
    options.record_f004 = 36000;
    options.record_f00c = record;
    bind_frontend_race_input(launch, controller, 0, 1);
    const auto name = copy_frontend_initials_43d690(f, roster_address);
    W length = 0;
    for (;;)
    {
        const auto byte = read(f, name + length, 1);
        ++length;
        if (length == 0)
            throw FormatError("43AED2 name scan wraps its source extent");
        if (byte == 0)
            break;
    }
    const auto model = read(f, roster_address + 4u); // 43AED8 AFTER strlen, before destination stores
    auto &selected = world.selection[0];
    if (length > selected.name.size())
        throw FormatError("43AEE8 name copy leaves admitted selection-name owner");
    const auto scratch = f.source.source_bytes(name, length);
    std::size_t copied = 0;
    for (; copied + 4u <= length; copied += 4u)
    {
        W value;
        std::memcpy(&value, scratch.data() + copied, 4);
        std::memcpy(selected.name.data() + copied, &value, 4);
    }
    for (; copied < length; ++copied)
        std::memcpy(selected.name.data() + copied, scratch.data() + copied, 1);
    selected.kind = 2;
    selected.control = 0;
    prepare_frontend_stock_configuration_43a430(f, selected, model, 8);
    world.trial_selection.split_selection = 0; // 43AF12 AFTER configuration child
    // 43AF1B tail43D420 is the complete source RET; caller owns Start25.
}
} // namespace sgn::retail
