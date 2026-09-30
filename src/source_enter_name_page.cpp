#include "sgn/source_enter_name_page.hpp"
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_menu_page_draw.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
extern "C" void sgn_enter_name_fraction(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_enter_name_linear_43d8f0(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_enter_name_integer_float(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_enter_name_layout(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_enter_name_cell(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_enter_name_edit_panel(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_enter_name_player_x(const std::uint32_t *, std::uint32_t *) noexcept;
namespace
{
using W = std::uint32_t;
using sgn::retail::frontend_page_words::source_word;
W read(FrontendState &f, W a)
{
    return persistence_word(f.source.source_bytes(a, 4), 0);
}
void write(FrontendState &f, W a, W v)
{
    persistence_word(f.source.source_bytes(a, 4), 0, v);
}
W get(FrontendState &f, W p, W o)
{
    return read(f, p + o);
}
void put(FrontendState &f, W p, W o, W v)
{
    write(f, p + o, v);
}
std::uint8_t byte(FrontendState &f, W a)
{
    return f.source.source_bytes(a, 1)[0];
}
void byte(FrontendState &f, W a, std::uint8_t v)
{
    f.source.source_bytes(a, 1)[0] = v;
}
using sgn::bit_reinterpret::as_signed;
W length(FrontendState &f, W p)
{
    // Actual REPNE SCASB from object+48. Read guards apply only to the reached
    // byte in the real pool; do not truncate or manufacture a fourth NUL.
    W n = 0;
    while (byte(f, p + 0x48u + n) != 0)
        ++n;
    return n;
}
W int_float(W v)
{
    W out;
    sgn_enter_name_integer_float(&v, &out);
    return out;
}
W linear(W current, W target, W step)
{
    const std::array<W, 3> in{current, target, step};
    W out;
    sgn_enter_name_linear_43d8f0(in.data(), &out);
    return out;
}
W approach(W current, W target)
{
    return std::bit_cast<W>(approach_frontend_value_43d810(std::bit_cast<float>(current), std::bit_cast<float>(target),
                                                           std::bit_cast<float>(W{0x3a83126f})));
}
// Full4547E0. Pointer/row reads stay dynamic; result-1 leaves both outputs alone.
W find_character(FrontendState &f, std::uint8_t character, W &x, W &y)
{
    for (W row = 0; row < 4u; ++row)
    {
        const auto pointer = source_word(f, 0x4881b0u + row * 4u);
        for (W column = 0; column < 8u; ++column)
        {
            if (byte(f, pointer + column) == character)
            {
                x = column;
                y = row;
                return column + row * 8u;
            }
        }
    }
    return 0xffffffffu;
}
void append(FrontendState &f, W p, std::uint8_t character)
{ // 454AF0
    const auto n = length(f, p);
    if (as_signed(n) < 3)
    {
        byte(f, p + 0x48u + n, character);
        byte(f, p + 0x49u + n, 0);
        put(f, p, 0x14, 0);
    }
}
void erase(FrontendState &f, W p)
{ // 454B20, no tick reset
    const auto n = length(f, p);
    if (as_signed(n) > 0)
        byte(f, p + 0x47u + n, 0);
}
void cursor(FrontendState &f, W p, W x, W y)
{ // 454B40, original publication order
    const auto fx = int_float(x);
    put(f, p, 0x1c, x);
    put(f, p, 0x20, y);
    put(f, p, 0x24, fx);
    const auto fy = int_float(y);
    put(f, p, 0x28, fy);
    put(f, p, 0x2c, fx);
    put(f, p, 0x30, fy);
}
bool admissible(FrontendState &f, W p, W x, W y)
{ // 454C80
    if (as_signed(x) < 0 || as_signed(y) < 0 || as_signed(x) >= 8 || as_signed(y) >= 4)
        return false;
    const auto n = length(f, p);
    if (as_signed(n) >= 3)
        return as_signed(x) >= 6 && y == 3;
    return !(x == 7 && y == 3);
}
void activate(FrontendState &f, W p, SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{ // 454B70
    if (get(f, p, 0x4c) != 0)
        return;
    const auto x = get(f, p, 0x1c);
    put(f, p, 0x4c, 1); // retained EDX before gate store
    if (x == 7 && get(f, p, 0x20) == 3)
    {
        if (length(f, p) < 3u)
        {
            pages.feedback_440880(f, 9);
            return;
        }
        const auto player = get(f, p, 0x18);
        const auto count = length(f, p) + 1u; // second source SCASB, includes NUL
        const auto destination = 0x5e3a34u + (player << 6u);
        W at = 0;
        for (W words = count >> 2u; words != 0; --words, at += 4u)
        {
            const auto value = read(f, p + 0x48u + at);
            write(f, destination + at, value);
        }
        for (W bytes = count & 3u; bytes != 0; --bytes, ++at)
        {
            const auto value = byte(f, p + 0x48u + at);
            byte(f, destination + at, value);
        }
        pages.feedback_440880(f, 3);
        (void)pop_frontend_page_43eef0(f, pages, music);
        return;
    }
    if (x == 6 && get(f, p, 0x20) == 3)
    {
        if (length(f, p) != 0)
        {
            pages.feedback_440880(f, 5);
            erase(f, p);
        }
        return;
    }
    if (length(f, p) < 3u)
    {
        const auto row = get(f, p, 0x20), pointer = source_word(f, 0x4881b0u + row * 4u);
        append(f, p, byte(f, pointer + x));
        pages.feedback_440880(f, 4);
    }
    if (length(f, p) == 3u)
        cursor(f, p, 7, 3);
}
void character_event(FrontendState &f, W p, W argument, SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    auto character = static_cast<std::uint8_t>(argument);
    if (character >= 0x61u && character <= 0x7au)
        character = static_cast<std::uint8_t>(character - 0x20u); // full43D940
    if (character == 13u)
    {
        activate(f, p, pages, music);
        return;
    }
    if (character == 8u && length(f, p) != 0)
    {
        erase(f, p);
        pages.feedback_440880(f, 5);
        const auto row = get(f, p, 0x20), column = get(f, p, 0x1c);
        if (!admissible(f, p, column, row))
            cursor(f, p, 6, 3);
        return;
    }
    W x, y;
    const auto found = find_character(f, character, x, y);
    if (as_signed(found) < 0)
        return;
    if (length(f, p) >= 3u)
        return;
    const auto pointer = source_word(f, 0x4881b0u + y * 4u); // reread after4547E0 and SCASB
    append(f, p, byte(f, pointer + x));
    pages.feedback_440880(f, 4);
    if (length(f, p) < 3u)
        cursor(f, p, 6, 3);
    else
        cursor(f, p, 7, 3);
}
void tick(FrontendState &f, W p)
{ // 4546AD..454758
    const auto old_phase = get(f, p, 0x44);
    W phase;
    sgn_enter_name_fraction(&old_phase, &phase);
    put(f, p, 0x44, phase);
    const auto target = get(f, p, 0x38), current = get(f, p, 0x34);
    put(f, p, 0x34, approach(current, target));
    const auto column = get(f, p, 0x1c), old_x = get(f, p, 0x24);
    const auto fx = int_float(column);
    put(f, p, 0x2c, fx);
    const auto fy = int_float(get(f, p, 0x20));
    put(f, p, 0x30, fy);
    const auto next_x = linear(old_x, fx, 0x3e4ccccd);
    const auto old_y = get(f, p, 0x28);
    put(f, p, 0x24, next_x);
    put(f, p, 0x28, linear(old_y, fy, 0x3e4ccccd));
    const auto n = length(f, p), old_length = get(f, p, 0x3c);
    const auto target_length = int_float(n);
    put(f, p, 0x40, target_length);
    const auto next_length = linear(old_length, target_length, 0x3ecccccd);
    const auto old_tick = get(f, p, 0x14);
    put(f, p, 0x3c, next_length);
    put(f, p, 0x14, old_tick + 1u);
}
W coordinate(W fraction, W base)
{
    const std::array<W, 2> in{fraction, base};
    W out;
    sgn_enter_name_cell(in.data(), &out);
    return out;
}
void mark(FrontendState &f, W descriptor, W x, W y, SourceTitlePageServices &draw)
{ // full454AC0
    draw.sprite_438710(f, 2, descriptor, {x, y, 0x3f800000, 0x103});
}
void draw_name(FrontendState &f, W p, W x, W y, W slide, SourceTitlePageServices &draw, SourceMenuPageDrawServices &ui)
{
    const std::array<W, 2> input{x, y};
    std::array<W, 6> layout;
    sgn_enter_name_layout(input.data(), layout.data());
    // layout={left,top,frame-left,frame-top,frame-right,frame-bottom}; original spills.
    ui.frame_439450(f, 10, 110, {layout[2], layout[3], layout[4], layout[5]}, 0x40);
    for (W row = 0; row < 4u; ++row)
    {
        for (W column = 0; column < 8u; ++column)
        {
            const auto sy = coordinate(int_float(row), layout[1]);
            const auto sx = coordinate(int_float(column), layout[0]);
            draw.sprite_438710(f, 12, 0x136u + row * 8u + column, {sx, sy, 0x3f800000, 3});
        }
    }
    const auto cy = coordinate(get(f, p, 0x28), layout[1]);
    const auto cx = coordinate(get(f, p, 0x24), layout[0]);
    mark(f, 108, cx, cy, draw);
    const std::array<W, 2> panel_in{slide, layout[5]};
    std::array<W, 3> panel;
    sgn_enter_name_edit_panel(panel_in.data(), panel.data()); // y/top/bottom, retains pre-spill x87 value
    ui.frame_439450(f, 10, 111, {0x43400000, panel[1], 0x43e00000, panel[2]}, 0x40);
    ui.fill_439380(f, 13, 0, {0x43400000, panel[1], 0x43e00000, panel[2]}, 0, 0);
    auto character = byte(f, p + 0x48u);
    W count = 0;
    while (character != 0)
    {
        W found_x, found_y;
        const auto glyph = find_character(f, character, found_x, found_y);
        const auto sx = coordinate(int_float(count), 0x43600000);
        draw.sprite_438710(f, 12, glyph + 0x136u, {sx, panel[0], 0x3f800000, 3});
        character = byte(f, p + 0x49u + count);
        ++count; // fresh byte after draw callback
    }
    const auto row = get(f, p, 0x20), pointer = source_word(f, 0x4881b0u + row * 4u);
    const auto column = get(f, p, 0x1c);
    W found_x, found_y;
    const auto glyph = find_character(f, byte(f, pointer + column), found_x, found_y);
    const auto sx = coordinate(int_float(count), 0x43600000);
    draw.sprite_438710(f, 12, glyph + 0x136u, {sx, panel[0], 0x3f800000, 3});
    mark(f, 109, coordinate(get(f, p, 0x3c), 0x43600000), panel[0], draw);
    if (as_signed(source_word(f, 0x5e379c)) > 1)
    {
        const auto phase = get(f, p, 0x44), player = get(f, p, 0x18);
        W px;
        sgn_enter_name_player_x(&phase, &px);
        draw.sprite_438710(f, 13, player + 0x35u, {px, panel[0], 0x3f800000, 0x203});
        draw.sprite_438710(f, 12, player + 0x31u, {px, panel[0], 0x3f800000, 0x103});
    }
}
} // namespace
std::uint32_t create_frontend_enter_name_page_43e000(FrontendState &f, W key)
{
    f.shared_platform()->require_owner_thread();
    const auto pool = std::as_writable_bytes(std::span{f.source.page_pool});
    const auto p = allocate_frontend_page_object_43d470(pool, 80);
    const auto close = source_word(f, 0x4881c0); // 43E484 captured BEFORE clearing object
    for (W o = 0; o < 80u; o += 4u)
        put(f, p, o, 0);
    put(f, p, 8, 0);
    const auto event = source_word(f, 0x4881c4);
    put(f, p, 12, close);
    put(f, p, 4, 7);
    put(f, p, 0, 0x47971c);
    put(f, p, 16, event);
    // Full454410. Player is captured before any initializer store.
    const auto player = source_word(f, 0x5e3798);
    for (const W o : {0x1cu, 0x20u, 0x24u, 0x28u, 0x2cu, 0x30u, 0x3cu, 0x40u, 0x44u, 0x34u, 0x38u, 0x14u, 0x4cu})
        put(f, p, o, 0);
    source_word(f, 0x5e3a30) = 0;
    byte(f, p + 0x48u, 0);
    put(f, p, 0x18, player);
    put(f, p, 8, key);
    return p; // source init returns1; common43ED94 store
}
void destroy_frontend_enter_name_page_454450(FrontendState &, W)
{ /*full source RET*/
}
std::uint32_t dispatch_frontend_enter_name_page_454460(FrontendState &f, W p, W event, W argument,
                                                       SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                                       SourceMainPageServices &main, SourceTitlePageServices &draw,
                                                       SourceMenuPageDrawServices &ui)
{
    f.shared_platform()->require_owner_thread();
    switch (event)
    {
    case 1:
        put(f, p, 0x4c, 0);
        break;
    case 2:
        tick(f, p);
        break;
    case 3: {
        main.background_4392b0(f, 0x183, 0);
        const auto slide = get(f, p, 0x34);
        draw_name(f, p, 0x43a00000, 0x43580000, slide, draw, ui);
        break;
    }
    case 11:
    case 12:
    case 13:
    case 14: {
        W x, y;
        if (event == 11)
        {
            y = get(f, p, 0x20) - 1u;
            x = get(f, p, 0x1c);
        }
        else if (event == 12)
        {
            y = get(f, p, 0x20) + 1u;
            x = get(f, p, 0x1c);
        }
        else
        {
            y = get(f, p, 0x20);
            x = get(f, p, 0x1c) + (event == 13 ? 0xffffffffu : 1u);
        }
        if (admissible(f, p, x, y))
        {
            const W offset = (event == 11 || event == 12) ? 0x20u : 0x1cu;
            put(f, p, offset, get(f, p, offset) + ((event == 11 || event == 13) ? 0xffffffffu : 1u));
            main.feedback_440890(f, event);
        }
        break;
    }
    case 16:
        activate(f, p, pages, music);
        break;
    case 17:
        main.feedback_440890(f, event);
        source_word(f, 0x5e3a30) = 1;
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 18:
        character_event(f, p, argument, pages, music);
        break;
    case 19:
        source_word(f, 0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        source_word(f, 0x5ff448) = 96;
        source_word(f, 0x5ff44c) = 0xbe99999a;
        source_word(f, 0x5ff450) = 0xbf000000;
        put(f, p, 0x38, 0x3f800000);
        break;
    case 20:
        put(f, p, 0x38, 0);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
