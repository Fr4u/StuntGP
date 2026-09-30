#include "sgn/source_modify_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
extern "C" void sgn_modify_numeric(const std::uint32_t *, std::uint32_t *, std::uint32_t) noexcept;
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::page_pool;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
using sgn::retail::frontend_page_words::source_word;
void clear(FrontendState &f, W page, W size)
{
    for (W at = 0; at < size; at += 4)
        set_page_word(f, page, at, 0);
}
W num(W operation, W a, W b = 0, W c = 0)
{
    const std::array<W, 3> input{a, b, c};
    W out;
    sgn_modify_numeric(input.data(), &out, operation);
    return out;
}
W integer(W a)
{
    return num(0, a);
}
W add(W a, W b)
{
    return num(8, a, b);
}
W sub(W a, W b)
{
    return num(9, a, b);
}
W mul(W a, W b)
{
    return num(10, a, b);
}
W approach(W from, W to)
{
    return std::bit_cast<W>(approach_frontend_value_43d810(as_float(from), as_float(to), as_float(0x3a83126f)));
}
template <std::size_t N> auto bank(FrontendState &f, W address)
{
    return std::as_writable_bytes(std::span<std::uint8_t, N>{f.source.source_bytes(address, N)});
}
CampaignConfigurationCatalog catalog(FrontendState &f)
{
    return {bank<0x57e4>(f, 0x5f3af4), bank<0x660>(f, 0x5f943c)};
}
W material_437af0(W category)
{
    constexpr W values[]{173, 194, 150, 120, 135, 130, 155, 125, 145, 140, 165, 160, 170};
    return category - 1u < 13u ? values[category - 1u] : 0u;
}
W item_43db20(W category, W level)
{
    // The source does not read the two caller stack words;436420 consumes them.
    constexpr W bases[]{0x5f6b00, 0x5f596c, 0x5f5390, 0x5f70dc, 0x5f85f4, 0x5f6524,
                        0x5f7a3c, 0x5f8bd0, 0x5f8018, 0x5f5f48, 0x5f76b8};
    return category - 3u < 11u ? bases[category - 3u] + level * 300u : 0;
}
W price(FrontendState &f, W category, W level, W selection)
{
    const auto item = item_43db20(category, level);
    const auto sponsor = source_word(f, 0x5f93e4); // after43DB20, before436420
    return campaign_item_price_436420(catalog(f), sponsor, bank<16>(f, selection), bank<300>(f, item));
}
void statistics_436d60(FrontendState &f, W page, const std::array<W, 59> &source)
{
    // 436D60 takes a STACK configuration here, unlike the existing car-page
    // address-only private helper. The original59-word second local is retained.
    auto local = source;
    for (W at : std::array<W, 11>{0x60, 0x50, 0xd0, 0x90, 0x40, 0x70, 0xe0, 0xa0, 0xc0, 0x80, 0xb0})
        if (local[at / 4] == 0)
            local[at / 4] = at == 0xe0 ? 3u : 5u;
    const auto s = local[0xb0 / 4], d = local[0xc0 / 4], q = local[0x60 / 4];
    auto a = 2u * (s + 4u * d + q), b = 2u * (local[0x80 / 4] + 3u * (6u - local[0xd0 / 4]) + 2u * local[0x90 / 4]),
         c = 6u * d + 4u * (s + q);
    set_page_word(f, page, 0x48, b);
    set_page_word(f, page, 0x44, a);
    set_page_word(f, page, 0x4c, c); // 436E16/23/2B
    switch (source[1])
    {
    case 1:
        a += 15;
        b += 40;
        c += 5;
        set_page_word(f, page, 0x44, a);
        set_page_word(f, page, 0x48, b);
        set_page_word(f, page, 0x4c, c);
        break;
    case 2:
        a += 40;
        b += 20;
        c += 10;
        set_page_word(f, page, 0x44, a);
        set_page_word(f, page, 0x4c, c);
        set_page_word(f, page, 0x48, b);
        break;
    case 3:
        a += 25;
        b += 15;
        c += 30;
        set_page_word(f, page, 0x44, a);
        set_page_word(f, page, 0x4c, c);
        set_page_word(f, page, 0x48, b);
        break;
    default:
        break;
    }
}
void refresh_453680(FrontendState &f, W page, SourceMainPageServices &main)
{
    std::array<W, 59> config;
    for (W i = 0; i < 59; ++i)
        config[i] = source_word(f, 0x5f92f0 + i * 4u); // before callback
    const auto selected = main.menu_text_event(f, page_word(f, page, 0x14), 26, 0), delta = page_word(f, page, 0x1c);
    const auto category = source_word(f, 0x624dbc + (selected << 6));
    const auto level = source_word(f, 0x624dec + (selected << 6)) + delta;
    const auto at = 0x10u + (category << 4);
    if (at / 4 >= config.size())
        throw FormatError("4536C7 source stack configuration offset outside actual236 bytes");
    config[at / 4] = level;
    statistics_436d60(f, page, config);
}
W initialize_453480(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &main)
{
    for (W at : std::array<W, 7>{0x1c, 0x20, 0x24, 0x28, 0x18, 0x50, 0x54})
        set_page_word(f, page, at, at == 0x54 ? 224u : 0u);
    for (W i = 0; i < 3; ++i)
    {
        set_page_word(f, page, 0x2c + i * 4, 0);
        set_page_word(f, page, 0x38 + i * 4, 0);
    }
    W entry = 0x487dbc, row = 0x487c38;
    while (source_word(f, entry) != 0)
    {
        const auto category = source_word(f, entry); // 437A90 result is dead; leaf has no side effects.
        source_word(f, row) = 0xffffffffu;
        if (source_word(f, 0x5f9300 + (category << 4)) != 0)
        {
            const auto minimum = source_word(f, 0x5f9304 + (category << 4)),
                       maximum = source_word(f, 0x5f9308 + (category << 4));
            if (as_signed(minimum) < as_signed(maximum))
            {
                const auto count = page_word(f, page, 0x18);
                source_word(f, row) = count;
                for (W i = 0; i < 16; ++i)
                    source_word(f, 0x624db8 + (count << 6) + i * 4) = source_word(f, entry - 4 + i * 4);
                set_page_word(f, page, 0x18, count + 1u);
            }
        }
        entry += 64;
        row += 32;
    }
    const auto count = page_word(f, page, 0x18); // one retained signed loop count
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
    {
        const auto r = 0x624db8 + (i << 6), category = source_word(f, r + 4), base = category << 4;
        source_word(f, r + 0x34) = source_word(f, 0x5f9300 + base);
        const auto minimum = source_word(f, 0x5f9304 + base), maximum = source_word(f, 0x5f9308 + base);
        source_word(f, r + 0x38) = minimum;
        source_word(f, r + 0x3c) = maximum;
        source_word(f, r + 0x30) = integer(source_word(f, r + 0x34) - 1u);
        for (W k = 0; k < 5; ++k)
        {
            source_word(f, r + 8 + k * 4) = 0;
            source_word(f, r + 0x1c + k * 4) = 0;
        }
    }
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
    {
        const auto r = 0x624db8 + (i << 6), category = source_word(f, r + 4), material = material_437af0(category);
        const auto maximum = source_word(f, 0x5f9308 + (category << 4)) - 1u;
        for (W k = source_word(f, 0x5f9304 + (category << 4)) - 1u; as_signed(k) <= as_signed(maximum); ++k)
        {
            source_word(f, r + 8 + k * 4) = material + k;
            source_word(f, r + 0x1c + k * 4) = source_word(f, r) + k;
        }
    }
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    clear(f, child, 200);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4880f8);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4880b8, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    (void)main.menu_text_event(f, page_word(f, page, 0x14), 24, 0);
    refresh_453680(f, page, main);
    return 1;
}
W purchase_454150(FrontendState &f, W page, SourceMainPageServices &main)
{
    const auto r = main.menu_text_event(f, page_word(f, page, 0x14), 26, 0) << 6;
    const auto delta = page_word(f, page, 0x1c), car = source_word(f, 0x5f92ec), current = source_word(f, 0x624dec + r),
               category = source_word(f, 0x624dbc + r);
    const auto level = current + delta, kind = category << 4;
    const auto cost = price(f, category, level, 0x5f26b8 + kind + car * 236u);
    const auto previous = price(f, category, source_word(f, 0x624dec + r), 0x5f92fc + kind);
    const auto balance = source_word(f, 0x5f92e8) - cost + previous;
    if (as_signed(balance) < 0)
        return 0;
    source_word(f, 0x5f92e8) = balance;
    source_word(f, 0x5f92fc + kind) = 0;
    source_word(f, 0x5f9300 + kind) = level;
    source_word(f, 0x624dec + r) = level;
    return 1;
}
void tick_4538d3(FrontendState &f, W page, W event, SourceMainPageServices &main)
{
    const auto age = page_word(f, page, 0x50), child = page_word(f, page, 0x14);
    set_page_word(f, page, 0x50, age + 1u);
    (void)main.menu_text_event(f, child, event, 0);
    const auto selected = main.menu_text_event(f, child, 26, 0);
    const auto old_row = page_word(f, page, 0x24), target_row = integer(selected);
    set_page_word(f, page, 0x28, target_row);
    const auto next_row = approach(old_row, target_row);
    const auto delta = page_word(f, page, 0x1c), old_col = page_word(f, page, 0x20);
    set_page_word(f, page, 0x24, next_row);
    set_page_word(f, page, 0x20, approach(old_col, integer(delta)));
    const auto rows = integer(page_word(f, page, 0x18)), r = selected << 6;
    const auto minimum = source_word(f, 0x624df0 + r), current = source_word(f, 0x624dec + r),
               maximum = source_word(f, 0x624df4 + r);
    const std::array<W, 6> nav{
        integer(maximum - minimum + 1u), 0x3f800000, integer(current - minimum + delta), rows, 0x3f800000, target_row};
    for (W i = 0; i < 6; ++i)
        source_word(f, 0x5ff488 + i * 4) = nav[i];
    for (W i = 0; i < 3; ++i)
        set_page_word(f, page, 0x38 + i * 4, num(1, page_word(f, page, 0x44 + i * 4)));
    for (W i = 0; i < 3; ++i)
    {
        const auto target = page_word(f, page, 0x38 + i * 4), old = page_word(f, page, 0x2c + i * 4);
        set_page_word(f, page, 0x2c + i * 4, approach(old, target));
    }
    const auto count = page_word(f, page, 0x18);
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
    {
        const auto r0 = 0x624de8 + (i << 6), level = source_word(f, r0 + 4), old = source_word(f, r0);
        source_word(f, r0) = approach(old, integer(level - 1u));
    }
}
// Actual438230/4385D0 rectangle strip children. Descriptor is captured once;
// callbacks retain the same local48-byte record and source split UV endpoints.
void stretched(FrontendState &f, W layer, W id, W x, W y, W extent, W flags, bool vertical,
               SourceFrontendDrawServices &draw)
{
    if (id == 0 || !(as_float(extent) >= 0))
        return;
    if (id >= 387)
        throw FormatError("modify stretch descriptor outside actual atlas bank");
    std::array<W, 12> d;
    for (W i = 0; i < 12; ++i)
        d[i] = f.source.frontend_descriptors[id * 12u + i];
    d[0] = 2;
    d[11] = 255;
    d[10] = (flags & 0x100) ? 0x58u : (flags & 0x200) ? 0x70u : 0x20u;
    const W size = vertical ? 4u : 3u, axis = vertical ? 2u : 1u, uv = vertical ? 6u : 5u;
    const auto half = mul(d[size], 0x3f000000), end = d[uv + 2];
    d[1] = x;
    d[2] = y;
    if (flags & 1u)
        d[1] = vertical ? num(13, x, d[3]) : num(14, x, d[3], extent);
    else if (flags & 4u)
        d[1] = vertical ? sub(x, d[3]) : num(15, x, d[3], extent);
    if (flags & 2u)
        d[2] = vertical ? num(14, y, d[4], extent) : num(13, y, d[4]);
    else if (flags & 8u)
        d[2] = vertical ? num(15, y, d[4], extent) : sub(y, d[4]);
    d[uv + 2] = num(11, half, 0x40800000, d[uv]);
    d[size] = half;
    draw.rectangle_438370(f, d, layer, 0);
    d[axis] = add(half, d[axis]);
    if (as_float(extent) != 0)
    {
        d[uv] = d[uv + 2];
        d[size] = extent;
        draw.rectangle_438370(f, d, layer, 0);
        d[axis] = add(d[axis], extent);
    }
    d[size] = half;
    d[uv + 2] = end;
    draw.rectangle_438370(f, d, layer, 0);
}
void outline_453d20(FrontendState &f, W descriptor, const SourceMenuRect &r, SourceFrontendDrawServices &draw)
{
    const auto width = sub(r[2], r[0]), height = sub(r[3], r[1]), cx = num(12, r[0], r[2]), cy = num(12, r[1], r[3]);
    stretched(f, 17, descriptor, cx, r[1], width, 0x103, false, draw);
    stretched(f, 17, descriptor, cx, r[3], width, 0x103, false, draw);
    stretched(f, 17, descriptor, r[0], cy, height, 0x103, true, draw);
    stretched(f, 17, descriptor, r[2], cy, height, 0x103, true, draw);
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
void label(FrontendState &f, W x, W y, W id, W scale, W color, W flags, W style = 4)
{
    queue_frontend_text_438990(f.source, 12, 1, x, y, style, language(f, id), scale, color, flags, 0);
}
auto number(FrontendState &f, W value)
{
    if (as_signed(value) < 0)
        return std::span<const std::uint8_t>{f.source.source_bytes(0x479664, 4)};
    const auto b = format_campaign_value_43d5f0(value, bank<128>(f, 0x5ff328));
    return std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(b.data()), b.size()};
}
void amount(FrontendState &f, W y, W color, std::span<const std::uint8_t> text)
{
    f.world().ui.fonts.metrics().digit_advance_bits = 0x41600000;
    queue_frontend_text_438990(f.source, 12, 0, 0x43880000, y, 4, text, 0x3fa00000, color, 6, 0x41600000);
}
void bars_453a90(FrontendState &f, W page, SourceFrontendDrawServices &draw)
{
    for (W i = 0; i < 3; ++i)
    {
        const auto y = num(2, i);
        label(f, 0x43bd8000, y, i + 28, 0x3f800000, 0xffffffff, 6, 2);
        const auto width = num(3, page_word(f, page, 0x2c + i * 4)), descriptor = source_word(f, 0x4880e4 + i * 4);
        stretched(f, 12, descriptor, 0x43c58000, y, num(4, width), 0x102, false, draw);
    }
}
void grid_453b50(FrontendState &f, W page, W col_phase, W row_phase, SourceMainPageServices &main,
                 SourceTitlePageServices &title, SourceMenuPageDrawServices &panels, SourceFrontendDrawServices &draw)
{
    const auto selected = main.menu_text_event(f, page_word(f, page, 0x14), 26, 0);
    const auto delta = page_word(f, page, 0x1c), current = source_word(f, 0x624dec + (selected << 6));
    const auto x = num(5, col_phase), highlight = current + delta - 1u, count = page_word(f, page, 0x18);
    panels.fill_439380(f, 18, 5, {sub(x, 0x42766667), 0xc2766667, add(x, 0x42766667), 0x44076666}, 0x100, 0);
    panels.fill_439380(f, 18, 6, {sub(x, 0x44800000), 0xc2766667, x, 0x44076666}, 0x100, 0);
    panels.fill_439380(f, 18, 7, {x, 0xc2766667, add(x, 0x44800000), 0x44076666}, 0x100, 0);
    for (W row = 0; as_signed(row) < as_signed(count); ++row)
    {
        const auto r = 0x624dc0 + (row << 6), base = source_word(f, r + 0x28), y = num(6, row, row_phase);
        for (W col = 0; col < 5; ++col)
        {
            const auto px = num(7, col, col_phase, base);
            if (row == selected && col == highlight)
                outline_453d20(
                    f, 64, {sub(px, 0x42766667), sub(y, 0x42766667), add(px, 0x42766667), add(y, 0x42766667)}, draw);
            title.sprite_438710(f, 18, source_word(f, r + col * 4), {px, y, 0x3f19999a, 3});
        }
    }
}
void detail_453dd0(FrontendState &f, W page, SourceMainPageServices &main, SourceMenuPageDrawServices &panels)
{
    const auto selected = main.menu_text_event(f, page_word(f, page, 0x14), 26, 0), delta = page_word(f, page, 0x1c);
    set_page_word(f, page, 0x54, 224);
    if (as_signed(delta) > 0)
        set_page_word(f, page, 0x54, 225);
    if (as_signed(delta) < 0)
        set_page_word(f, page, 0x54, 226);
    const auto r = selected << 6, current = source_word(f, 0x624dec + r), category = source_word(f, 0x624dbc + r),
               index = current + delta - 1u;
    const SourceMenuRect rectangle{0x42400000, 0x43950000, 0x44140000, 0x43c20000};
    panels.frame_439450(f, 10, 110, rectangle, 64);
    panels.fill_439380(f, 13, 0, rectangle, 0, 0);
    const auto car = source_word(f, 0x5f92ec), kind = category << 4;
    const auto cost = price(f, category, source_word(f, 0x624dec + r) + delta, 0x5f26b8 + kind + car * 236u);
    const auto previous = price(f, category, source_word(f, 0x624dec + r), 0x5f92fc + kind);
    auto cash = source_word(f, 0x5f92e8);
    if (delta != 0)
        cash = cash - cost + previous;
    label(f, 0x43240000, 0x439d0000, 228, 0x3fa00000, 0xffffffff, 6);
    const auto color = 0xffc8c8ffu + (delta != 0 ? 0x36b781u : 0u);
    label(f, 0x432c0000, 0x439d0000, source_word(f, 0x624dd4 + (selected << 6) + index * 4), 0x3fa00000, color, 2);
    label(f, 0x43240000, 0x43a70000, 229, 0x3fa00000, 0xffffffff, 6);
    label(f, 0x43380000, 0x43a70000, 27, 0x3fa00000, color, 6);
    amount(f, 0x43a70000, color, number(f, cost));
    label(f, 0x43240000, 0x43b10000, 231, 0x3fa00000, 0xffffffff, 6);
    const auto remaining_color = 0xffc8c8ffu + (as_signed(cash) < 0 ? 0x36b781u : 0u);
    label(f, 0x43380000, 0x43b10000, 27, 0x3fa00000, remaining_color, 6);
    std::array<std::uint8_t, 32> local;
    std::size_t length = 0;
    if (as_signed(cash) < 0)
    {
        set_page_word(f, page, 0x54, 227);
        const auto text = number(f, 0u - cash);
        SourceScriptPathCall call;
        auto platform = f.shared_platform();
        const std::array<W, 1> arguments{0};
        format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(),
                              f.source.source_bytes(0x488104, 4), arguments, local, 0x45405e, call, {4, text});
        length = call.initialized_prefix;
    }
    else
    {
        const auto text = number(f, cash);
        const auto end = std::find(text.begin(), text.end(), std::uint8_t{0});
        if (end == text.end())
            throw FormatError("454076 numeric string has no source terminator");
        length = static_cast<std::size_t>(end - text.begin()) + 1u;
        if (length > local.size())
            throw FormatError("454085 numeric copy leaves actual32-byte source local");
        std::copy_n(text.begin(), length, local.begin());
    }
    amount(f, 0x43b10000, remaining_color, std::span<const std::uint8_t>{local}.first(length));
    label(f, 0x43240000, 0x43bb0000, 230, 0x3fa00000, 0xffffffff, 6);
    label(f, 0x43380000, 0x43bb0000, 27, 0x3fa00000, 0xffc8c8ff, 6);
    amount(f, 0x43bb0000, 0xffc8c8ff, number(f, previous));
}
} // namespace
W create_frontend_modify_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                     SourceMainPageServices &main)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 88);
    clear(f, page, 88);
    const auto event = source_word(f, 0x4880f4);
    set_page_word(f, page, 8, 0);
    const auto close = source_word(f, 0x4880f0);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 4, 5);
    set_page_word(f, page, 0, 0x4796b8);
    if (initialize_453480(f, page, pages, main) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_modify_page_4536f0(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = page_word(f, page, 0x14);
    if (page_word(f, child, 12) != 0x43c970)
        throw FormatError("4536F9 unimplemented MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), child);
    set_page_word(f, page, 0x14, 0);
}
W dispatch_frontend_modify_page_453710(FrontendState &f, W page, W event, SourceNavigationPageServices &pages,
                                       CampaignScoreMusic &music, SourceMainPageServices &main,
                                       SourceTitlePageServices &title, SourceMenuPageDrawServices &panels,
                                       SourceFrontendDrawServices &draw)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 2:
        tick_4538d3(f, page, event, main);
        break;
    case 3: {
        const auto row = page_word(f, page, 0x24), col = page_word(f, page, 0x20);
        grid_453b50(f, page, col, row, main, title, panels, draw);
        set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
        main.background_4392b0(f, 223, 0);
        title.text_438ff0(f, 2, 4,
                          {0x43a00000, 0x43d20000, page_word(f, page, 0x54), 0x3fa00000, 0xffffffff, 4, 0, 0x3f800000,
                           0x41, 0xfffffe70, 0});
        bars_453a90(f, page, draw);
        detail_453dd0(f, page, main, panels);
        break;
    }
    case 11:
    case 12: {
        const auto child = page_word(f, page, 0x14), selected = main.menu_text_event(f, child, 26, 0);
        if (event == 11 ? as_signed(selected) <= 0 : as_signed(selected) >= as_signed(page_word(f, page, 0x18) - 1u))
            break;
        pages.feedback_440880(f, 20);
        (void)main.menu_text_event(f, child, event, 0);
        set_page_word(f, page, 0x1c, 0);
        refresh_453680(f, page, main);
        set_page_word(f, page, 0x50, 0);
        break;
    }
    case 13:
    case 14: {
        const auto r = main.menu_text_event(f, page_word(f, page, 0x14), 26, 0) << 6, delta = page_word(f, page, 0x1c);
        const auto candidate = source_word(f, 0x624dec + r) + delta + (event == 13 ? 0xffffffffu : 1u);
        if (event == 13 ? as_signed(candidate) < as_signed(source_word(f, 0x624df0 + r))
                        : as_signed(candidate) > as_signed(source_word(f, 0x624df4 + r)))
            break;
        pages.feedback_440880(f, 20);
        main.feedback_440890(f, event);
        set_page_word(f, page, 0x1c, delta + (event == 13 ? 0xffffffffu : 1u));
        refresh_453680(f, page, main);
        set_page_word(f, page, 0x50, 0);
        break;
    }
    case 16:
        if (page_word(f, page, 0x1c) == 0 || purchase_454150(f, page, main) == 0)
        {
            pages.feedback_440880(f, 9);
            break;
        }
        set_page_word(f, page, 0x1c, 0);
        refresh_453680(f, page, main);
        main.feedback_440890(f, event);
        set_page_word(f, page, 0x50, 0);
        break;
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            main.feedback_440890(f, event);
        break;
    case 19:
        source_word(f, 0x5ff46c) = 2;
        set_frontend_packed_word_43dd10(f.source, 0x534527);
        source_word(f, 0x5ff448) = 105;
        source_word(f, 0x5ff44c) = 0xbe99999a;
        source_word(f, 0x5ff450) = 0x3f000000;
        [[fallthrough]];
    case 20:
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
