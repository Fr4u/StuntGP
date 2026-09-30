#include "sgn/retail_results_submission.hpp"
#include "sgn/retail_font_selection.hpp"
#include "sgn/retail_quad_submission.hpp"
#include "sgn/race_session_lifecycle.hpp"
#include "retail_results_literals.hpp"
#include "sgn/host_wide_race_anchors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::format_check::require;
double finite(float value)
{
    require(std::isfinite(value), "results nonfinite source float outside candidate domain");
    return double(value);
}
float store(double value)
{
    const float result = static_cast<float>(value);
    require(std::isfinite(value) && std::isfinite(result), "results arithmetic overflow outside candidate domain");
    return result;
}
std::uint32_t word(double value)
{
    return std::bit_cast<std::uint32_t>(store(value));
}
double k(unsigned address)
{
    return results_source::scalar(address);
}
unsigned selected(RaceHudView hud)
{
    require(hud.selected_hud < 4, "results selected HUD identity");
    return hud.selected_hud;
}

// Canonical source offset translation, not a packed reinterpretation or an
// extra HUD buffer. HUD+10 begins source36-byte elements; computed coordinates
// alias the preceding element's word_1c/word_20. Existing hud_element resolves
// countdown0 and respawn6 ownership separately.
std::uint32_t hud_word(RaceHudView hud, unsigned identity, unsigned offset)
{
    require(offset >= 0x10 && offset % 4 == 0, "results HUD source offset");
    const auto relative = offset - 0x10;
    auto e = hud_element(hud, identity, relative / 0x24);
    switch (relative % 0x24)
    {
    case 0:
        return std::bit_cast<std::uint32_t>(e.layout.x);
    case 4:
        return std::bit_cast<std::uint32_t>(e.layout.y);
    case 8:
        return e.active;
    case 12:
        return std::bit_cast<std::uint32_t>(e.value);
    case 16:
        return e.phase;
    case 20:
        return e.layout.word_14;
    case 24:
        return e.layout.word_18;
    case 28:
        return e.layout.word_1c;
    case 32:
        return e.layout.word_20;
    default:
        throw FormatError("results HUD field boundary");
    }
}
double hud_real(RaceHudView hud, unsigned identity, unsigned offset)
{
    return finite(as_float(hud_word(hud, identity, offset)));
}
void hud_store(RaceHudView hud, unsigned identity, unsigned offset, double value)
{
    require(offset >= 0x10 && offset % 4 == 0, "results HUD store offset");
    const auto r = offset - 0x10;
    auto e = hud_element(hud, identity, r / 0x24);
    const auto field = r % 0x24;
    require(field == 28 || field == 32, "results store must alias canonical computed word");
    if (field == 28)
        e.layout.word_1c = word(value);
    else
        e.layout.word_20 = word(value);
}
unsigned controller_vehicle(const ResultsWorld &w)
{
    require(w.hud.controls.current < 4, "results current controller identity");
    const auto v = w.hud.controls.controllers[w.hud.controls.current].vehicle;
    require(v < 6, "results controller vehicle identity");
    return v;
}
std::uint32_t startup(const ResultsWorld &w, unsigned vehicle, unsigned offset)
{
    require(vehicle < 6 && offset >= 0x768c && offset <= 0x76ec && offset % 4 == 0, "results startup field identity");
    return w.vehicles.startup[vehicle].fields_768c[(offset - 0x768c) / 4];
}
std::span<const std::uint8_t> game_text(const ResultsWorld &w, unsigned id)
{
    require(id < w.game_text.offsets.size(), "results game text pointer table outside owner");
    if (!w.game_text.offsets[id] || *w.game_text.offsets[id] >= w.game_text.arena.size())
        return {};
    const auto at = *w.game_text.offsets[id];
    return {reinterpret_cast<const std::uint8_t *>(w.game_text.arena.data() + at), w.game_text.arena.size() - at};
}
std::span<const std::uint8_t> row_text(const ResultsWorld &w, unsigned row, unsigned offset, std::uint32_t site)
{
    require(row < 6 && offset <= 48 && offset % 16 == 0, "results row/string identity");
    // Reload the actual selected4856C0/4856D8/4856F0/485708 pointer at
    // each source use, including after synchronous box/text submissions.
    const auto pointer = w.results.result_rows.words[(offset / 16u) * 6u + row];
    if (pointer - 0x4857b8u < 216u)
        return w.drawing.borrow_result_initial_text(pointer, site);
    const auto relative = pointer - 0x60f050u;
    require(relative < 600u, "results text pointer has no App/car source owner");
    const auto slot = relative / 100u, within = relative % 100u;
    const auto &name = w.cars[slot].name; // 64 owned bytes, source car stride100
    require(within < name.size(), "results text pointer is outside the owned car name/result bytes");
    return {reinterpret_cast<const std::uint8_t *>(name.data() + within), name.size() - within};
}

