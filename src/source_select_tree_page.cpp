#include "sgn/source_select_tree_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

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
std::uint8_t &byte(FrontendState &f, W address)
{
    return f.source.source_bytes(address, 1)[0];
}
W stage_track_437920(FrontendState &f, W stage)
{
    return read(f, 0x5f9c10u + stage * 132u);
}
template <class Store> W branches_437930(W stage, Store store)
{
    store(0u, 0u);
    store(1u, 0u); // 43793C then437942, including an invalid stage
    const auto index = stage - 7u;
    if (index > 11u)
        return 0;
    // Literal437A58 jump destinations: seven/twelve/sixteen begin each row.
    constexpr std::array<W, 12> left{1, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 14};
    store(1u, left[index] + 1u);
    store(0u, left[index]);
    return 1;
}
W initialize_tree_45d1a0(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &calls)
{
    const auto initial = st(double(as_signed(source_word(f, 0x5e39e8))));
    const auto count = source_word(f, 0x627ff4) + 1u, selected = source_word(f, 0x5e39e8);
    set_page_word(f, page, 0x24, initial);
    set_page_word(f, page, 0x28, initial);
    set_page_word(f, page, 0x20, selected);
    source_word(f, 0x627ff4) = count;
    source_word(f, 0x5e39e4) = 0;
    set_page_word(f, page, 0x1c, 0);
    auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto close = source_word(f, 0x4794e8);
    clear(f, child, 200);
    const auto event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x489678);
    set_page_word(f, child, 8, 0);
    set_page_word(f, child, 16, event);
    if (initialize_frontend_menu_text_43c860(f, child, 0x489640, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    (void)calls.menu_text_event(f, page_word(f, page, 0x14), 24, 0);
    child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto second_event = source_word(f, 0x4794ec);
    clear(f, child, 200);
    const auto second_close = source_word(f, 0x4794e8);
    set_page_word(f, child, 12, second_close);
    set_page_word(f, child, 16, second_event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x489684);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4895b0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x18, 0);
    }
    else
        set_page_word(f, page, 0x18, child);
    (void)calls.menu_text_event(f, page_word(f, page, 0x18), 24, 0);
    const auto index = source_word(f, 0x5e39ec), stage = read(f, 0x5e39ecu + index * 4u);
    (void)branches_437930(stage, [&](W slot, W value) { set_page_word(f, page, 0x2cu + slot * 4u, value); });
    const auto first_track = stage_track_437920(f, page_word(f, page, 0x2c));
    const auto second_stage = page_word(f, page, 0x30);
    set_page_word(f, page, 0x34, first_track);
    set_page_word(f, page, 0x38, stage_track_437920(f, second_stage));
    return 1;
}
void open_exit_45db10(FrontendState &f, W page, SourceMainPageServices &calls)
{
    (void)calls.menu_text_event(f, page_word(f, page, 0x14), 20, 0);
    const auto child = page_word(f, page, 0x18);
    (void)calls.menu_text_event(f, child, 19, 0);
    (void)calls.menu_text_event(f, child, 28, 2);
    set_page_word(f, page, 0x1c, 1);
}
void close_exit_45db50(FrontendState &f, W page, SourceMainPageServices &calls)
{
    (void)calls.menu_text_event(f, page_word(f, page, 0x14), 19, 0);
    (void)calls.menu_text_event(f, page_word(f, page, 0x18), 20, 0);
    set_page_word(f, page, 0x1c, 0);
}
void draw_preview_45da70(FrontendState &f, W page, W selected, W position, SourceTitlePageServices &draw)
{
    // 462860's low DWORD is spilled and then FILD signed, not a rounded float
    // or a floor. Source PC53, finite input; keep each actual FSTP boundary.
    const auto truncated = W(frontend_ftol_f32_462860(position));
    double fraction = double(as_float(position)) - double(as_signed(truncated));
    if (fraction >= 0.5)
        fraction -= 1.0;
    const double shifted = fraction * -1280.0;
    const auto track = page_word(f, page, 0x34u + selected * 4u);
    const auto x = st(shifted - 35.0);
    draw.sprite_438710(f, 17, track + 0xd9u, {x, 0x42a40000, 0x3f800000, 0});
    draw.sprite_438710(f, 17, track + 0xf1u, {st(double(as_float(x)) + 255.0), 0x42a40000, 0x3f800000, 0});
}
void draw_name_45da40(FrontendState &f, W stage, SourceTitlePageServices &draw)
{
    const auto track = stage_track_437920(f, stage);
    draw.text_438ba0(f, 12, 4, {0x43fd8000, 0x43500000, track + 0x1d0u, 0x3fa00000, 0xffffffffu, 3});
}
void draw_tree_45d630(FrontendState &f, W selected, W direction, SourceMenuPageDrawServices &panels,
                      SourceTitlePageServices &draw)
{
    const auto flash = source_word(f, 0x5ff45c) & 0x10u;
    const auto history = source_word(f, 0x5e39ec);
    // Exactly19 bytes, in source DWORD/DWORD/DWORD/DWORD/WORD/BYTE order.
    source_word(f, 0x627fe0) = 0;
    source_word(f, 0x627fe4) = 0;
    source_word(f, 0x627fe8) = 0;
    source_word(f, 0x627fec) = 0;
    auto low = f.source.source_bytes(0x627ff0, 2);
    low[0] = 0;
    low[1] = 0;
    byte(f, 0x627ff2) = 0;
    if (as_signed(history) > 0)
        for (W i = 0; i < history; ++i)
        {
            const auto stage = read(f, 0x5e39f0u + i * 4u);
            byte(f, 0x627fe0u + stage) = 1;
        }
    const auto current = read(f, 0x5e39ecu + history * 4u);
    const SourceMenuRect rect{0x42440000, 0x43b70000, 0x43c38000, 0x43de0000};
    panels.frame_439450(f, 10, 111, rect, 0x40);
    panels.fill_439380(f, 13, 0, rect, 0, 0);
    W stage = 1, y = 0x43bf0000, label = 3;
    for (W count = 6; count >= 3u; --count, ++label)
    {
        const auto last = count - 1u, half = st(double(as_signed(last)) * 0.5);
        for (W column = 0; column < count; ++column, ++stage)
        {
            const auto offset = st(double(as_signed(column)) - double(as_float(half)));
            const auto mode = source_word(f, 0x5e3790), track = stage_track_437920(f, stage);
            const auto available = source_track_available_4371c0(f, track, mode);
            const auto position = as_float(offset);
            const W shape = position <= -1.0f ? 0u : (position >= 1.0f ? 2u : 1u);
            const W sprite = (available != 0 ? 65u : 68u) + shape;
            if (column == last)
            {
                const auto text = resolve_frontend_language_text(
                    f.source.language_strings(), frontend_language_text_441170(f.source.language_strings(), label));
                queue_frontend_text_438990(f.source, 12, 1, st(double(position) * 56.0 + 232.0), y, 2, text, 0x3f4ccccd,
                                           0xffffffffu, 2, 0); // 438BE0 ->438990, no text measurement
            }
            const auto x = st(double(position) * 56.0 + 220.0);
            draw.sprite_438710(f, 12, sprite, {x, y, 0x3f800000, 0x103});
            if (stage == selected && flash != 0)
            {
                draw.sprite_438710(f, 12, 71, {x, y, 0x3f800000, 0x103});
                if (as_signed(direction) < 0)
                    draw.sprite_438710(
                        f, 12, 74,
                        {st(double(position) * 56.0 + 234.0), st(double(as_float(y)) + 8.0), 0x3f800000, 0x103});
                if (as_signed(direction) > 0)
                    draw.sprite_438710(
                        f, 12, 74, {st(double(as_float(x)) - 14.0), st(double(as_float(y)) + 8.0), 0x3f800000, 0x113});
            }
            const W progress = 0x5df64du + stage * 2u;
            if (byte(f, 0x627fe0u + stage) != 0 && progress > 0x5df659u)
            {
                std::array<W, 2> branches;
                (void)branches_437930(stage, [&](W slot, W value) { branches[slot] = value; });
                if (byte(f, 0x627fe0u + branches[0]) != 0)
                    draw.sprite_438710(
                        f, 12, 75, {st(double(as_float(x)) - 14.0), st(double(as_float(y)) - 8.0), 0x3f800000, 0x103});
                if (byte(f, 0x627fe0u + branches[1]) != 0)
                    draw.sprite_438710(
                        f, 12, 75,
                        {st(double(position) * 56.0 + 234.0), st(double(as_float(y)) - 8.0), 0x3f800000, 0x113});
            }
            if (byte(f, progress - 1u) != 0 && !(stage == current && flash != 0 && as_signed(direction) < 0))
                draw.sprite_438710(f, 12, 76,
                                   {st(double(as_float(x)) - 14.0), st(double(as_float(y)) - 8.0), 0x3f800000, 0x103});
            if (byte(f, progress) != 0 && !(stage == current && flash != 0 && as_signed(direction) > 0))
                draw.sprite_438710(
                    f, 12, 76, {st(double(position) * 56.0 + 234.0), st(double(as_float(y)) - 8.0), 0x3f800000, 0x113});
        }
        y = st(double(as_float(y)) + 16.0);
    }
}
} // namespace
W create_frontend_select_tree_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                          SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 60);
    const auto close = source_word(f, 0x489670);
    clear(f, page, 60);
    set_page_word(f, page, 8, 0);
    const auto event = source_word(f, 0x489674);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 4, 10);
    set_page_word(f, page, 0, 0x47974c);
    set_page_word(f, page, 16, event);
    if (initialize_tree_45d1a0(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_select_tree_page_45d2e0(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto count = source_word(f, 0x627ff4) - 1u;
    const auto first = page_word(f, page, 0x14);
    source_word(f, 0x627ff4) = count;
    if (page_word(f, first, 12) != 0x43c970u)
        throw FormatError("45D2F4 reached an unreconstructed MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), first);
    const auto second = page_word(f, page, 0x18);
    set_page_word(f, page, 0x14, 0);
    if (page_word(f, second, 12) != 0x43c970u)
        throw FormatError("45D30A reached an unreconstructed MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), second);
    set_page_word(f, page, 0x18, 0);
}
W dispatch_frontend_select_tree_page_45d320(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                            CampaignScoreMusic &music, SourceMainPageServices &calls,
                                            SourceMenuPageDrawServices &panels, SourceTitlePageServices &draw)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 19:
        source_word(f, 0x5ff46c) = 5;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        source_word(f, 0x5ff448) = 96;
        source_word(f, 0x5ff44c) = 0xbe99999a;
        source_word(f, 0x5ff450) = 0xbf000000;
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 13:
        if (page_word(f, page, 0x1c) != 0)
            break;
        if (as_signed(page_word(f, page, 0x20)) >= 2)
            set_page_word(f, page, 0x20, 1);
        if (as_signed(page_word(f, page, 0x20)) > 0)
        {
            set_page_word(f, page, 0x20, page_word(f, page, 0x20) - 1u);
            calls.feedback_440890(f, event);
        }
        break;
    case 14:
        if (page_word(f, page, 0x1c) != 0)
            break;
        if (as_signed(page_word(f, page, 0x20)) < 1)
        {
            set_page_word(f, page, 0x20, page_word(f, page, 0x20) + 1u);
            calls.feedback_440890(f, event);
        }
        break;
    case 11:
    case 12:
        (void)calls.menu_text_event(f, page_word(f, page, page_word(f, page, 0x1c) != 0 ? 0x18u : 0x14u), event, 0);
        break;
    case 20:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 17:
        if (page_word(f, page, 0x1c) == 0)
        {
            calls.feedback_440890(f, event);
            open_exit_45db10(f, page, calls);
        }
        else
        {
            close_exit_45db50(f, page, calls);
            calls.feedback_440890(f, event);
        }
        break;
    case 16:
        if (page_word(f, page, 0x1c) != 0)
        {
            calls.feedback_440890(f, event);
            if (calls.menu_text_event(f, page_word(f, page, 0x18), 26, 0) == 1)
                close_exit_45db50(f, page, calls);
            else
            {
                source_word(f, 0x5e39e4) = 1;
                (void)pop_frontend_page_43eef0(f, pages, music);
            }
        }
        else if (calls.menu_text_event(f, page_word(f, page, 0x14), 26, 0) != 0)
        {
            calls.feedback_440890(f, event);
            open_exit_45db10(f, page, calls);
        }
        else
        {
            const auto history = source_word(f, 0x5e39ec), selected = page_word(f, page, 0x20);
            const auto current = read(f, 0x5e39ecu + history * 4u), next = page_word(f, page, 0x2cu + selected * 4u);
            byte(f, 0x5df64cu + current * 2u + selected) = 1;
            const auto old_index = source_word(f, 0x5e39ec);
            source_word(f, 0x5e39f0u + old_index * 4u) = next;
            const auto new_index = source_word(f, 0x5e39ec) + 1u;
            source_word(f, 0x5e39e8) = selected;
            source_word(f, 0x5e39ec) = new_index;
            calls.feedback_440890(f, event);
            (void)pop_frontend_page_43eef0(f, pages, music);
        }
        break;
    case 2: {
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x18), event, 0);
        const auto selected = page_word(f, page, 0x20), target = as_signed(selected) < 2 ? selected : 1u;
        const auto current = page_word(f, page, 0x24), target_float = st(double(as_signed(target)));
        set_page_word(f, page, 0x28, target_float);
        set_page_word(f, page, 0x24,
                      std::bit_cast<W>(approach_frontend_value_43d810(as_float(current), as_float(target_float),
                                                                      as_float(0x3a83126f))));
        // 43DF60 writes six distinct source words, after interpolation returned.
        source_word(f, 0x5ff488) = 0x40000000;
        source_word(f, 0x5ff48c) = 0x3f800000;
        source_word(f, 0x5ff490) = st(double(as_signed(target)));
        source_word(f, 0x5ff494) = 0;
        source_word(f, 0x5ff498) = 0;
        source_word(f, 0x5ff49c) = 0;
        break;
    }
    case 3: {
        const auto position = page_word(f, page, 0x24), selection = page_word(f, page, 0x20);
        draw_preview_45da70(f, page, selection, position, draw);
        set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
        calls.background_4392b0(f, 505, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x18), event, 0);
        const auto selected = page_word(f, page, 0x20), stage = page_word(f, page, 0x2cu + selected * 4u);
        draw_name_45da40(f, stage, draw);
        draw_tree_45d630(f, stage, selected != 0 ? 1u : 0xffffffffu, panels, draw);
        break;
    }
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
