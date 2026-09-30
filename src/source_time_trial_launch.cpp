#include "sgn/source_time_trial_launch.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/race_session_scene.hpp"
#include <algorithm>
#include <cstring>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
Word read(FrontendState &f, Word address)
{
    const auto b = f.source.source_bytes(address, 4);
    return Word(b[0]) | (Word(b[1]) << 8) | (Word(b[2]) << 16) | (Word(b[3]) << 24);
}
ReplayHandle recording_from_source_address(Word address)
{
    // 443050 publishes5096C0, the actual existing slot2. No heap buffer or
    // alternative identity is invented for44D740's optional imported replay.
    const auto offset = address - ReplayBank::source_base;
    if (offset >= ReplayBank::size || offset % ReplayBank::stride != 0)
        throw FormatError("43A947 recording address does not name an existing ReplayBank slot");
    return {offset / ReplayBank::stride};
}
} // namespace
void prepare_frontend_time_trial_race_43a910(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    f.source.race_return_selector = 3; // 43A91A
    f.source.bind_time_trial_return(); // 43A920/926, before either source child
    // Both43D420 calls are exactly XOR EAX,EAX;RET.43A720 uses existing owners.
    auto view = frontend_launch_world(f);
    clear_frontend_race_selection(view);
    auto &world = f.world();
    auto &trial = world.trial_selection;
    auto &options = world.options;
    auto &launch = world.frontend_launch;
    trial.previous_completed = read(f, 0x61c1e4); // 43A936..93B, launch+14
    const auto recording = read(f, 0x61c1e8);     // 43A940, launch+18
    if (recording != 0)
    {
        const auto handle = recording_from_source_address(recording);
        const auto source = world.replays.bytes();
        const auto at = ReplayBank::base(handle) + 0x3cu;
        const Word header = Word(source[at]) | (Word(source[at + 1]) << 8) | (Word(source[at + 2]) << 16) |
                            (Word(source[at + 3]) << 24); // 43A947 BEFORE publication
        trial.selected.replay = handle;                   // 43A94A, native representation of60DF8C
        trial.selected.header_value = header;             // 43A94F
    }
    else
    {
        trial.selected.replay.reset();   // 43A957
        trial.selected.header_value = 0; // 43A95D
    }
    const auto track = read(f, 0x61c1d8); // 43A963 BEFORE60DF88 clear
    trial.selected.vehicle.reset();       // 43A966
    options.render_b = 0;
    options.split = 0;
    options.demo_override = 0;
    options.record_f008 = 0;
    const auto music = read(f, 0x5fcb78u + track * 24u);   // 43A98B retained EDI
    const auto texture = read(f, 0x5fcb88u + track * 24u); // 43A991
    options.texture = texture;
    launch.track_index = track - 1u;     // 43A997/99C
    const auto laps = read(f, 0x61c1dc); // 43A9A2
    options.game_type = 4;
    options.start_delay = 4;                   // 43A9AA/9AF
    const auto record = read(f, 0x61c1e0);     // 43A9B4
    options.laps = laps;                       // 43A9B7
    const auto controller = read(f, 0x5e37a4); // 43A9BD
    options.record_f00c = 0;
    options.music = music;
    options.race_mode = 3;
    launch.prepared = 1;
    options.record = record;
    options.record_f004 = 36000;
    bind_frontend_race_input(launch, controller, 0, 1); // 43A9F2 ->43A760/45ECC0

    // 43D690: capture both source reads before writing2+1 bytes to the actual
    // 47966C scratch.479668 is the pinned, never-written pointer to that owner.
    const auto initials = f.source.source_bytes(0x61c1d0, 2);
    const Word first = Word(initials[0]) | (Word(initials[1]) << 8);
    const auto third = f.source.source_bytes(0x61c1d2, 1)[0];
    auto &scratch = f.source.initials_scratch;
    scratch[0] = static_cast<std::uint8_t>(first);
    scratch[1] = static_cast<std::uint8_t>(first >> 8);
    scratch[2] = third; // fourth byte retained
    const auto end = std::find(scratch.begin(), scratch.end(), std::uint8_t{});
    if (end == scratch.end())
        throw FormatError("43AA05 initials scan leaves actual47966C owner");
    const auto length = static_cast<std::size_t>(end - scratch.begin()) + 1u;
    auto &selected = world.selection[0];
    // 43AA18 full DWORD copies, then43AA24 trailing bytes; captured length.
    std::size_t copied = 0;
    for (; copied + 4u <= length; copied += 4u)
    {
        Word value;
        std::memcpy(&value, scratch.data() + copied, 4);
        std::memcpy(selected.name.data() + copied, &value, 4);
    }
    for (; copied < length; ++copied)
        std::memcpy(selected.name.data() + copied, scratch.data() + copied, 1);
    selected.control = 0;
    trial.split_selection = 0;                                          // 43AA26/2B
    const auto model = read(f, 0x61c1d4);                               // 43AA30 BEFORE kind store
    selected.kind = 2;                                                  // 43AA39
    prepare_frontend_stock_configuration_43a430(f, selected, model, 0); // 43AA43
    // 43AA4B tail43D420 is literal RET; Start25 and transition belong to caller.
}
} // namespace sgn::retail