double measure(ResultsWorld &w, std::span<const std::uint8_t> text, float scale)
{
    auto rendering = w.drawing.borrow_text();
    return measure_text_4290d0(rendering.fonts, text, scale);
}
void text(ResultsWorld &w, std::span<const std::uint8_t> bytes, std::uint32_t align, float x, float y,
          std::uint32_t color, float scale)
{
    auto rendering = w.drawing.borrow_text();
    submit_text_4291f0(rendering, bytes, align, {x, y, as_float(0x3f7d70a4), color, scale, 6}, w.sink);
}
void font(ResultsWorld &w, unsigned request)
{
    auto rendering = w.drawing.borrow_text();
    select_retail_font_428f70(rendering.fonts, request);
}
void quad(ResultsWorld &w)
{
    auto rendering = w.drawing.borrow_quad();
    require(rendering.descriptor.data() == w.descriptor.data(), "results quad descriptor alias differs");
    submit_quad_432e00(rendering, w.sink);
}
} // namespace

void prepare_result_layout_447fa0(RaceHudView hud)
{
    const auto identity = selected(hud); // 447FA0 cached EDX through whole child
    const auto factor = [&](unsigned input) {
        const double value = hud_real(hud, identity, input) + hud_real(hud, identity, 0x13c);
        return value > k(0x46e2f0) ? k(0x46e2f0) : value; // upper clamp only
    };
    const auto component = [&](unsigned output, unsigned signed_input, unsigned base, double f) {
        const double integer = std::bit_cast<std::int32_t>(hud_word(hud, identity, signed_input));
        hud_store(hud, identity, output, ((integer * f) * k(0x46e294)) + hud_real(hud, identity, base));
    };
    // 447FAC..447FF8, exact six writes pairs and source stride24hex.
    for (unsigned row = 0; row < 6; ++row)
    {
        const auto at = 0x1b8 + row * 0x24;
        const auto f = factor(at + 0x14);
        component(at, at + 0x1c, at + 8, f);
        component(at + 4, at + 0x20, at + 12, f);
    }
    auto f = factor(0x1a8); // 447FFA..448050, deliberately element11 factor for heading9
    component(0x14c, 0x168, 0x154, f);
    component(0x150, 0x16c, 0x158, f);
    f = factor(0x1a8); // 448052..4480A7
    component(0x194, 0x1b0, 0x19c, f);
    component(0x198, 0x1b4, 0x1a0, f);
    f = factor(0x184); // 4480A9..4480FE
    component(0x170, 0x18c, 0x178, f);
    component(0x174, 0x190, 0x17c, f);
}

