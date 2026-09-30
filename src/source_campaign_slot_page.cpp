#include "sgn/source_campaign_slot_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_challenge_launch.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
W st(double value)
{
    return std::bit_cast<W>(vehicle_f32(value));
}
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
void clear(FrontendState &f, W page, W size)
{
    for (W at = 0; at < size; at += 4u)
        set_page_word(f, page, at, 0);
}
using sgn::retail::frontend_page_words::source_word;
W read(FrontendState &f, W address)
{
    const auto b = f.source.source_bytes(address, 4);
    W value = 0;
    for (W i = 0; i < 4u; ++i)
        value |= W(b[i]) << (8u * i);
    return value;
}
template <std::size_t N> auto bank(FrontendState &f, W address)
{
    return std::as_writable_bytes(std::span<std::uint8_t, N>{f.source.source_bytes(address, N)});
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
W price_437de0(FrontendState &f, W sponsor, W car, W configuration)
{
    // Original0x218-byte caller local; the actual436490 fills it from live banks.
    // No stricter car0..20 host precondition is substituted for the source read.
    std::array<std::byte, 0x218> local;
    compose_campaign_configuration_436490({bank<0x57e4>(f, 0x5f3af4), bank<0x660>(f, 0x5f943c)}, sponsor,
                                          bank<0xec>(f, configuration), bank<300>(f, 0x5f3af4u + car * 300u), local);
    W value;
    std::memcpy(&value, local.data() + 0xec, 4);
    return value;
}
std::span<const std::uint8_t> value_43d5f0(FrontendState &f, W value)
{
    // Actual45AAFE contract EDX=0. Negative signed values return live479664.
    if (as_signed(value) < 0)
        return f.source.source_bytes(0x479664, 4);
    auto scratch = f.source.source_bytes(0x5ff328, 128);
    W written = 0, group = 0;
    do
    {
        if (group++ == 3)
        {
            scratch[written++] = ',';
            group = 1;
        }
        scratch[written++] = std::uint8_t('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    W out = 0;
    while (written != 0)
    {
        const auto byte = scratch[written - 1u];
        --written;
        scratch[64u + out++] = byte;
    }
    scratch[64u + out] = 0;
    return scratch.subspan(64, out + 1u);
}
void copy_value(FrontendState &f, W destination, std::span<const std::uint8_t> source)
{
    std::size_t length = 0;
    while (length < source.size() && source[length] != 0)
        ++length;
    if (length == source.size())
        throw FormatError("45AB13 numeric source string leaves its represented owner");
    ++length;
    auto out = f.source.source_bytes(destination, length);
    std::size_t at = 0;
    for (; at + 4u <= length; at += 4u)
    {
        W value;
        std::memcpy(&value, source.data() + at, 4);
        std::memcpy(out.data() + at, &value, 4);
    }
    for (; at < length; ++at)
        out[at] = source[at];
}
void format_literal(FrontendState &f, W format, W extent, W argument, W destination, W capacity, W return_site,
                    SourceSaveStringBorrow borrowed = {})
{
    const std::array<W, 1> args{argument};
    SourceScriptPathCall call;
    const auto platform = f.shared_platform();
    (void)format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(),
                                f.source.source_bytes(format, extent), args,
                                f.source.source_bytes(destination, capacity), return_site, call, borrowed);
}
W initialize_slot_45a9c0(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &calls)
{
    const auto selected = source_word(f, 0x5e38e4);
    set_page_word(f, page, 0x1c, as_signed(selected) >= 0 ? selected : 0u);
    const auto row = st(double(as_signed(page_word(f, page, 0x1c))));
    source_word(f, 0x5e38e0) = 0;
    source_word(f, 0x5e38e8) = 0;
    set_page_word(f, page, 0x18, 0);
    set_page_word(f, page, 0x30, row);
    set_page_word(f, page, 0x2c, row);
    set_page_word(f, page, 0x14, 1);
    set_page_word(f, page, 0x24, 0x3f800000);
    set_page_word(f, page, 0x28, 0x3f800000);
    auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    clear(f, child, 200);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4890b8);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x488ff0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x34, 0);
    }
    else
        set_page_word(f, page, 0x34, child);
    child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto second_close = source_word(f, 0x4794e8); // 45AA68 capture BEFORE the second clear
    clear(f, child, 200);
    const auto second_event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, second_close);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4890c4);
    set_page_word(f, child, 8, 0);
    set_page_word(f, child, 16, second_event);
    if (initialize_frontend_menu_text_43c860(f, child, 0x489080, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x38, 0);
    }
    else
        set_page_word(f, page, 0x38, child);
    (void)calls.menu_text_event(f, page_word(f, page, 0x34), 24, 0);
    (void)calls.menu_text_event(f, page_word(f, page, 0x38), 24, 0);
    W destination = 0x627dd8;
    for (W record = 0x5e047c; record < 0x5e0f7c; record += 0x160u, destination += 32u)
    {
        const auto active = read(f, record);
        auto value = read(f, record + 12u);
        if (active != 0)
        {
            const auto car = read(f, record + 16u), sponsor = read(f, record + 0x108u);
            value += price_437de0(f, sponsor, car, record + 20u);
        }
        const auto text = value_43d5f0(f, value);
        copy_value(f, destination - 256u, text);
        const auto duration = read(f, record + 0x104u);
        format_literal(f, 0x4890d0, 4, duration, destination, 32, 0x45ab40);
    }
    return 1;
}
void raw_text(FrontendState &f, W x, W y, std::span<const std::uint8_t> text, W color, W flags, W spacing = 0)
{
    if (spacing != 0)
        f.world().ui.fonts.metrics().digit_advance_bits = spacing; // 438A20 ->4291A0
    queue_frontend_text_438990(f.source, 12, 0, x, y, 4, text, 0x3fc00000, color, flags, spacing);
}
void label(FrontendState &f, W x, W y, W id, W color, W flags)
{
    queue_frontend_text_438990(f.source, 12, 1, x, y, 4, language(f, id), 0x3fc00000, color, flags, 0);
}
void draw_rows_45af60(FrontendState &f, W page, SourceMenuPageDrawServices &draw)
{
    const double offset = (double(as_float(page_word(f, page, 0x24))) - 1.0) * 640.0;
    const auto left = st(offset + 40.0), right = st(offset + 600.0);
    const SourceMenuRect frame{left, 0x42b60000, right, 0x43958000};
    draw.frame_439450(f, 10, 110, frame, 0x40);
    draw.fill_439380(f, 13, 2, frame, 0, 0);
    const auto selected = page_word(f, page, 0x1c);
    const auto origin = st((double(as_float(left)) + 100.0) - 12.0);
    const auto ordinal_x = st(double(as_float(origin)) + 4.0);
    W y = 0x42de0000;
    W value_text = 0x627dd8, record = 0x5e0480;
    for (W index = 0; index < 8u; ++index, record += 0x160u, value_text += 32u)
    {
        const auto color = index == selected ? 0xffff8080u : 0xffffffffu;
        label(f, ordinal_x, y, index + 0x2fu, color, 6);
        if (read(f, record - 4u) == 0)
            label(f, st(double(as_float(origin)) + 16.0), y, 0x37, color, 2);
        else
        {
            const auto initials = copy_frontend_initials_43d690(f, record);
            raw_text(f, st(double(as_float(origin)) + 16.0), y, f.source.source_bytes(initials, 4), color, 2);
            const auto id =
                frontend_language_text_441170(f.source.language_strings(), read(f, record + 0x104u) + 0x1eau);
            const auto text = resolve_frontend_language_text(f.source.language_strings(), id);
            format_literal(f, 0x4890d4, 8, id.value, 0x627ed8, 256, 0x45b0c6, {4, text});
            raw_text(f, st(double(as_float(origin)) + 64.0), y, f.source.source_bytes(0x627ed8, 256), color, 2);
            label(f, st((double(as_float(origin)) + 192.0) - 24.0), y, 0x1b, color, 3);
            raw_text(f, st(double(as_float(origin)) + 264.0), y, f.source.source_bytes(value_text - 256u, 32), color, 6,
                     0x41600000);
            label(f, st((double(as_float(origin)) + 352.0) - 16.0), y, 0x39, color, 6);
            raw_text(f, st(double(as_float(origin)) + 376.0), y, f.source.source_bytes(value_text, 32), color, 6,
                     0x41600000);
            label(f, st(double(as_float(origin)) + 396.0), y, 0x38, color, 2);
            const auto race = read(f, record + 4u);
            format_literal(f, 0x4890dc, 4, race, 0x627ed8, 256, 0x45b1e6);
            raw_text(f, st(double(as_float(origin)) + 460.0), y, f.source.source_bytes(0x627ed8, 256), color, 6);
        }
        y = st(double(as_float(y)) + 24.0);
    }
    const double center = double(as_float(page_word(f, page, 0x2c))) * 24.0 + 111.0;
    auto pulse = (page_word(f, page, 0x20) - 32u) & 63u;
    if (pulse >= 32u)
        pulse = 64u - pulse;
    const auto alpha = st(double(as_signed(pulse + 32u)) * 0.015625);
    draw.fill_439380(f, 11, 4, {left, st(center - 16.0), right, st(center + 16.0)}, 0x400, alpha);
}
void draw_choose_45b2c0(FrontendState &f, W page, SourceMenuPageDrawServices &panels, SourceTitlePageServices &titles)
{
    const SourceMenuRect rect{0x42200000, 0x43a60000, 0x44160000, 0x43d80000};
    panels.frame_439450(f, 10, 111, rect, 0x40);
    panels.fill_439380(f, 13, 2, rect, 0x100, 0);
    const auto active = read(f, 0x5e047cu + page_word(f, page, 0x1c) * 0x160u);
    titles.text_438ba0(f, 12, 4, {0x43a00000, 0x43bf0000, 0x3cu + W(active != 0), 0x3fc00000, 0xffffffff, 0x803});
}
void draw_confirm_45b360_45b440(FrontendState &f, W page, W id, W child_offset, SourceMainPageServices &calls,
                                SourceMenuPageDrawServices &panels, SourceTitlePageServices &titles)
{
    const SourceMenuRect left{0x42200000, 0x43a60000, 0x43cc0000, 0x43d80000};
    panels.frame_439450(f, 10, 111, left, 0x40);
    panels.fill_439380(f, 13, 2, left, 0x100, 0);
    titles.text_438ba0(f, 12, 4, {0x43600000, 0x43bf0000, id, 0x3fc00000, 0xffffffff, 0x803});
    const SourceMenuRect right{0x43dc0000, 0x43a60000, 0x44160000, 0x43d80000};
    panels.frame_439450(f, 10, 110, right, 0x40);
    panels.fill_439380(f, 13, 2, right, 0, 0);
    (void)calls.menu_text_event(f, page_word(f, page, child_offset), 3, 0);
}
void background_43dd40(FrontendState &f, W descriptor)
{
    source_word(f, 0x5ff448) = descriptor;
    source_word(f, 0x5ff44c) = 0x3e99999a;
    source_word(f, 0x5ff450) = 0xbf000000;
}
} // namespace
W create_frontend_campaign_slot_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                            SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 60);
    clear(f, page, 60);
    const auto event = source_word(f, 0x4890b4);
    set_page_word(f, page, 8, 0);
    const auto close = source_word(f, 0x4890b0);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 4, 27);
    set_page_word(f, page, 0, 0x47987c);
    if (initialize_slot_45a9c0(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_campaign_slot_page_45ab70(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto first = page_word(f, page, 0x34);
    if (page_word(f, first, 12) != 0x43c970u)
        throw FormatError("45AB79 reached an unreconstructed MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), first);
    const auto second = page_word(f, page, 0x38);
    set_page_word(f, page, 0x34, 0); // 45AB83 precedes45AB86
    if (page_word(f, second, 12) != 0x43c970u)
        throw FormatError("45AB8F reached an unreconstructed MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), second);
    set_page_word(f, page, 0x38, 0);
}
W dispatch_frontend_campaign_slot_page_45abb0(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                              CampaignScoreMusic &music, SourceMainPageServices &calls,
                                              SourceMenuPageDrawServices &panels, SourceTitlePageServices &titles)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        background_43dd40(f, 95);
        (void)calls.menu_text_event(f, page_word(f, page, 0x38), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x34), event, 0);
        set_page_word(f, page, 0x18, 1);
        break;
    case 2: {
        const auto target = st(double(as_signed(page_word(f, page, 0x1c))));
        const auto tick = page_word(f, page, 0x20) + 1u;
        set_page_word(f, page, 0x20, tick);
        const auto current = page_word(f, page, 0x2c);
        set_page_word(f, page, 0x30, target);
        const auto next = approach_frontend_value_43d810(as_float(current), as_float(target), as_float(0x3e4ccccd));
        const auto slide_target = page_word(f, page, 0x28), slide_current = page_word(f, page, 0x24);
        set_page_word(f, page, 0x2c, std::bit_cast<W>(next));
        set_page_word(f, page, 0x24,
                      std::bit_cast<W>(approach_frontend_value_43d810(as_float(slide_current), as_float(slide_target),
                                                                      as_float(0x3a83126f))));
        [[fallthrough]];
    }
    case 1:
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 0x38), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x34), event, 0);
        break;
    case 3: {
        calls.background_4392b0(f, 44, 0);
        draw_rows_45af60(f, page, panels);
        const auto state = page_word(f, page, 0x14);
        background_43dd40(f, state == 3 && page_word(f, page, 0x18) != 0 ? 105u : 95u);
        if (state == 1)
            draw_choose_45b2c0(f, page, panels, titles);
        else if (state == 2)
            draw_confirm_45b360_45b440(f, page, 0x3a, 0x34, calls, panels, titles);
        else if (state == 3)
            draw_confirm_45b360_45b440(f, page, 0x3b, 0x38, calls, panels, titles);
        break;
    }
    case 11:
    case 12: {
        const auto state = page_word(f, page, 0x14);
        if (state == 1)
        {
            const auto selected = page_word(f, page, 0x1c);
            if (event == 11 ? as_signed(selected) <= 0 : as_signed(selected) >= 7)
                break;
            set_page_word(f, page, 0x20, 0);
            set_page_word(f, page, 0x1c, event == 11 ? selected - 1u : selected + 1u);
            calls.feedback_440890(f, event);
        }
        else if (state == 2)
            (void)calls.menu_text_event(f, page_word(f, page, 0x34), event, 0);
        else if (state == 3)
            (void)calls.menu_text_event(f, page_word(f, page, 0x38), event, 0);
        break;
    }
    case 16: {
        const auto state = page_word(f, page, 0x14);
        if (state == 1)
        {
            calls.feedback_440890(f, event);
            const auto selected = page_word(f, page, 0x1c);
            if (read(f, 0x5e047cu + selected * 0x160u) != 0)
            {
                (void)calls.menu_text_event(f, page_word(f, page, 0x34), 28, 0);
                set_page_word(f, page, 0x14, 2);
            }
            else
            {
                source_word(f, 0x5e38e8) = 1;
                source_word(f, 0x5e38e4) = selected;
                set_page_word(f, page, 0x14, 4);
                (void)pop_frontend_page_43eef0(f, pages, music);
            }
        }
        else if (state == 2)
        {
            const auto child = page_word(f, page, 0x34);
            (void)calls.menu_text_event(f, child, event, 0);
            const auto selected = calls.menu_text_event(f, child, 26, 0); // same retained child
            if (selected == 0)
            {
                source_word(f, 0x5e38e4) = page_word(f, page, 0x1c);
                set_page_word(f, page, 0x14, 4);
                (void)pop_frontend_page_43eef0(f, pages, music);
            }
            else if (selected == 1)
            {
                set_page_word(f, page, 0x14, 3);
                (void)calls.menu_text_event(f, page_word(f, page, 0x38), 28, 0);
            }
        }
        else if (state == 3)
        {
            const auto child = page_word(f, page, 0x38), selected = calls.menu_text_event(f, child, 26, 0);
            if (selected == 0)
            {
                (void)calls.menu_text_event(f, child, event, 0);
                const auto row = page_word(f, page, 0x1c);
                source_word(f, 0x5e38e8) = 1;
                source_word(f, 0x5e38e4) = row;
                set_page_word(f, page, 0x14, 4);
                (void)pop_frontend_page_43eef0(f, pages, music);
            }
            else if (selected == 1)
            {
                set_page_word(f, page, 0x14, 1);
                pages.feedback_440880(f, 2);
            }
        }
        break;
    }
    case 17: {
        const auto state = page_word(f, page, 0x14);
        if (state == 1)
        {
            source_word(f, 0x5e38e0) = 1;
            calls.feedback_440890(f, event);
            (void)pop_frontend_page_43eef0(f, pages, music);
        }
        else if (state == 2 || state == 3)
        {
            (void)calls.menu_text_event(f, page_word(f, page, state == 2 ? 0x34u : 0x38u), event, 0);
            set_page_word(f, page, 0x14, 1);
            calls.feedback_440890(f, event);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
