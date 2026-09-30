#include "sgn/source_race_hud_draw.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/retail_quad_submission.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
extern "C" void sgn_hud_layout_x87(const float *, const float *, const HudElementLayout *, HudElementLayout *);
extern "C" W sgn_hud_displacement_x87(const float *, RaceHudLayout *, const unsigned *);
struct EnergyRatioArguments
{
    const float *energy;
    const float *capacity;
    const unsigned *selected;
    const float *threshold;
    float *ratio;
    W *captured_selected;
};
static_assert(sizeof(EnergyRatioArguments) == 48);
extern "C" W sgn_hud_energy_ratio_x87(const EnergyRatioArguments *);
extern "C" W sgn_hud_energy_phase_x87(const float *);
extern "C" W sgn_hud_energy_volume_x87(const float *, const float *);
extern "C" void sgn_hud_energy_draw_x87(W *, const float *, const HudElementLayout *);
extern "C" void sgn_hud_notice_first_x87(W *, W, W, float *);
extern "C" void sgn_hud_notice_second_x87(W *, const float *, W);
extern "C" void sgn_hud_finish_x87(W *, const W *, W);
extern "C" void sgn_hud_start_x87(W *, const W *, W);
extern "C" W sgn_hud_wrong_y_x87(const W *);
extern "C" W sgn_hud_notice_y_x87(W);
static_assert(sizeof(HudElementLayout) == 24 && sizeof(RaceHudLayout) == 628);
static_assert(offsetof(HudElementLayout, word_14) == 8 && offsetof(HudElementLayout, word_1c) == 16);
static_assert(offsetof(HudElementLayout, word_20) == 20);
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
class SourceHud final : public RaceHudSourceEnergy
{
    SourceCoreRaceDrawContext &c;
    std::shared_ptr<RaceSessionPlatform> platform;
    RaceSessionState &w;
    W controller(W at) const
    {
        const auto i = w.controls.current;
        if (i >= w.controls.controllers.size())
            throw SourceAllocationError(at, "HUD current controller outside source owner");
        return i;
    }
    W vehicle(W i, W at) const
    {
        if (i >= w.controls.controllers.size())
            throw SourceAllocationError(at, "HUD controller outside source owner");
        const auto slot = w.controls.controllers[i].vehicle;
        if (slot >= w.vehicles.physics.size())
            throw SourceAllocationError(at, "HUD vehicle outside source owner");
        return slot;
    }
    W current_vehicle(W at) const
    {
        return vehicle(controller(at), at);
    }
    W selected(W at) const
    {
        const auto i = w.selected_hud;
        if (i >= w.hud_layout.size())
            throw SourceAllocationError(at, "HUD selected identity outside source owner");
        return i;
    }
    W &d(unsigned offset)
    {
        return w.ui.descriptor[offset / 4];
    }
    void put(std::initializer_list<std::pair<unsigned, W>> values)
    {
        for (const auto &[at, value] : values)
            d(at) = value;
    }
    const W &startup(W slot, unsigned offset) const
    {
        return w.vehicles.startup[slot].fields_768c[(offset - 0x768c) / 4];
    }
    LoadingFrameWorld drawing()
    {
        auto frame = c.frames.borrow_loading_frame();
        if (&frame.submission != &c.submission || frame.quads.descriptor.data() != w.ui.descriptor.data() ||
            frame.quads.vertices.data() != c.submission.alpha_vertices.data() ||
            &frame.quads.selected_material != &w.ui.selected_material || &frame.quads.viewport != &w.viewport)
            throw FormatError("source HUD received foreign descriptor/vertex/material/viewport owners");
        return frame;
    }
    TextSubmission text_rendering(const LoadingFrameWorld &f)
    {
        return {w.ui.fonts,        w.ui.selected_material, c.submission.specular_enabled,
                f.quads.materials, w.view_quality.second,  w.view_quality.legacy_driver_restriction,
                w.viewport};
    }
    void quad()
    {
        auto f = drawing();
        submit_quad_432e00({w.ui.descriptor, f.quads.vertices, w.ui.selected_material, f.quads.materials, w.viewport},
                           c.sink);
    }
    // The actual6127A0 pointer table and60F6A0 arena. A null pointer is an
    // empty borrowed span, NOT an empty string: the existing reader rejects
    // its first reached byte, while a vertically clipped call never reads it.
    std::span<const std::uint8_t> game_text(W id)
    {
        const auto offset = w.game_text.offsets.at(id);
        if (!offset)
            return {};
        if (*offset >= w.game_text.arena.size())
            throw SourceAllocationError(0x4291f0, "HUD text pointer outside actual arena");
        return {reinterpret_cast<const std::uint8_t *>(w.game_text.arena.data() + *offset),
                w.game_text.arena.size() - *offset};
    }
    void text(std::span<const std::uint8_t> bytes, TextGlyphParameters p)
    {
        auto f = drawing();
        submit_text_4291f0(text_rendering(f), bytes, 1, p, c.sink);
    }
    void finite(RaceHudFiniteLeaf leaf)
    {
        auto f = drawing();
        submit_race_hud_finite_leaf({{w.controls, w.hud_countdown, w.hud_layout, w.selected_hud, w.hud_margins},
                                     w.vehicles,
                                     w.clock,
                                     w.records,
                                     w.ranking,
                                     w.cars,
                                     w.game_text,
                                     w.split_setting,
                                     w.frame_pause,
                                     w.ui.stars,
                                     w.ui.descriptor,
                                     f.quads.vertices,
                                     text_rendering(f)},
                                    leaf, *this, c.sink);
    }
    void layout()
    {
        const auto i = selected(0x447d80);
        auto &h = w.hud_layout[i];
        // ECX retains this HUD throughout447D80. No full-layout copy or early
        // numeric preflight. Each helper reads/writes the live source words.
        for (const W e : {1u, 2u, 7u, 3u, 4u, 5u, 6u})
            sgn_hud_layout_x87(&h.value[e - 1], &w.hud_countdown[i].value, &h.elements[e], &h.elements[e - 1]);
    }
    void displacement()
    {
        const auto slot = current_vehicle(0x44ac99);
        const auto &p = w.vehicles.physics[slot].auxiliary_displacement;
        if (sgn_hud_displacement_x87(&p.x, w.hud_layout.data(), &w.selected_hud) == 0)
            throw SourceAllocationError(0x44ad0a, "HUD selected identity outside source owner");
    }
    void wrong_way()
    {
        if (w.frame_pause == 1)
            return;
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
        const auto ticks = w.vehicles.race[current_vehicle(0x449862)].event7678;
        if (ticks < 30)
            return;
        const auto remainder = ticks % 60u;
        const auto distance = remainder < 30 ? 30u - remainder : remainder - 30u;
        const auto alpha = std::min((distance * 510u) / 60u, 255u);
        put({{0, 2},
             {4, 0x43873333},
             {8, 0x432c0000},
             {12, 0x3f7ae148},
             {24, 0x44030000},
             {28, 0x41800000},
             {32, 0x447f0000},
             {36, 0x43b00000},
             {16, 0x42c66667},
             {20, 0x42866667},
             {40, 13},
             {48, alpha},
             {44, 0x48}});
        quad(); // 449939, only then select font and read the text pointer
        const auto color = ((0u - W(alpha < 127u)) & 0xff404100u) - 0x100u;
        select_retail_font_428f70(w.ui.fonts, 1);
        const auto y = sgn_hud_wrong_y_x87(&d(8));
        const auto bytes = game_text(7);
        text(bytes, {320, as_float(y), as_float(0x3f7d70a4), color, 2, 6});
        select_retail_font_428f70(w.ui.fonts, 0); // no scope-exit restore on failure
    }
    void waiting()
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
        const auto slot = current_vehicle(0x449ce5);
        if (startup(slot, 0x76ec) != 1)
            return;
        if (startup(vehicle(0, 0x449cf6), 0x76a4) != 0)
            return;
        const auto phase = startup(slot, 0x76ac);
        const auto color = ((0u - W(phase > 30u)) & 0xff404100u) - 0x100u;
        select_retail_font_428f70(w.ui.fonts, 1);
        const auto bytes = game_text(16);
        text(bytes, {320, 240, as_float(0x3f733333), color, 2, 0});
        select_retail_font_428f70(w.ui.fonts, 0);
    }
    void notice_panel()
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
        const auto slot = current_vehicle(0x4499ab);
        if (w.vehicles.race[slot].notice_active != 1)
            return;
        if (w.source_overlay_selection_61b634 != 20)
            return;
        const auto ticks = w.vehicles.race[slot].notice_ticks;
        const W alpha = ticks < 30 ? (ticks * 224u) / 30u : ticks <= 90 ? 224u : (0x6900u - ticks * 224u) / 30u;
        float stored_motion; // 449A8D writes before the first quad
        sgn_hud_notice_first_x87(w.ui.descriptor.data(), ticks, alpha, &stored_motion);
        quad(); // 449AB6; its exception retains first panel and motion store
        sgn_hud_notice_second_x87(w.ui.descriptor.data(), &stored_motion, alpha);
        quad();
    }
    void finish()
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
        const auto slot = current_vehicle(0x449d75);
        if (startup(slot, 0x76b8) == 0)
            return;
        const auto image = w.source_overlay_selection_61b634;
        if (image < 21 || image > 23)
            return;
        const auto ticks = startup(slot, 0x76bc);
        const auto numerator = ticks < 180 ? ticks * 255u : 0xef10u - ticks * 255u;
        const auto alpha = std::min(numerator / 60u, 255u);
        sgn_hud_finish_x87(w.ui.descriptor.data(), &startup(slot, 0x76c0), alpha);
        quad();
    }
    void start()
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
        const auto slot = current_vehicle(0x44a3d5);
        if (startup(slot, 0x768c) == 0)
            return;
        const auto alpha = std::min((startup(slot, 0x7690) * 240u) / 60u, 240u);
        sgn_hud_start_x87(w.ui.descriptor.data(), &startup(slot, 0x7694), alpha);
        quad();
    }
    void notice_text()
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::bottom_center); // NEW HOST (wide_hud R20)
        if (w.frame_pause == 1)
            return;
        const W limit = static_cast<W>(w.hud_margins.y) + 40u; // unsignedLEA, before controller
        auto &notices = w.controls.controllers[controller(0x442e4b)].notices;
        for (auto &notice : notices)
        {
            const W remaining = static_cast<W>(notice.remaining);
            if (remaining == 0 || remaining == static_cast<W>(notice.initial))
                continue;
            W animation = remaining < limit ? remaining : std::min(static_cast<W>(notice.initial) - remaining, limit);
            if (w.split_setting != 0)
                animation = limit;
            const auto color = ((0u - W(30u < remaining % 60u)) & 0xffff4140u) + 0xffff00u;
            const auto y = sgn_hud_notice_y_x87(animation);
            text({reinterpret_cast<const std::uint8_t *>(notice.text.data()), notice.text.size()},
                 {320, as_float(y), as_float(0x3f70a3d7), color, 1.75f, 6});
            return; // only the first eligible actual controller record
        }
    }

  public:
    explicit SourceHud(SourceCoreRaceDrawContext &context)
        : c(context), platform(c.frontend.shared_platform()), w(c.frontend.world())
    {
        if (!platform || !platform->source_construction_only_entry() || &platform->state() != &w)
            throw FormatError("source HUD requires its actual platform/frontend world");
        platform->require_owner_thread();
        c.frames.require_source_frontend(c.frontend);
    }
    void energy_4493e0() override
    {
        const auto slot = current_vehicle(0x4493ea);
        const auto &car = w.vehicles.physics[slot];
        const auto &parameters = w.vehicles.parameters[slot];
        float ratio;
        W captured_selected;
        const EnergyRatioArguments args{&car.energy,     &car.energy_capacity,
                                        &w.selected_hud, &parameters.turbo_energy_threshold,
                                        &ratio,          &captured_selected};
        const auto branch = sgn_hud_energy_ratio_x87(&args);
        if (branch == 0)
            return; // C3 includes NaN, no selected-HUD access
        if (captured_selected >= w.hud_layout.size())
            throw SourceAllocationError(0x449413, "HUD selected identity outside source owner");
        auto &h = w.hud_layout[captured_selected];
        if (branch == 1)
        {
            auto &phase = h.elements[17].word_1c;
            phase -= 1u; // 449436 store survives every following failure
            if (as_signed(phase) <= 0)
            {
                phase = sgn_hud_energy_phase_x87(&ratio);
                if (as_signed(phase) < 24)
                    phase = 24;
                return;
            }
            if (car.energy_exempt == 0 && as_signed(phase) < 16)
            {
                if (phase != 1 || w.frame_pause != 0)
                    return;
                const auto volume = sgn_hud_energy_volume_x87(&parameters.turbo_energy_threshold, &ratio);
                SourceMenuSampleNative samples(platform); // inert borrower, no device construction
                samples.start_461150(c.frontend, 22, 0, 0, volume, 0, 1, nullptr); // 4494B9
                return;
            }
        }
        sgn_hud_energy_draw_x87(w.ui.descriptor.data(), &ratio, &h.elements[0]);
        quad();
    }
    void countdown()
    {
        host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::center); // NEW HOST (wide_hud R20)
        if (w.frame_pause == 1)
            return;
        const auto ticks = w.vehicles.race[vehicle(0, 0x449b5d)].start_countdown;
        if (ticks == 0 || ticks >= 240)
            return;
        const auto image = w.source_overlay_selection_61b634;
        if (image < 16 || image > 19)
            return;
        W alpha = 192;
        if ((ticks <= 30 || ticks >= 120) && ticks % 60u < 30u)
            alpha = ((ticks % 60u) * 192u) / 30u;
        put({{0, 2},
             {4, 0x4359999a},
             {8, 0x42933333},
             {12, 0x3f7ae148},
             {24, 0},
             {28, 0},
             {32, 0x44800000},
             {36, 0x44800000},
             {16, 0x434ccccd},
             {20, 0x434ccccd},
             {40, 15},
             {72, 0},
             {44, 0x48},
             {48, alpha}});
        quad();
    }
    void run()
    {
        w.selected_hud = controller(0x44ae40); // 44AE50, before descriptor reset
        for (auto &word : w.ui.descriptor)
            word = 0; // 35DWORD,44AE5C
        layout();
        if (w.hud_countdown[selected(0x44ae63)].active == 0)
        {
            countdown();
            return;
        }
        displacement();
        using L = RaceHudFiniteLeaf;
        switch (w.clock.game_mode)
        { // 44AE79, capture after displacement
        case 1:
            finite(L::position);
            finite(L::timers);
            finite(L::laps);
            finite(L::gauge);
            finite(L::score);
            finite(L::progress);
            finite(L::gear);
            break;
        case 2:
            finite(L::position);
            if (w.split_setting == 0)
            {
                finite(L::timers);
                finite(L::laps);
                finite(L::gauge);
                finite(L::score);
                finite(L::progress);
                finite(L::gear);
                wrong_way();
            }
            else
            {
                finite(L::split_gauge);
                finite(L::gear);
                wrong_way();
            }
            waiting();
            break;
        case 3:
            finite(L::remaining);
            finite(L::gauge);
            finite(L::score);
            finite(L::gear);
            waiting();
            break;
        case 4:
            finite(L::timers);
            finite(L::gauge);
            finite(L::gear);
            wrong_way();
            break;
        default:
            break; // the actual unsigned switch reaches the suffix
        }
        notice_panel();
        if (w.split_setting == 0)
            finish();
        start();
        notice_text();
    }
};
} // namespace
void submit_source_race_hud_44ae40(SourceCoreRaceDrawContext &c)
{
    SourceHud(c).run();
}
void submit_source_race_countdown_449b50(SourceCoreRaceDrawContext &c)
{
    SourceHud(c).countdown();
}
} // namespace sgn::retail