void submit_result_box_4476c0(ResultsWorld &w, ResultBoxParameters p)
{
    // 4476C4..4476E7: flag priority40,20,10,default; low2 before low4.
    const unsigned table = (p.flags & 0x40) ? 3u : (p.flags & 0x20) ? 2u : (p.flags & 0x10) ? 1u : 0u;
    const auto &uv = results_source::box_uv[table];
    float width = store(finite(p.width) + k(0x46e568)); // 4476F8 argument stack store
    float offset = (p.flags & 2)   ? store(finite(width) * k(0x46e4cc))
                   : (p.flags & 4) ? store(-(finite(width) - k(0x46ead8)))
                                   : as_float(0xc1900000);
    auto &d = w.descriptor;
    // 447727..44779B: retained partial descriptor writes in source order.
    d[3] = std::bit_cast<std::uint32_t>(p.depth);
    d[5] = std::bit_cast<std::uint32_t>(p.height);
    d[7] = uv[0];
    d[9] = uv[1];
    d[2] = std::bit_cast<std::uint32_t>(p.y);
    d[1] = word(finite(offset) + finite(p.x));
    d[0] = 2;
    d[10] = 15;
    d[11] = 0x20;
    d[6] = uv[2];
    d[8] = uv[3];
    d[4] = 0x41900000;
    quad(w);
    // 4477A0..4477E9. FST stores width but x87 retains its unrounded value
    // until the descriptor width FSTP; both destination stores are binary32.
    const auto advanced = finite(offset) + k(0x46ead8);
    d[2] = std::bit_cast<std::uint32_t>(p.y);
    d[6] = uv[3];
    offset = store(advanced);
    const auto middle = finite(width) - k(0x46e568);
    d[8] = uv[4];
    width = store(middle);
    d[1] = word(finite(offset) + finite(p.x));
    d[4] = word(middle);
    quad(w);
    // 4477EE..447826: source adds width then offset then x, not reassociated.
    const double right = finite(width) + finite(offset);
    d[2] = std::bit_cast<std::uint32_t>(p.y);
    d[6] = uv[4];
    d[8] = uv[5];
    d[4] = 0x41900000;
    d[1] = word(right + finite(p.x));
    quad(w);
}

