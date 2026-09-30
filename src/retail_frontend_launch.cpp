#include "sgn/retail_frontend_launch.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include <algorithm>
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t get(std::span<const std::uint8_t> b, std::size_t at)
{
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("frontend launch word outside source owner");
    std::uint32_t v;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}
std::uint32_t get(std::span<const std::byte> b, std::size_t at)
{
    std::uint32_t v;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}
template <std::size_t N> auto bank(FrontendSourceStorage &f, std::uint32_t at)
{
    return std::as_bytes(std::span<const std::uint8_t, N>{f.source_bytes(at, N)});
}
void publish_stock_configuration(FrontendSourceStorage &f, SelectionRecord &selected, std::uint32_t model,
                                 std::uint32_t livery)
{
    // 43A430's 0x218-byte local holds config0xEC followed by model300. It
    // passes the MODEL suffix to43A4A0, not the prefix or the entire scratch.
    std::array<std::byte, 0x218> scratch{};
    compose_campaign_configuration_436490({bank<0x57e4>(f, 0x5f3af4), bank<0x660>(f, 0x5f943c)}, 0,
                                          bank<0xec>(f, 0x5f26ac + model * 0xec), bank<300>(f, 0x5f3af4 + model * 300),
                                          scratch);
    publish_frontend_vehicle_configuration(selected, std::span<const std::byte, 300>{scratch.data() + 0xec, 300}, model,
                                           livery);
}
} // namespace
void clear_frontend_race_selection(FrontendLaunchWorld &w)
{
    // 43A72E clears60D730..60DF97.612BEC and trial availability/results lie
    // outside that interval and must survive. Options.texture's C++ default8
    // is not the source clear value.
    auto &o = w.options;
    const auto target = o.selection_phase;
    o = {};
    o.selection_phase = target;
    o.texture = 0;
    std::fill(w.selection.begin(), w.selection.end(), SelectionRecord{});
    w.trial.split_selection = 0;
    w.trial.previous_completed = 0;
    w.trial.track = {};
    w.trial.selected = {};
    std::fill(w.other_selected_vehicles.begin(), w.other_selected_vehicles.end(), 0);
    auto &s = w.state;
    s.setting_60d744 = s.track_index = s.prepared = s.catalog_count = 0;
    s.catalog_count = get(w.frontend.source_bytes(0x5f26a8, 4), 0);
    for (unsigned i = 0; i < 4; ++i)
        s.input_bindings[i] = {0xffffu, i + 1};
    s.setting_60d744 = get(w.frontend.source_bytes(0x5df5f0, 4), 0);
}
void bind_frontend_race_input(FrontendLaunchState &s, std::uint32_t controller, std::uint32_t vehicle,
                              std::uint32_t kind)
{
    if (controller < 1 || controller > 4)
        throw FormatError("frontend race controller outside physical1..4");
    s.input_bindings[controller - 1] = {vehicle, kind};
}
void publish_frontend_vehicle_configuration(SelectionRecord &s, std::span<const std::byte, 300> model,
                                            std::uint32_t identity, std::uint32_t livery)
{
    // 43A4A0's two source destinations are fields of this same SelectionRecord.
    // Offsets name model words in source publication order. Slot3E stays zero.
    // clang-format off: SelectionRecord offset table, 12 offsets per row in source publication order
    constexpr std::array<unsigned,64> offsets{
        0x04,0x08,0x0c,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0xb4,
        0x30,0x34,0x38,0x3c,0x44,0x40,0x48,0x4c,0x50,0x54,0x58,0x5c,
        0x60,0x64,0x68,0x6c,0x70,0x74,0x78,0x7c,0x80,0x84,0x88,0x90,
        0x94,0x98,0x9c,0xa0,0xac,0xa4,0xa8,0xb0,0xb4,0xb8,0xbc,0xc0,
        0xc4,0xc8,0xcc,0xd0,0xd4,0xd8,0xdc,0xe0,0xe4,0xe8,0xec,0xf0,
        0xf4,0xffffffffu,0xf8,0xfc};
    // clang-format on
    s.configuration = {};
    s.configuration.present = 1;
    for (unsigned i = 0; i < offsets.size(); ++i)
        if (offsets[i] != 0xffffffffu)
            s.configuration.tuning_words[i] = get(model, offsets[i]);
    s.tyre = get(model, 0x8c);
    s.model = identity;
    s.config = identity;
    s.livery = livery;
}
void select_campaign_race(FrontendLaunchWorld &w)
{
    auto &f = w.frontend;
    const auto roster = f.source_bytes(0x61bbe0, 0x1ec);
    const auto track = get(roster, 0x1d4), controller = get(f.source_bytes(0x5e37a4, 4), 0);
    // Host bounds guard. Valid source bank inputs are read in original order
    // below; these checks only reject source pointers outside represented owners.
    if (track > 24 || f.initials_scratch[3] != 0)
        throw FormatError("campaign launch track or initials outside bounded source domain");
    for (unsigned row = 0; row < 6; ++row)
    {
        const auto at = 0x130 + row * 0x1c;
        const auto grid = get(roster, at + 8), model = get(roster, at + 4);
        if (grid < 1 || grid > 6 || model >= 22)
            throw FormatError("campaign launch grid/model outside source catalog");
        if (get(roster, at - 4) != 0 && (controller < 1 || controller > 4))
            throw FormatError("campaign launch controller outside physical1..4");
    }
    f.race_return_selector = 5;
    f.bind_campaign_return(); // 5DF528/544/548
    // 436410 and43D420 are actual source no-ops.
    clear_frontend_race_selection(w);
    auto &o = w.options;
    o.render_b = 1;
    o.race_mode = 1;
    w.state.prepared = 1;
    o.record = get(roster, 0x1dc);
    o.game_type = 2;
    w.state.track_index = track - 1u;
    o.music = get(f.source_bytes(0x5fcb78 + track * 24, 4), 0);
    o.texture = get(f.source_bytes(0x5fcb8c + track * 24, 4), 0);
    o.record_f00c = get(roster, 0x1e8);
    o.laps = get(roster, 0x1d8);
    o.record_f008 = get(roster, 0x1e4);
    o.split = 0;
    o.demo_override = 0;
    o.record_f004 = get(roster, 0x1e0);
    o.start_delay = 4;
    for (unsigned row = 0; row < 6; ++row)
    {
        const auto at = 0x130 + row * 0x1c;
        const auto grid = get(roster, at + 8) - 1u;
        auto &selected = w.selection[grid];
        // 43D690 overwrites exactly three bytes and preserves the fourth NUL.
        std::copy_n(roster.begin() + at, 3, f.initials_scratch.begin());
        const auto zero = std::find(f.initials_scratch.begin(), f.initials_scratch.end(), 0);
        const auto length = static_cast<std::size_t>(zero - f.initials_scratch.begin()) + 1;
        std::memcpy(selected.name.data(), f.initials_scratch.data(), length);
        selected.kind = 2;
        const auto model = get(roster, at + 4), livery = get(roster, at + 0xc);
        if (get(roster, at - 4) == 0)
        {
            selected.control = 1;
            selected.ai_skill = get(roster, at + 0x14);
            selected.ai_mode = get(roster, at + 0x10);
            publish_stock_configuration(f, selected, model, livery);
        }
        else
        {
            bind_frontend_race_input(w.state, controller, grid, 1);
            w.trial.split_selection = grid;
            selected.control = 0;
            publish_frontend_vehicle_configuration(
                selected, std::as_bytes(std::span<const std::uint8_t, 300>{roster.data(), 300}), model, livery);
        }
    }
}
} // namespace sgn::retail
