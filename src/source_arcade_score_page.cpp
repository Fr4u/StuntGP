#include "sgn/source_arcade_score_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_challenge_launch.hpp"
#include "sgn/source_save_page_draw.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_menu_draw_children.hpp"
#include "sgn/source_menu_fill.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <array>
#include <bit>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
W st(double v)
{
    return std::bit_cast<W>(vehicle_f32(v));
}
using sgn::retail::frontend_page_words::page_pool;
W get(FrontendState &f, W page, W offset)
{
    if (offset > 0x1fcu)
        throw FormatError("ArcadeScore read leaves its source page block");
    const auto b = frontend_page_object_bytes(page_pool(f), page, offset + 4u);
    W v = 0;
    for (W i = 0; i < 4; ++i)
        v |= std::to_integer<W>(b[offset + i]) << (8u * i);
    return v;
}
void put(FrontendState &f, W page, W offset, W value)
{
    if (offset > 0x1fcu)
        throw FormatError("ArcadeScore write leaves its source page block");
    const auto b = frontend_page_object_bytes(page_pool(f), page, offset + 4u);
    for (W i = 0; i < 4; ++i)
        b[offset + i] = std::byte((value >> (8u * i)) & 255u);
}
using sgn::retail::frontend_page_words::source_word;
W load(FrontendState &f, W a)
{
    return persistence_word(f.source.source_bytes(a, 4), 0);
}
void store(FrontendState &f, W a, W v)
{
    persistence_word(f.source.source_bytes(a, 4), 0, v);
}
W update_record_437d20_437d60(FrontendState &f, W address, W value, bool lower)
{
    const auto previous = load(f, address);
    if (lower ? as_signed(value) >= as_signed(previous) : as_signed(value) <= as_signed(previous))
        return 0;
    // 437D2E/6E: change marker, value, then captured WORD+byte of initials.
    f.source.source_bytes(address + 7u, 1)[0] = 1;
    store(f, address, value);
    const auto name = f.source.source_bytes(0x5fa5e0, 3);
    const W pair = W(name[0]) | (W(name[1]) << 8);
    const auto third = name[2];
    auto out = f.source.source_bytes(address + 4u, 2);
    out[0] = std::uint8_t(pair);
    out[1] = std::uint8_t(pair >> 8);
    f.source.source_bytes(address + 6u, 1)[0] = third;
    return 1;
}
W award_459a00(FrontendState &f, W status)
{
    if (status == 2)
        return source_word(f, 0x5f9c04);
    const auto award = source_word(f, 0x5f9c08); // read even for status0
    return status == 1 ? award : 0;
}
void initialize_459790(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &main)
{
    const auto stage = source_word(f, 0x5fa5d8); // retained before child allocation/callbacks
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto event = source_word(f, 0x4794ec); // 4597AD before child clear
    for (W o = 0; o < 200; o += 4)
        put(f, child, o, 0);
    const auto close = source_word(f, 0x4794e8);
    put(f, child, 12, close);
    put(f, child, 16, event);
    put(f, child, 4, 41);
    put(f, child, 0, 0x488dd8);
    put(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x488da0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        put(f, page, 0x14, 0);
    }
    else
        put(f, page, 0x14, child);
    const auto retained = get(f, page, 0x14);
    (void)main.menu_text_event(f, retained, 24, 0);
    (void)main.menu_text_event(f, retained, 22, 0);
    const auto placing = source_word(f, 0x5fa6a0), score = source_word(f, 0x5fa6b0);
    put(f, page, 0x1c, 0);
    put(f, page, 0x18, 0);
    put(f, page, 0x30, source_word(f, 0x5f9be8u + placing * 4u));
    const auto total_time = source_word(f, 0x5fa6a4);
    put(f, page, 0x48, 1);
    put(f, page, 0x4c, 1);
    put(f, page, 0x50, 1);
    put(f, page, 0x54, 1);
    put(f, page, 0x70, 0);
    put(f, page, 0x74, 0);
    put(f, page, 0x20, 0);
    put(f, page, 0x34, score);
    for (W a = 0x5fa6bcu; a < 0x5fa720u; a += 20u)
    {
        if (as_signed(source_word(f, a - 4u)) < as_signed(total_time))
            put(f, page, 0x48, 0);
        if (as_signed(source_word(f, a)) < as_signed(source_word(f, 0x5fa6a8)))
            put(f, page, 0x4c, 0);
        if (as_signed(source_word(f, a + 4u)) > as_signed(source_word(f, 0x5fa6ac)))
            put(f, page, 0x50, 0);
        if (as_signed(source_word(f, a + 8u)) > as_signed(score))
            put(f, page, 0x54, 0);
    }
    const auto records = 0x5df674u + stage * 32u;
    if (update_record_437d20_437d60(f, records, total_time, true) != 0)
        put(f, page, 0x48, 2);
    if (update_record_437d20_437d60(f, records + 8u, source_word(f, 0x5fa6a8), true) != 0)
        put(f, page, 0x4c, 2);
    if (update_record_437d20_437d60(f, records + 16u, source_word(f, 0x5fa6ac), false) != 0)
        put(f, page, 0x50, 2);
    if (update_record_437d20_437d60(f, records + 24u, source_word(f, 0x5fa6b0), false) != 0)
        put(f, page, 0x54, 2);
    const auto first = award_459a00(f, get(f, page, 0x48));
    put(f, page, 0x38, first);
    const auto second = award_459a00(f, get(f, page, 0x4c));
    put(f, page, 0x3c, second);
    const auto third = award_459a00(f, get(f, page, 0x50));
    put(f, page, 0x40, third);
    const auto fourth = award_459a00(f, get(f, page, 0x54));
    put(f, page, 0x44, fourth);
    const auto old_total = source_word(f, 0x5e37ac);
    put(f, page, 0x28, old_total);
    // The speed award+40 contributes to the total, but retail never animates it.
    source_word(f, 0x5e37ac) = old_total + get(f, page, 0x34) + fourth + third + second + first + get(f, page, 0x30);
    for (W i = 0; i < 6; ++i)
        put(f, page, 0x58u + i * 4u, i);
    for (W remaining = 5; as_signed(remaining) >= 0; --remaining)
    {
        for (W i = 0; i < remaining; ++i)
        {
            const auto current = get(f, page, 0x58u + i * 4u), next = get(f, page, 0x5cu + i * 4u);
            if (as_signed(source_word(f, 0x5fa6a0u + current * 20u)) >
                as_signed(source_word(f, 0x5fa6a0u + next * 20u)))
            {
                put(f, page, 0x58u + i * 4u, next);
                put(f, page, 0x5cu + i * 4u, current);
            }
        }
    }
    put(f, page, 0x78, 0);
    put(f, page, 0x7c, 0);
    for (W i = 0; i < 8; ++i)
    {
        put(f, page, 0x80u + i * 4u, 0);
        put(f, page, 0xa0u + i * 4u, 0);
    }
    if (get(f, page, 0x48) != 0)
        put(f, page, 0x7c, get(f, page, 0x7c) + 1u);
    if (get(f, page, 0x4c) != 0)
        put(f, page, 0x7c, get(f, page, 0x7c) + 1u);
    if (get(f, page, 0x54) != 0)
        put(f, page, 0x7c, get(f, page, 0x7c) + 1u);
}
void badge(FrontendState &f, W page, W sprite)
{
    const auto i = get(f, page, 0x78);
    if (i >= 8u)
        throw FormatError("ArcadeScore badge index leaves original eight-slot arrays");
    put(f, page, 0xa0u + i * 4u, 0x3f800000);
    put(f, page, 0x80u + get(f, page, 0x78) * 4u, sprite);
    put(f, page, 0x78, get(f, page, 0x78) + 1u);
}
void tick_459af9(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMenuSampleReceiving &samples)
{
    const auto pending = get(f, page, 0x24);
    if (pending != 0)
    {
        const auto amount = as_signed(pending) > 1000 ? 1000u : pending;
        const auto difference = get(f, page, 0x2c) - pending;
        W frequency = W(as_signed(difference) / 20) + 20000u;
        if (as_signed(frequency) > 44000)
            frequency = 44000;
        if (as_signed(get(f, page, 0x20)) % 3 == 0)
            source_menu_feedback_440770(f, 11, frequency, samples);
        const auto total = get(f, page, 0x28) + amount, ticks = get(f, page, 0x20) + 1u;
        put(f, page, 0x24, pending - amount);
        put(f, page, 0x28, total);
        put(f, page, 0x20, ticks);
        return;
    }
    const auto delay = get(f, page, 0x1c);
    if (delay != 0)
    {
        put(f, page, 0x1c, delay - 1u);
        return;
    }
    const auto phase = get(f, page, 0x18);
    if (phase == 0)
    {
        put(f, page, 0x1c, 60);
        put(f, page, 0x18, 1);
        return;
    }
    if (phase > 6)
        return;
    if (phase == 6)
    {
        put(f, page, 0x74, 0);
        put(f, page, 0x70, 0);
        return;
    }
    const W offsets[]{0, 0x30, 0x38, 0x3c, 0x44, 0x34};
    W value = get(f, page, offsets[phase]);
    if (value != 0)
    {
        if (phase >= 2 && phase <= 4)
        {
            const W status_offsets[]{0x48, 0x4c, 0x54};
            const W normal[]{0x4d, 0x50, 0x4f}, record[]{0x51, 0x54, 0x53};
            const auto at = status_offsets[phase - 2u];
            if (get(f, page, at) == 1)
                badge(f, page, normal[phase - 2u]);
            if (get(f, page, at) == 2)
                badge(f, page, record[phase - 2u]);
            value = get(f, page, offsets[phase]); // 459C59/CF2/D8B reload
        }
        put(f, page, 0x74, phase + 14u);
        put(f, page, 0x70, value);
        put(f, page, 0x1c, 60);
        put(f, page, 0x20, 0);
        pages.feedback_440880(f, 12); // 45A350
    }
    put(f, page, 0x24, value);
    put(f, page, 0x2c, value);
    put(f, page, offsets[phase], 0);
    if (value == 0)
    {
        put(f, page, 0x18, phase + 1u);
        if (phase == 5)
            pages.feedback_440880(f, 13);
    }
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
void label_438be0(FrontendState &f, W x, W y, W id, W color, W flags, W scale = 0x3fa00000)
{
    queue_frontend_text_438990(f.source, 12, 1, x, y, 4, language(f, id), scale, color, flags, 0);
}
void raw_text(FrontendState &f, W x, W y, std::span<const std::uint8_t> text, W color, W flags, W scale = 0x3fa00000,
              W spacing = 0x41600000)
{
    if (spacing != 0)
        f.world().ui.fonts.metrics().digit_advance_bits = spacing; // 438A20/4291A0
    queue_frontend_text_438990(f.source, 12, 0, x, y, 4, text, scale, color, flags, spacing);
}
void decimal(FrontendState &f, W format, W extent, W value, W site)
{
    const auto platform = f.shared_platform();
    const std::array<W, 1> args{value};
    SourceScriptPathCall call;
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(),
                          f.source.source_bytes(format, extent), args, f.source.source_bytes(0x627c94, 64), site, call);
}
void draw_table_459ec0(FrontendState &f, W page)
{
    const SourceMenuRect r{0x42f00000, 0x42b40000, 0x44020000, 0x43be0000};
    draw_frontend_frame_439450(f, 10, 110, r, 0x40);
    draw_frontend_fill_439380(f, 13, 2, r, 0, 0);
    label_438be0(f, 0x43160000, 0x42dc0000, 9, 0xffff8080, 3);
    label_438be0(f, 0x43480000, 0x42dc0000, 10, 0xffff8080, 2);
    label_438be0(f, 0x43a00000, 0x42dc0000, 11, 0xffff8080, 6);
    label_438be0(f, 0x43cd0000, 0x42dc0000, 13, 0xffff8080, 6);
    label_438be0(f, 0x43fa0000, 0x42dc0000, 12, 0xffff8080, 6);
    W y = 0x43020000;
    for (W i = 0; i < 6; ++i)
    {
        const auto racer = get(f, page, 0x58u + i * 4u), offset = racer * 20u;
        const W color = racer == 0 ? 0xffffffffu : 0xff808080u;
        label_438be0(f, 0x43160000, y, source_word(f, 0x5fa6a0u + offset) + 2u, color, 3);
        const auto initials = copy_frontend_initials_43d690(f, 0x5fa5e0u + racer * 28u);
        raw_text(f, 0x43480000, y, f.source.source_bytes(initials, 4), color, 2, 0x3fa00000, 0);
        raw_text(f, 0x43a00000, y, format_source_time_43d550(f, source_word(f, 0x5fa6a4u + offset)), color, 6);
        raw_text(f, 0x43c50000, y, format_source_time_43d550(f, source_word(f, 0x5fa6a8u + offset)), color, 6);
        decimal(f, 0x488de4, 4, source_word(f, 0x5fa6b0u + offset), 0x45a0d8);
        raw_text(f, 0x43f20000, y, f.source.source_bytes(0x627c94, 64), color, 6);
        y = st(double(as_float(y)) + 20.0);
    }
    y = st(double(as_float(y)) + 10.0);
    decimal(f, 0x488de8, 8, get(f, page, 0x28), 0x45a145);
    if (get(f, page, 0x18) == 6)
    {
        const auto total_y = st(double(as_float(y)) + 20.0);
        label_438be0(f, 0x438c0000, total_y, 14, 0xffff8080, 6, 0x3ff00000);
        raw_text(f, 0x43cd0000, total_y, f.source.source_bytes(0x627c94, 64), 0xffffffff, 6, 0x3ff00000);
    }
    else
    {
        label_438be0(f, 0x43960000, y, 14, 0xffff8080, 6);
        raw_text(f, 0x43c00000, y, f.source.source_bytes(0x627c94, 64), 0xffffffff, 6);
    }
    y = st(double(as_float(y)) + 30.0);
    const auto title = get(f, page, 0x74);
    if (title != 0)
    {
        const auto bonus = get(f, page, 0x70);
        if (bonus != 0)
        {
            label_438be0(f, 0x43960000, y, title, 0xffff8080, 6);
            decimal(f, 0x488df0, 8, bonus, 0x45a242);
            raw_text(f, 0x43c00000, y, f.source.source_bytes(0x627c94, 64), 0xffffffff, 6);
        }
    }
}
void draw_badges_45a280(FrontendState &f, W page, SourceTitlePageServices &draw, SourceFrontendDrawServices &geometry)
{
    // FILD signed(count-1)*30; FSUBR320; only the final result is stored32.
    auto x = st(320.0 - double(as_signed(get(f, page, 0x7c) - 1u)) * 30.0);
    const auto count = get(f, page, 0x78); // retained EBX across draw children
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
    {
        draw.sprite_438710(f, 12, 85, {x, 0x43a90000, 0x3f400000, 0x203});
        draw.sprite_438710(f, 11, get(f, page, 0x80u + i * 4u), {x, 0x43a90000, 0x3f400000, 0x103});
        const auto alpha = get(f, page, 0xa0u + i * 4u);
        // 46EC18 is this exact double. Ordered greater reproduces TEST AH,41h.
        if (double(as_float(alpha)) > std::bit_cast<double>(std::uint64_t{0x3f1a36e2eb1c432d}))
            draw_frontend_sprite_438740(f, 10, 86, {x, 0x43a90000, 0x3f400000, 0x403, alpha}, geometry);
        x = st(double(as_float(x)) + 60.0);
    }
}
} // namespace
W create_frontend_arcade_score_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                           SourceMainPageServices &main)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0xc0);
    const auto close = source_word(f, 0x488dd0);
    for (W o = 0; o < 0xc0; o += 4)
        put(f, page, o, 0);
    put(f, page, 8, 0);
    const auto event = source_word(f, 0x488dd4);
    put(f, page, 12, close);
    put(f, page, 4, 36);
    put(f, page, 0, 0x479888);
    put(f, page, 16, event);
    initialize_459790(f, page, pages, main);
    put(f, page, 8, key);
    return page;
}
void destroy_frontend_arcade_score_page_459a20(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = get(f, page, 0x14);
    if (get(f, child, 12) != 0x43c970)
        throw FormatError("ArcadeScore reached an unreconstructed MenuText destructor");
    release_frontend_page_object_43d4c0(page_pool(f), child);
    put(f, page, 0x14, 0);
}
W dispatch_frontend_arcade_score_page_459a40(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                             CampaignScoreMusic &music, SourceMainPageServices &main,
                                             SourceTitlePageServices &draw, SourceFrontendDrawServices &geometry,
                                             SourceMenuSampleReceiving &samples)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        source_word(f, 0x5ff448) = 95;
        source_word(f, 0x5ff44c) = 0x3e99999a;
        source_word(f, 0x5ff450) = 0xbf000000;
        [[fallthrough]];
    case 20:
        (void)main.menu_text_event(f, get(f, page, 0x14), event, 0);
        break;
    case 16:
        (void)main.menu_text_event(f, get(f, page, 0x14), event, 0);
        clear_persistence_record_changes(f.source.persistence_view());
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 1:
        tick_459af9(f, page, pages, samples);
        break;
    case 2:
        (void)main.menu_text_event(f, get(f, page, 0x14), event, 0);
        for (W i = 0; i < 8; ++i)
        {
            const auto value =
                approach_frontend_value_43d810(as_float(get(f, page, 0xa0u + i * 4u)), 0.0f, as_float(0x3a83126f));
            put(f, page, 0xa0u + i * 4u, std::bit_cast<W>(value));
        }
        break;
    case 3:
        draw_table_459ec0(f, page);
        draw_badges_45a280(f, page, draw, geometry);
        set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
        (void)main.menu_text_event(f, get(f, page, 0x14), event, 0);
        main.background_4392b0(f, 0x1fa, 0);
        break;
    default:
        break; // event17 is ignored; event16 above leaves immediately
    }
    return 0;
}
} // namespace sgn::retail