void submit_race_results_449fb0(ResultsWorld &w)
{
    // NEW HOST (wide_hud R20): results table, heading/title/prompt and 4487D0
    // position glyphs stay one centered 4:3 composition.
    host_wide_overlay::AnchorScope anchor(host_wide_overlay::race::panel);
    const unsigned vehicle = controller_vehicle(w); // cached EDI/-2C through whole parent
    if (startup(w, vehicle, 0x76a4) == 0)
        return; // 449FC3..449FD4
    if (w.results.field_60d728 < 2)
    {
        ++w.results.field_60d728;
        return;
    } // 449FDA..449FF3
    if (startup(w, w.current_vehicle, 0x76b4) > 120)
        return; // 449FF4..44A001
    if (w.textures.selected_overlay_texture() != 27)
        w.textures.replace_source_texture(15, 27); // 44A007..44A01A
    unsigned title_id{}, string_offset{};
    float row_width{};
    bool selectors_known = true;
    // 44A01F..44A05E reads cached vehicle AFTER upload. The invalid selector
    // guard is deferred until the first reached local read; prefix children persist.
    switch (startup(w, vehicle, 0x76d0))
    {
    case 0:
        title_id = 3;
        string_offset = 16;
        row_width = as_float(0x428ccccd);
        break;
    case 1:
        title_id = 2;
        string_offset = 32;
        row_width = as_float(0x428ccccd);
        break;
    case 2:
        title_id = 4;
        string_offset = 48;
        row_width = as_float(0x42b33333);
        break;
    default:
        selectors_known = false;
        break; // no read yet: layout/font/glyph still execute
    }
    prepare_result_layout_447fa0(w.hud);      // 44A066
    const unsigned row_hud = selected(w.hud); // 44A06B cached row-coordinate pointer
    float y_offset = 0;                       // 44A076 stored float bits0
    font(w, 1);                               // 44A086
    float motion_factor = as_float(0x40a00000);
    const auto placement = w.vehicles.race[controller_vehicle(w)].finish_state; // 44A08B..A0A5 fresh current
    // NEW HOST (widescreen R37): the rows slide by +640+64*i units (4478C0 ranking init) and their
    // result pills mirror that path (44A0FF: 521.6 - (x - base)), so on a wide canvas a parked row
    // (value 100) left its star at the right screen edge and its pill at the left edge, visible over
    // the black screen after the results. The drawn path is lengthened by the visible extension on
    // the side it moves to: x = base + dx*t is drawn at base + (dx + E)*t (t = (x-base)/dx, the
    // source animation fraction), so rest positions (t = 0) are unchanged and every parked or
    // departing row starts/ends beyond the real screen edge. Host drawing only: the HUD words
    // 447FA0 wrote are not changed. Identity (E = 0) on 4:3 / original_4x3.
    double slide_right = 0.0, slide_left = 0.0;
    {
        const auto frame = w.drawing.borrow_quad();
        slide_right = host_wide_overlay::side_extension(frame.viewport, host_wide_overlay::Horizontal::right);
        slide_left = host_wide_overlay::side_extension(frame.viewport, host_wide_overlay::Horizontal::left);
    }
    for (unsigned i = 0; i < 6; ++i)
    {
        if (!w.ranking.slots[i])
            break;                             // 44A0B3..44A0BC checks only null, not pointed vehicle
        const auto live_hud = selected(w.hud); // 44A0C2 cached ECX for pre-draw math
        std::uint32_t color = 0xffffff00;
        if (i == placement && startup(w, vehicle, 0x76ac) > 30 &&
            hud_word(w.hud, live_hud, 0x1c8 + placement * 0x24) == 0)
            color = 0xff404000; // 44A0C8..44A0EA
        const auto at = 0x1b8 + i * 0x24;
        // NEW HOST (widescreen R37): host slide lengthening (see above); 0 keeps the source floats.
        double slide = 0.0;
        if (slide_right > 0.0 || slide_left > 0.0)
        {
            const auto dx = std::bit_cast<std::int32_t>(hud_word(w.hud, row_hud, at + 0x1c));
            if (dx != 0)
            {
                const double t = (hud_real(w.hud, row_hud, at) - hud_real(w.hud, row_hud, at + 8)) / double(dx);
                const double reach = dx > 0 ? slide_right : -slide_left;
                if (std::isfinite(t) && std::abs(t) <= 4.0)
                    slide = t * reach;
            }
        }
        const float x = slide != 0.0 ? store(hud_real(w.hud, row_hud, at) + slide)
                                     : as_float(hud_word(w.hud, row_hud, at));
        const float y = store(finite(y_offset) + hud_real(w.hud, row_hud, at + 4));
        float result_x =
            slide != 0.0
                ? store(k(0x46eb7c) - ((hud_real(w.hud, row_hud, at) + slide) - hud_real(w.hud, row_hud, at + 8)))
                : store(k(0x46eb7c) - (hud_real(w.hud, row_hud, at) - hud_real(w.hud, row_hud, at + 8)));
        if (startup(w, vehicle, 0x76cc) != 0)
        {
            motion_factor = store(finite(motion_factor) + k(0x46e1d8));
            result_x =
                store(((hud_real(w.hud, live_hud, 0x170) - hud_real(w.hud, live_hud, 0x178)) * finite(motion_factor)) +
                      finite(result_x));
        }
        {
            const float glyph_y = store(finite(y) - k(0x46eb78));
            const float glyph_x = store(finite(x) - k(0x46eb74));
            w.drawing.submit_position_4487d0(w.sink, i, glyph_x, glyph_y, 0x3f7d70a4); // 44A152, existing child
        }
        const float box_y = store(finite(y) - k(0x46e20c));
        if (!selectors_known)
            throw FormatError("44A160 results reads uninitialized selector width after glyph");
        submit_result_box_4476c0(
            w, {result_x, box_y, as_float(0x3f75c28f), row_width, as_float(0x420ccccd), 0xa});         // 44A17E
        const float name_width = store(measure(w, row_text(w, i, 0, 0x44a183), as_float(0x3fcccccd))); // 44A18F..44A194
        submit_result_box_4476c0(w, {x, box_y, as_float(0x3f75c28f), name_width, as_float(0x420ccccd), 9}); // 44A1AF
        text(w, row_text(w, i, 0, 0x44a1c7), 0, x, y, color, as_float(0x3fcccccd));                         // 44A1CF
        text(w, row_text(w, i, string_offset, 0x44a1f1), 1, result_x, y, color, as_float(0x3fcccccd));      // 44A1F4
        y_offset = store(finite(y_offset) + k(0x46e438)); // 44A1F9..44A20F, stored per row
    }
    {
        const auto h = selected(w.hud);
        // NEW HOST (widescreen R37): the RACE RESULTS heading is a tab whose flat left cap lies
        // beyond the left screen edge (box x = 12.8-16-18). It follows the real left edge; the
        // rows below stay the centred 4:3 composition.
        host_wide_overlay::AnchorScope tab(host_wide_overlay::Anchor::at(host_wide_overlay::Horizontal::left,
                                                                         host_wide_overlay::Vertical::top));
        const auto bytes = game_text(w, 1); // 44A218..44A239
        const float x = as_float(hud_word(w.hud, h, 0x14c)), y = as_float(hud_word(w.hud, h, 0x150));
        const float width = store(measure(w, bytes, as_float(0x3fcccccd)) + k(0x46e49c));
        submit_result_box_4476c0(w, {store(finite(x) - k(0x46e49c)), store(finite(y) - k(0x46e43c)),
                                     as_float(0x3f75c28f), width, as_float(0x424ccccd), 0x21}); // 44A277
        text(w, bytes, 0, x, y, 0xffffff00, as_float(0x3fcccccd));                              // 44A299
    }
    {
        const auto h = selected(w.hud); // 44A29E
        // NEW HOST (widescreen R37): the title tab (RACE TIMES / LAP TIMES / AEROMILES) is
        // right-aligned at 624 with its flat right cap ending on the right screen edge; it follows
        // the real right edge.
        host_wide_overlay::AnchorScope tab(host_wide_overlay::Anchor::at(host_wide_overlay::Horizontal::right,
                                                                         host_wide_overlay::Vertical::top));
        const float x = as_float(hud_word(w.hud, h, 0x170)), y = as_float(hud_word(w.hud, h, 0x174));
        if (!selectors_known)
            throw FormatError("44A2B4 results reads uninitialized title selector after heading");
        const auto bytes = game_text(w, title_id);
        const float width = store(measure(w, bytes, as_float(0x3fcccccd)));
        submit_result_box_4476c0(
            w, {x, store(finite(y) - k(0x46e43c)), as_float(0x3f75c28f), width, as_float(0x424ccccd), 0x44}); // 44A2F2
        text(w, bytes, 2, x, y, 0xffffff00, as_float(0x3fcccccd));                                            // 44A314
    }
    font(w, 0); // 44A31B, not an exception cleanup or early-return postcondition
    std::uint32_t color = 0xffffff00;
    if (startup(w, vehicle, 0x76ac) > 30 && hud_word(w.hud, selected(w.hud), 0x1a4) == 0)
        color = 0xff404000;
    const auto prompt = game_text(w, w.clock.phase == 2 ? 6u : 95u); // 443770 getter,44A344..44A354
    const float width = store(measure(w, prompt, 1));
    {
        const auto h = selected(w.hud);
        const float x = as_float(hud_word(w.hud, h, 0x194));
        const float y = store(hud_real(w.hud, h, 0x198) - k(0x46e298));
        submit_result_box_4476c0(w, {x, y, as_float(0x3f75c28f), width, as_float(0x41d33333), 0x12}); // 44A398
    }
    const auto h = selected(w.hud); // 44A39D rereads after box child
    text(w, prompt, 1, as_float(hud_word(w.hud, h, 0x194)), as_float(hud_word(w.hud, h, 0x198)), color, 1); // 44A3C4
}
} // namespace sgn::retail
