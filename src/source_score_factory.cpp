#include "sgn/source_score_factory.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>
extern "C" void sgn_score_factory_add(float *, std::uint32_t);
extern "C" void sgn_score_factory_layout(void *, std::uint32_t);
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
static_assert(std::is_standard_layout_v<VehicleScorePopup> && sizeof(VehicleScorePopup) == 0x138);
static_assert(offsetof(VehicleScorePopup, text) == 0x38 && offsetof(VehicleScorePopup, dimensions) == 0xbc &&
              offsetof(VehicleScorePopup, presentation_size) == 0xe4);
W current(RaceSessionState &w, W site)
{
    if (w.current_slot >= 6)
        throw SourceAllocationError(site, "401220 selected vehicle outside source bank");
    return w.current_slot;
}
void put(VehicleScorePopup &p, W at, W value)
{
    std::memcpy(reinterpret_cast<std::uint8_t *>(&p) + at, &value, 4);
}
} // namespace
void create_source_score_popup_401220(FrontendState &frontend, SourceMenuSampleNative &samples, W category, W variant,
                                      W points)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    auto &w = platform->state();
    if (!platform->source_construction_only_entry() || &frontend.world() != &w)
        throw FormatError("401220 requires same source Platform");
    if (w.clock.phase != 2)
        return;
    const auto selected = current(w, 0x40123f);
    if (w.vehicles.physics[selected].energy_exempt == 1 || w.clock.game_mode == 4 ||
        (w.clock.game_mode == 3 && w.score_block != 0))
        return;
    sgn_score_factory_add(&w.vehicles.physics[selected].score, points); // 401280..401289 unsigned FILD64
    if (!contact::player::find_controller(w.controls, selected) || w.hud_block == 1)
        return;
    W index = 0;
    while (index < 16 && w.score_popups.popups[index].active == 1)
        ++index;
    if (index == 16)
        return;
    auto &p = w.score_popups.popups[index]; // source ESI remains this slot across audio callback
    const auto descriptor = 0x472030u + category * 28u;
    put(p, 0x14, 0);
    put(p, 0x18, 0);
    put(p, 0x1c, 0);
    put(p, 0x34, points);
    const auto app = platform->source_application_literals();
    SourceScriptPathCall format;
    const std::array<W, 1> arguments{points};
    auto output = std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(&p) + 0x38, sizeof(p) - 0x38);
    (void)format_race_lifecycle_46280c(w.boot_config, app, 0x47220c, arguments, output, 0x401303, format);
    // 4011C0: full raw text spill is visible before this synchronous sound call.
    const auto sound = std::bit_cast<std::int32_t>(category) <= 2 ? 47u : 46u;
    w.score_popups.sound_volume = 192;
    w.score_popups.sound_class = 6;
    w.score_popups.sound_id = sound;
    samples.start_461150(frontend, sound, 0, 0, 192, 255, 0, nullptr);
    if (variant > 3)
        return; // 401328 is AFTER score/format/sound writes
    const auto row = [&](W at, W site) { return app.read_score_factory_word(descriptor + at, site); };
    constexpr W sites[4][7] = {{0x401335, 0x401337, 0x401340, 0x401349, 0x401352, 0x40135b, 0x401364},
                               {0x4013e2, 0x4013e4, 0x4013ed, 0x4013f6, 0x4013ff, 0x401408, 0x401411},
                               {0x4014a5, 0x4014a7, 0x4014b0, 0x4014b9, 0x4014c2, 0x4014cb, 0x4014d4},
                               {0x401564, 0x401566, 0x40156f, 0x401578, 0x401581, 0x40158a, 0x401593}};
    const auto *pcs = sites[variant];
    auto a = row(0, pcs[0]), b = row(20, pcs[1]);
    put(p, 0xd4, a);
    a = row(24, pcs[2]);
    put(p, 0xbc, b);
    b = row(4, pcs[3]);
    put(p, 0xc0, a);
    a = row(8, pcs[4]);
    put(p, 0xc4, b);
    b = row(12, pcs[5]);
    put(p, 0xc8, a);
    a = row(16, pcs[6]);
    put(p, 0xcc, b);
    put(p, 0xd0, a);
    // 401373..401656 literal writes in source branch order; no whole-slot reset.
    if (variant == 0)
    {
        put(p, 0x10, 24);
        put(p, 0x40, 1);
        put(p, 0x2c, 60);
        put(p, 0x18, 0xc0a00000);
        put(p, 0x24, 0xbccccccd);
        put(p, 0x70, 0);
        put(p, 0x64, 0x41000000);
        put(p, 0x68, 0);
        put(p, 0x6c, 0x41700000);
        put(p, 0x44, 0x40000000);
        put(p, 0x48, 0x42700000);
        put(p, 0x4c, 0);
        put(p, 0x54, 0x437f0000);
        put(p, 0x58, 0x41700000);
        put(p, 0x5c, 0x41700000);
        put(p, 0x8c, 9);
        put(p, 0x88, 15);
        put(p, 0x90, 63);
        put(p, 0x94, 0);
        put(p, 0x98, 0);
    }
    else if (variant == 1)
    {
        put(p, 0x10, 25);
        put(p, 0x40, 1);
        put(p, 0x24, 0xbccccccd);
        put(p, 0x2c, 60);
        put(p, 0x18, 0xc0a00000);
        put(p, 0x70, 0);
        put(p, 0x64, 0xc1000000);
        put(p, 0x68, 0);
        put(p, 0x6c, 0x41700000);
        put(p, 0x44, 0x40000000);
        put(p, 0x48, 0x42700000);
        put(p, 0x4c, 0);
        put(p, 0x54, 0x437f0000);
        put(p, 0x58, 0x41700000);
        put(p, 0x5c, 0x41700000);
        put(p, 0x8c, 9);
        put(p, 0x88, 15);
        put(p, 0x90, 85);
        put(p, 0x94, 0);
        put(p, 0x98, 0);
    }
    else
    {
        put(p, 0x10, 57);
        put(p, 0x40, 1);
        put(p, 0x2c, 120);
        put(p, 0x24, 0x3ccccccd);
        put(p, 0x70, 0);
        put(p, 0x64, 0xc13c0000);
        put(p, 0x68, 0x42700000);
        put(p, 0x6c, 0);
        put(p, 0x44, 0x40000000);
        put(p, 0x48, 0x41f00000);
        put(p, 0x4c, 0);
        put(p, 0x54, 0x437f0000);
        put(p, 0x58, 0);
        put(p, 0x5c, 0x41700000);
        put(p, 0x8c, 9);
        put(p, 0x88, 16);
        put(p, 0x90, variant == 2 ? 63u : 127u);
        put(p, 0x94, 0);
        if (variant == 2)
            put(p, 0x98, 0);
        else
        {
            put(p, 0x74, 0x41f00000);
            put(p, 0x78, 0x41700000);
            put(p, 0x7c, 0);
            put(p, 0x80, 0xffffff);
            put(p, 0x84, 0xc0c0c0);
            put(p, 0x98, 1);
            put(p, 0x9c, 0);
            put(p, 0xa0, 0x41f00000);
            put(p, 0xa4, 0x41700000);
            put(p, 0xa8, 0x41700000);
        }
    }
    p.flags |= 32;
    put(p, 0x74, 0x40c00000);
    put(p, 0x78, 0x40c00000);
    W color{};
    bool has_color = true;
    switch (category)
    {
    case 0:
    case 3:
        color = 0xffa000;
        break;
    case 1:
    case 4:
        color = 0xe0f0ff;
        break;
    case 2:
    case 5:
        color = 0xffdc00;
        break;
    default:
        has_color = false;
        break;
    }
    if (has_color)
    {
        put(p, 0x7c, color);
        put(p, 0x80, color);
        put(p, 0x84, color);
    }
    const auto selected_after = w.current_slot; // 4016AC pointer load; no early stale identity capture
    p.active = 1;
    p.vehicle = selected_after;
    p.variant = variant;
    p.category = category;
    p.age = 0;
    p.presentation_lifetime = 72;
    p.presentation_flags = p.flags;
    p.presentation_x = 0;
    p.presentation_y = 0;
    put(p, 0xec, 0x3f800000);
    put(p, 0xf0, 0x3f800000);
    sgn_score_factory_layout(&p, category);
}
} // namespace sgn::retail
