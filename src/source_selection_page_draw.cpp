#include "sgn/source_selection_page_draw.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/source_selection_pages.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
namespace sgn::retail
{
extern "C" void sgn_selection_player_x(const std::uint32_t *, std::uint32_t *);
extern "C" void sgn_selection_frame_xy(const std::uint32_t *, std::uint32_t *);
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
W st(double v)
{
    return std::bit_cast<W>(vehicle_f32(v));
}
// 45CE0C/46/5F also read the existing BYTE-owned persistence bank.
// Value reads must borrow it; source_word cannot invent a typed lvalue there.
W rd(FrontendState &f, W a)
{
    return persistence_word(f.source.source_bytes(a, 4), 0);
}
using sgn::retail::frontend_page_words::page_word;
W nearest_index(W v)
{
    const double a = double(as_float(v)) + 0.5;
    if (!std::isfinite(a) || a < -2147483648.0 || a >= 2147483648.0)
        throw FormatError("selection rounded index outside admitted signed32 FTOL domain");
    return std::bit_cast<W>(static_cast<std::int32_t>(a));
}
double fraction(W v)
{
    const auto integer = static_cast<W>(frontend_ftol_f32_462860(v));
    double a = double(as_float(v)) - double(as_signed(integer));
    if (!(a < 0.5))
        a -= 1.0;
    return a;
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
void text(FrontendState &f, W x, W y, W style, std::span<const std::uint8_t> bytes, W scale, W color, W flags,
          W advance = 0)
{
    if (advance != 0)
        f.world().ui.fonts.metrics().digit_advance_bits = advance; // 438A20 ->4291A0
    queue_frontend_text_438990(f.source, 12, 0, x, y, style, bytes, scale, color, flags, advance);
}
void label(FrontendState &f, W x, W y, W id, W scale, W color, W flags, W style = 4)
{
    queue_frontend_text_438990(f.source, 12, 1, x, y, style, language(f, id), scale, color, flags, 0);
}
std::span<const std::uint8_t> page_string(FrontendState &f, W p, W offset, W extent)
{
    const auto b = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), p, extent);
    return {reinterpret_cast<const std::uint8_t *>(b.data() + offset), b.size() - offset};
}
auto number(FrontendState &f, W v)
{
    const auto b = format_campaign_value_43d5f0(v, std::as_writable_bytes(std::span{f.source.numeric_scratch}));
    return std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(b.data()), b.size()};
}
auto initials(FrontendState &f, W a)
{
    // Pinned479668 points to47966C. Copy only the three source bytes, retain fourth.
    const auto s = f.source.source_bytes(a, 3);
    auto &d = f.source.initials_scratch;
    const auto lo = s[0], hi = s[1];
    d[0] = lo;
    d[1] = hi;
    d[2] = s[2];
    return std::span<const std::uint8_t>{d};
}
auto time_text(FrontendState &f, W ticks)
{
    const auto hundredths = as_signed(ticks * 100u) / 60;
    const std::array<W, 3> args{W((hundredths / 6000) % 60), W((hundredths / 100) % 60), W(hundredths % 100)};
    // 479654 bytes are pinned in PE-EVIDENCE.json, including punctuation.
    constexpr std::array<std::uint8_t, 13> fmt{'%', 'd', ':', '%', '0', '2', 'd', ':', '%', '0', '2', 'd', 0};
    SourceScriptPathCall call;
    auto platform = f.shared_platform();
    format_selection_numbers_46280c(platform->state().boot_config, platform->source_application_literals(), fmt, args,
                                    f.source.selection_time_scratch, 0x43d5be, call);
    return std::span<const std::uint8_t>{f.source.selection_time_scratch}.first(call.initialized_prefix);
}
void bar_438230(FrontendState &f, W descriptor, W x, W y, W width, SourceFrontendDrawServices &calls)
{
    // Actual451FD0 caller: ECX12, flags2. Other438230 call contracts are not exposed.
    if (descriptor == 0 || !(as_float(width) >= 0))
        return;
    if (descriptor >= 387)
        throw FormatError("438230 descriptor outside existing atlas");
    std::array<W, 12> d;
    for (W i = 0; i < 12; ++i)
        d[i] = f.source.frontend_descriptors[descriptor * 12u + i];
    d[0] = 2;
    d[11] = 255;
    d[10] = 0x20;
    d[1] = x;
    d[2] = st(double(as_float(y)) - double(as_float(d[4])) * 0.5);
    const auto half = st(double(as_float(d[3])) * 0.5), right = d[7];
    // 46E20C =4.0. Keep the source half-width FSTP32 before UV arithmetic.
    d[7] = st(double(as_float(half)) * 4.0 + double(as_float(d[5])));
    d[3] = half;
    calls.rectangle_438370(f, d, 12, 0);
    d[1] = st(double(as_float(half)) + double(as_float(d[1])));
    if (as_float(width) != 0)
    {
        d[5] = d[7];
        d[3] = width;
        calls.rectangle_438370(f, d, 12, 0);
        d[1] = st(double(as_float(d[1])) + double(as_float(width)));
    }
    d[3] = half;
    d[7] = right;
    calls.rectangle_438370(f, d, 12, 0);
}
void icon_43da40(FrontendState &f, W kind, W x, W y, SourceFrontendDrawServices &calls)
{
    if (kind - 1u > 3u)
        return; // source default intentionally draws nothing
    draw_frontend_sprite_438710(f, 9, kind + 8u,
                                {st(double(as_float(x)) + 8), st(double(as_float(y)) + 8), 0x3f000000, 0x203}, calls);
    draw_frontend_sprite_438710(f, 8, kind + 4u, {x, y, 0x3f000000, 0x203}, calls);
    draw_frontend_sprite_438710(f, 7, kind, {x, y, 0x3f000000, 0x103}, calls);
}
} // namespace
SourceSelectionPageDrawChildren::SourceSelectionPageDrawChildren(MenuKeyboardAcquisition &k,
                                                                 Win32MenuDirectInputStartup &i, CampaignScoreMusic &m,
                                                                 SessionOuterFrameServices &fs,
                                                                 PolygonSubmissionSink &ps, RaceTextureReplacement &tx,
                                                                 std::uint8_t &ch)
    : SourceMenuWidgetDrawChildren(k, i, m, fs, ps, tx, ch), selection_music_(m)
{
}
void SourceSelectionPageDrawChildren::remaining_page_event(FrontendState &f, W p, W cb, W ev, W arg)
{
    if (cb == 0x45c6a0)
    {
        (void)dispatch_frontend_track_page_45c6a0(f, p, ev, arg, *this, *this, selection_music_, *this, *this);
        return;
    }
    if (cb == 0x451aa0)
    {
        (void)dispatch_frontend_car_page_451aa0(f, p, ev, arg, *this, selection_music_, *this, *this);
        return;
    }
    remaining_selection_page_event(f, p, cb, ev, arg);
}
void SourceSelectionPageDrawChildren::finalize_track_45c670(FrontendState &f, W p)
{
    finalize_frontend_track_page_45c670(f, p);
}
W SourceSelectionPageDrawChildren::track_value_437da0(FrontendState &, W track)
{
    return source_track_value_437da0(track);
}
void SourceSelectionPageDrawChildren::clip_438190(FrontendState &f, std::array<W, 4> v)
{
    set_frontend_clip_438190(f.source, v);
}
void SourceSelectionPageDrawChildren::track_strip_45cba0(FrontendState &f, W selected, W phase)
{
    const auto x = st(fraction(phase) * -1280.0 - 35.0);
    draw_frontend_sprite_438710(f, 17, rd(f, 0x5e396c + selected * 4u) + 0xd9, {x, 0x42a40000, 0x3f800000, 0}, *this);
    draw_frontend_sprite_438710(f, 17, rd(f, 0x5e396c + selected * 4u) + 0xf1,
                                {st(double(as_float(x)) + 255), 0x42a40000, 0x3f800000, 0}, *this);
}
void SourceSelectionPageDrawChildren::track_title_45cc40(FrontendState &f, W track)
{
    draw_frontend_text_438ba0(f, 12, 4, {0x43fd8000, 0x43500000, track + 0x1d0, 0x3fa00000, 0xffffffff, 3}, *this);
}
W SourceSelectionPageDrawChildren::displayed_car_451e80(FrontendState &f, W p, W x, W y)
{
    const auto category = nearest_index(y), index = nearest_index(x);
    if (as_signed(index) >= as_signed(page_word(f, p, 0x50u + category * 48u)))
        return 0;
    return page_word(f, p, 0x58u + (index + category * 12u) * 4u);
}
void SourceSelectionPageDrawChildren::car_strip_452070(FrontendState &f, W car, W phase_x, W phase_y)
{
    const auto x = st(fraction(phase_x) * -1280.0 - 35.0), y = st(fraction(phase_y) * -600.0 + 80.0);
    draw_frontend_sprite_438710(f, 17, car + 0x109, {x, y, 0x3f800000, 0}, *this);
    draw_frontend_sprite_438710(f, 17, car + 0x11e, {st(double(as_float(x)) + 255), y, 0x3f800000, 0}, *this);
}
void SourceSelectionPageDrawChildren::car_frame_452130(FrontendState &f, W p, W x, W y, W phase)
{
    const std::array<W, 4> in{phase, nearest_index(phase), x, y};
    std::array<W, 2> out;
    sgn_selection_frame_xy(in.data(), out.data());
    icon_43da40(f, page_word(f, p, 0x54u + page_word(f, p, 0x1c) * 48u), out[0], out[1], *this);
}
void SourceSelectionPageDrawChildren::car_player_452000(FrontendState &f, W p, W x, W y)
{
    const auto player = page_word(f, p, 0x20);
    const std::array<W, 3> in{x, page_word(f, p, 0x34), 0};
    W out;
    sgn_selection_player_x(in.data(), &out);
    draw_frontend_sprite_438710(f, 13, player + 0x35, {out, y, 0x3f800000, 0x203}, *this);
    draw_frontend_sprite_438710(f, 12, player + 0x31, {out, y, 0x3f800000, 0x103}, *this);
}
void SourceSelectionPageDrawChildren::track_player_45d130(FrontendState &f, W p, W x, W y)
{
    const auto player = page_word(f, p, 0x18);
    const std::array<W, 3> in{x, page_word(f, p, 0x28), 1};
    W out;
    sgn_selection_player_x(in.data(), &out);
    draw_frontend_sprite_438710(f, 13, player + 0x35, {out, y, 0x3f800000, 0x203}, *this);
    draw_frontend_sprite_438710(f, 12, player + 0x31, {out, y, 0x3f800000, 0x103}, *this);
}
void SourceSelectionPageDrawChildren::car_stats_451ed0(FrontendState &f, W p)
{
    const bool multiple = as_signed(rd(f, 0x5e379c)) > 1;
    const auto left = st(multiple ? 49 : 81), right = multiple ? 0x43a38000u : 0x43b38000u;
    const SourceMenuRect rect{left, 0x43b70000, right, 0x43de0000};
    frame_439450(f, 12, 111, rect, 0x40);
    fill_439380(f, 13, 0, rect, 0, 0);
    const auto lx = st(double(as_float(left)) + 70), bx = st(double(as_float(lx)) + 16);
    for (W i = 0; i < 3; ++i)
    {
        const auto y = st(double(i * 20u) + 384);
        label(f, lx, y, i + 0x1c, 0x3f800000, 0xffffffff, 6, 2);
        const auto scaled = st(double(as_float(page_word(f, p, 0x38u + i * 4u))) * 20.0);
        const auto width = st(double(as_float(scaled)) * 8.0);
        bar_438230(f, rd(f, 0x4869c4 + i * 4u), bx, y, width, *this);
    }
}
void SourceSelectionPageDrawChildren::car_details_4521b0(FrontendState &f, W p, W car)
{
    const auto y = rd(f, 0x5e3790) == 5 ? 0x43210000u : 0x43500000u;
    draw_frontend_text_438ba0(f, 12, 4, {0x43fd8000, y, car + 0x19f, 0x3fa00000, 0xffffffff, 3}, *this);
    if (rd(f, 0x5e3790) != 5)
        return;
    label(f, 0x43f18000, 0x43918000, 0x185, 0x3fc00000, 0xffffffff, 6);
    text(f, 0x4411c000, 0x43918000, 4, page_string(f, p, 0x114, 0x134), 0x3fc00000, 0xffffffff, 6, 0x41600000);
    label(f, 0x43f18000, 0x439d8000, 0x186, 0x3fc00000, 0xffffffff, 6);
    text(f, 0x4411c000, 0x439d8000, 4, number(f, page_word(f, p, 0x134u + car * 4u)), 0x3fc00000, 0xffffffff, 6,
         0x41600000);
}
void SourceSelectionPageDrawChildren::track_details_45cc70(FrontendState &f, W p)
{
    const auto mode = rd(f, 0x5e3790);
    if (mode == 2)
    {
        f.source.source_word(rd(f, 0x489508) + 0x2c) = page_word(f, p, 0x30) + 0x1ff;
        f.source.source_word(rd(f, 0x489508) + 0x4c) = rd(f, 0x5e39e0) + 0x202;
        f.source.source_word(rd(f, 0x489508) + 0x6c) = rd(f, 0x5e39dc) + 0x205;
        return;
    }
    if (mode == 6)
    {
        f.source.source_word(rd(f, 0x489508) + 0x2c) = page_word(f, p, 0x30) + 0x1ff;
        return;
    }
    if (mode != 3 && mode != 5)
        return; // actual switch default
    const auto index = page_word(f, p, 0x1c), track = rd(f, 0x5e396c + index * 4u);
    const SourceMenuRect r{0x43d38000, 0x43b70000, 0x4413c000, 0x43dd0000};
    frame_439450(f, 10, 111, r, 0x40);
    fill_439380(f, 13, 2, r, 0, 0);
    label(f, 0x43fd8000, 0x43b90000, 0x24, 0x3fa00000, 0xffff8080, 1);
    if (mode == 3)
    {
        // Full45CD60. Read colors again before the second formatting call.
        label(f, 0x43d88000, 0x43ca0000, 0x22, 0x3fa00000, 0xffffffff, 2);
        const auto record = 0x5dfbf4u + track * 84u;
        const auto color = [&] { return ((rd(f, record) != 0 ? 0xffffffffu : 0u) & 0xffffe401u) - 1u; };
        const auto first_color = color();
        text(f, 0x43f88000, 0x43ca0000, 4, initials(f, record + 8u), 0x3fa00000, first_color, 3);
        const auto second_color = color();
        text(f, 0x44114000, 0x43ca0000, 4, time_text(f, rd(f, record + 4u)), 0x3fa00000, second_color, 6, 0x41600000);
        if (rd(f, 0x5e3c44) == track)
        {
            label(f, 0x43d88000, 0x43d30000, 0x23, 0x3fa00000, 0xffffffff, 2);
            // Source direct string, not43D690. Source span is read until its actual NUL.
            text(f, 0x43f88000, 0x43d30000, 4, f.source.source_bytes(0x5e3c74, 4), 0x3fa00000, 0xffffffff, 3);
            text(f, 0x44114000, 0x43d30000, 4, time_text(f, rd(f, 0x5e3c64)), 0x3fa00000, 0xffffffff, 6, 0x41600000);
        }
        // 45CCD6 rereads the indexed track after all draw/format calls.
        if (rd(f, 0x5e3c44) == rd(f, 0x5e396c + index * 4u))
            f.source.source_word(0x489380) = 2;
        else
        {
            const auto child = page_word(f, p, 0x14);
            if (menu_text_event(f, child, 26, 0) == 2)
                menu_text_event(f, child, 28, 1);
            f.source.source_word(0x489380) = 0xfffffffe;
        }
        return;
    }
    // Full45CF10, original three rows with distinct record offsets.
    const auto record = track * 32u;
    label(f, 0x43d88000, 0x43c60000, 0x25, 0x3fa00000, 0xffffffff, 2);
    text(f, 0x43f88000, 0x43c60000, 4, initials(f, 0x5e0f80 + record), 0x3fa00000, 0xffffffff, 3);
    text(f, 0x44114000, 0x43c60000, 4, time_text(f, rd(f, 0x5e0f7c + record)), 0x3fa00000, 0xffffffff, 6, 0x41600000);
    label(f, 0x43d88000, 0x43ce0000, 0x26, 0x3fa00000, 0xffffffff, 2);
    text(f, 0x43f88000, 0x43ce0000, 4, initials(f, 0x5e0f88 + record), 0x3fa00000, 0xffffffff, 3);
    text(f, 0x44114000, 0x43ce0000, 4, time_text(f, rd(f, 0x5e0f84 + record)), 0x3fa00000, 0xffffffff, 6, 0x41600000);
    label(f, 0x43d88000, 0x43d60000, 0x27, 0x3fa00000, 0xffffffff, 2);
    text(f, 0x43f88000, 0x43d60000, 4, initials(f, 0x5e0f98 + record), 0x3fa00000, 0xffffffff, 3);
    std::array<std::uint8_t, 32> local;
    const std::array<W, 1> args{rd(f, 0x5e0f94 + record)};
    SourceScriptPathCall call;
    auto platform = f.shared_platform();
    format_selection_numbers_46280c(platform->state().boot_config, platform->source_application_literals(),
                                    f.source.source_bytes(0x489524, 5), args, local, 0x45d0fb, call);
    text(f, 0x44114000, 0x43d60000, 4, std::span{local}.first(call.initialized_prefix), 0x3fa00000, 0xffffffff, 6,
         0x41600000);
}
} // namespace sgn::retail
