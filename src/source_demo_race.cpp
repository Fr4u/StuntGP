#include "sgn/source_demo_race.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/race_session_scene.hpp"

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
std::uint8_t byte(FrontendState &f, Word address)
{
    return f.source.source_bytes(address, 1)[0];
}
} // namespace
Word &source_demo_index_627fdc(FrontendState &f)
{
    f.shared_platform()->require_owner_thread();
    const auto app = f.shared_platform()->source_application_literals();
    return app.demo_index_627fdc();
}
void prepare_frontend_demo_43b0e0(FrontendState &f, Word record)
{
    auto platform = f.shared_platform();
    platform->require_owner_thread();
    const auto app = platform->source_application_literals();
    // 43B0E6 reads before43B0F4. The source scans to a sentinel but does not
    // publish the scan result; keep the actual App-owned literal table borrow.
    const auto first = app.demo_scan_word_43b0e6(0);
    f.source.race_return_selector = 8;
    if (first != 0)
    {
        Word i = 1;
        while (app.demo_scan_word_43b0e6(i) != 0)
            ++i;
    }
    // 43D420 is XOR EAX,EAX;RET.43A720 remains the existing sole clear.
    auto launch_view = frontend_launch_world(f);
    clear_frontend_race_selection(launch_view);
    auto &world = f.world();
    auto &options = world.options;
    const auto track = read(f, record);
    options.laps = 4;
    options.start_delay = 4;
    options.render_b = 1;
    world.frontend_launch.track_index = track - 1u;
    const auto music = read(f, 0x5fcb78u + track * 24u);
    options.texture = 8;
    options.music = music;
    auto cursor = record + 0x10u;
    options.split = 0;
    options.game_type = 0;
    options.race_mode = 0;
    options.demo_override = 0x1518;
    world.frontend_launch.prepared = 1;
    for (Word row = 0; row < 6; ++row)
    {
        auto &selected = world.selection[row];
        const auto name = cursor - 12u;
        Word length = 0;
        while (byte(f, name + length) != 0)
            ++length;
        ++length;
        auto put = [&](Word at, std::uint8_t value) {
            if (at >= selected.name.size())
                throw FormatError("43B0E0 name copy leaves represented SelectionRecord name");
            selected.name[at] = static_cast<char>(value);
        };
        // Full DWORD copies precede model/livery reads. Trailing byte copies
        // follow those reads, as43B1A6..43B1B2. Partial writes survive rejection.
        Word copied = 0;
        for (; copied + 4u <= length; copied += 4u)
        {
            const auto value = read(f, name + copied);
            for (Word i = 0; i < 4; ++i)
                put(copied + i, static_cast<std::uint8_t>(value >> (i * 8u)));
        }
        const auto livery = read(f, cursor), model = read(f, cursor - 4u);
        for (; copied < length; ++copied)
            put(copied, byte(f, name + copied));
        selected.kind = 2;
        selected.control = 1;
        prepare_frontend_stock_configuration_43a430(f, selected, model, livery);
        const auto skill = read(f, cursor + 8u), mode = read(f, cursor + 4u);
        selected.ai_skill = skill;
        selected.ai_mode = mode;
        cursor += 24u;
    }
    // Final43D420 is also literal RET. Caller Title owns index increment and Start25.
}
} // namespace sgn::retail
