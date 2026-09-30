#include "sgn/source_save_page_draw.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
#include <limits>
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
W word(FrontendState &f, W a)
{
    return f.source.source_word(a);
}
using sgn::retail::frontend_page_words::page_word;
auto page_text(FrontendState &f, W p, W o, W length)
{
    // NEW host bounds before addition or pointer arithmetic. A corrupted source
    // row/count must not wrap an oversized tail back to a512-byte owner view.
    if (o > 0x200u || length > 0x200u - o)
        throw FormatError("save row text leaves its existing page owner");
    const auto b = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), p, o + length);
    return std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t *>(b.data() + o), length};
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
W divide(W a, W b)
{
    if (b == 0 || (a == 0x80000000u && b == 0xffffffffu))
        throw FormatError("save drawing source IDIV fault");
    return std::bit_cast<W>(as_signed(a) / as_signed(b));
}
W blocks(W bytes, W unit)
{
    return divide(bytes - 1u + unit, unit);
} // 4586E0, DWORD wrap before signed IDIV
bool usable(FrontendState &f, W p)
{
    const auto n = page_word(f, p, 0x1e4); // 4586F0 captures ESI, not a live count loop
    for (W i = 0; as_signed(i) < as_signed(n); ++i)
    {
        const auto s = as_signed(page_word(f, p, 0x6cu + i * 48u));
        if (s == 1 || (s > 3 && s <= 5))
            return true;
    }
    return false;
}
void raw_text(FrontendState &f, W x, W y, std::span<const std::uint8_t> bytes, W color, W flags)
{
    queue_frontend_text_438990(f.source, 12, 0, x, y, 4, bytes, 0x3fc00000, color, flags, 0);
}
void label(FrontendState &f, W x, W y, W id, W color, W flags)
{
    queue_frontend_text_438990(f.source, 12, 1, x, y, 4, language(f, id), 0x3fc00000, color, flags, 0);
}
std::span<std::uint8_t> formatted_number(FrontendState &f, W id, W value, W site)
{
    auto out = f.source.source_bytes(0x627a54, 256);
    const std::array<W, 1> args{value};
    SourceScriptPathCall call;
    const auto platform = f.shared_platform();
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), language(f, id), args,
                          out, site, call);
    return out.first(call.initialized_prefix);
}
} // namespace
std::span<const std::uint8_t> format_source_time_43d550(FrontendState &f, W ticks)
{
    const auto t = as_signed(ticks * 100u) / 60;
    const std::array<W, 3> args{W((t / 6000) % 60), W((t / 100) % 60), W(t % 100)};
    constexpr std::array<std::uint8_t, 13> fmt{'%', 'd', ':', '%', '0', '2', 'd', ':', '%', '0', '2', 'd', 0};
    auto out = f.source.source_bytes(0x5ff3a8, 64);
    SourceScriptPathCall call;
    const auto platform = f.shared_platform();
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), fmt, args, out,
                          0x43d5be, call);
    return out.first(call.initialized_prefix);
}
std::uint32_t read_source_replay_time_45811b(FrontendState &f, std::uint32_t identity)
{
    const auto &platform = f.shared_platform();
    if (!platform)
        throw FormatError("45811B requires the retained frontend platform");
    platform->require_owner_thread();
    if (&f.world() != &platform->state())
        throw FormatError("45811B frontend and platform name different worlds");
    // NEW host admission bound: the pinned pointer publications to5E3C30 name
    // these two static buffers. Do not reread5E3C30 after the caller's458115 capture.
    if (identity != 0x5e3c48u && identity != 0x5eb178u)
        throw FormatError("45811B replay identity has no reconstructed frontend owner");
    return persistence_word(f.source.source_bytes(identity + 0x1cu, 4), 0);
}
void SourceSavePageDrawChildren::draw_cards_4581b0(FrontendState &f, W p)
{
    f.shared_platform()->require_owner_thread();
    const auto count = page_word(f, p, 0x1e4), rows = count == 0 ? 1u : count;
    const double height = double(as_signed(rows)) * 12.0 + 8.0;
    const auto top = st(195.0 - height), bottom = st(height + 195.0);
    const SourceMenuRect r{0x42200000, top, 0x44160000, bottom};
    frame_439450(f, 10, 110, r, 0x40);
    fill_439380(f, 13, 2, r, 0, 0);
    const auto center = st((double(as_float(bottom)) + double(as_float(top))) * 0.5);
    auto y = st(double(as_float(center)) - double(as_signed(count - 1u)) * 12.0);
    const auto initial_mode = word(f, 0x5e3c28);
    if (initial_mode - 1u > 4u)
        return;
    const W base = initial_mode <= 3 ? 0x247u : 0x24du;
    for (W i = 0; as_signed(i) < as_signed(count); ++i)
    {
        const auto row = i * 48u;
        const W color = i == page_word(f, p, 0x20) ? 0xffff8080u : 0xffffffffu;
        // Actual row name stores a language identity, not a host or guest pointer.
        const auto name = resolve_frontend_language_text(f.source.language_strings(),
                                                         FrontendLanguageTextIdentity{page_word(f, p, 0x90 + row)});
        raw_text(f, 0x42f00000, y, name, color, 6);
        label(f, 0x43080000, y, page_word(f, p, 0x6c + row) + base, color, 2);
        const auto state = page_word(f, p, 0x6c + row);
        if (state != 1 && state != 2)
        {
            if (state == 3)
            {
                const auto mode = word(f, 0x5e3c28);
                if (mode - 1u > 4u)
                    return;
                const auto quotient = divide(page_word(f, p, 0x68 + row), page_word(f, p, 0x18));
                if (mode == 1 || mode == 3 || mode == 5)
                {
                    // 440310/4405B0 are MOV EAX,ECX;RET. The prior PUSH quotient is discarded
                    // by the caller, but its IDIV must still occur before these identity leaves.
                    (void)quotient;
                    const auto bytes = mode == 5 ? word(f, 0x5e3c34) : 0x211cu;
                    (void)formatted_number(f, 0x254, blocks(bytes, page_word(f, p, 0x18)), 0x458379);
                }
                else
                    (void)formatted_number(f, 0x253, quotient, 0x45839b);
            }
            else
                (void)formatted_number(f, 0x253, divide(page_word(f, p, 0x68 + row), page_word(f, p, 0x18)), 0x45839b);
            if (page_word(f, p, 0x6c + row) == 5)
            {
                const auto mode = as_signed(word(f, 0x5e3c28));
                if (mode >= 4 && mode <= 5)
                    raw_text(f, 0x43cc0000, y, page_text(f, p, 0x70 + row, 0x200u - (0x70u + row)), color, 6);
            }
        }
        y = st(double(as_float(y)) + 24.0);
    }
    auto pulse = (page_word(f, p, 0x1c) - 32u) & 63u;
    if (pulse >= 32)
        pulse = 64u - pulse;
    // The highlight center has NO source FSTP32 before +/-16 stack arguments.
    const double current = double(as_float(page_word(f, p, 0x28))) * 24.0 +
                           (double(as_float(center)) - double(as_signed(rows - 1u)) * 12.0);
    fill_439380(f, 11, 4, {0x42200000, st(current - 16), 0x44160000, st(current + 16)}, 0x400,
                st(double(as_signed(pulse + 32u)) * 0.015625));
    if (page_word(f, p, 0x1f0) != 0 || page_word(f, p, 0x1e8) != 0 || page_word(f, p, 0x1ec) != 0)
        return;
    double phase = double(as_float(page_word(f, p, 0x30)));
    if (phase > 1.0)
        phase = 2.0 - phase;
    const double shift = (1.0 - phase) * 120.0;
    const auto lower_top = st(shift + 332.0), lower_bottom = st(shift + 432.0);
    const SourceMenuRect lower{0x42200000, lower_top, 0x44160000, lower_bottom};
    frame_439450(f, 10, 111, lower, 0x40);
    fill_439380(f, 13, 2, lower, 0x100, 0);
    const auto lower_y = st((double(as_float(lower_bottom)) + double(as_float(lower_top))) * 0.5);
    const auto mode = word(f, 0x5e3c28);
    if (mode - 1u > 4u)
        return;
    const bool any = usable(f, p);
    const auto state = any ? page_word(f, p, 0x6cu + page_word(f, p, 0x20) * 48u) : 0u;
    const auto id = 0x20du + (mode - 1u) * 7u + state;
    const auto bytes = mode <= 3 ? 0x211cu : word(f, 0x5e3c34);
    const auto output = formatted_number(f, id, blocks(bytes, page_word(f, p, 0x18)), 0x45866f);
    (void)draw_frontend_aligned_borrowed_438ab0(f, {12, 4, 0x43a00000, lower_y, {}, {0x3fc00000, 0xffffffff, 0x803}},
                                                output);
}
void SourceSavePageDrawChildren::draw_progress_458730(FrontendState &f, W p)
{
    W id;
    switch (page_word(f, p, 0x1ec))
    {
    case 1:
        id = 0x245;
        break;
    case 2:
    case 4:
        id = 0x244;
        break;
    case 3:
    case 5:
        id = 0x243;
        break;
    case 6:
        id = 0x242;
        break;
    default:
        return;
    }
    const SourceMenuRect r{0x42200000, 0x43a60000, 0x44160000, 0x43d80000};
    frame_439450(f, 10, 111, r, 0x40);
    fill_439380(f, 13, 2, r, 0x100, 0);
    draw_frontend_text_438ba0(f, 12, 4, {0x43a00000, 0x43bf0000, id, 0x3fc00000, 0xffff8080, 3}, *this);
}
void SourceSavePageDrawChildren::draw_confirmation_459070(FrontendState &f, W p, W id)
{
    const SourceMenuRect right{0x43dc0000, 0x43a60000, 0x44160000, 0x43d80000};
    frame_439450(f, 10, 110, right, 0x40);
    fill_439380(f, 13, 2, right, 0, 0);
    const SourceMenuRect left{0x42200000, 0x43a60000, 0x43cc0000, 0x43d80000};
    const auto descriptor = (word(f, 0x5ff45c) & 32u) != 0 ? 111u : 112u;
    frame_439450(f, 10, descriptor, left, 0x40);
    fill_439380(f, 13, 2, left, 0x100, 0);
    draw_frontend_text_438ba0(f, 12, 4, {0x43600000, 0x43bf0000, id, 0x3fc00000, 0xffffffff, 0x803}, *this);
    (void)menu_text_event(f, page_word(f, p, 0x40), 3, 0);
}
void SourceSavePageDrawChildren::replay_background_4580fc(FrontendState &f, W mode)
{
    auto platform = f.shared_platform();
    W title;
    std::span<const std::uint8_t> argument;
    if (mode == 4)
    {
        const auto id = frontend_language_text_441170(f.source.language_strings(), word(f, 0x5e3c2c) + 0x1b7u);
        title = 0x221;
        if (id.value == 0)
        {
            draw_frontend_main_background_4392b0(f, title, 0, *this);
            return;
        }
        argument = resolve_frontend_language_text(f.source.language_strings(), id);
    }
    else if (mode == 5)
    {
        const auto ticks = replay_time_word_45811b(f, word(f, 0x5e3c30));
        const auto time = format_source_time_43d550(f, ticks);
        const auto car_id = frontend_language_text_441170(f.source.language_strings(), word(f, 0x5e3c2c) + 0x1b7u);
        const auto car = resolve_frontend_language_text(f.source.language_strings(), car_id);
        auto out = f.source.source_bytes(0x627bd4, 128);
        SourceScriptPathCall call;
        const std::array<W, 2> args{0, 0};
        format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(),
                              f.source.source_bytes(0x488cac, 8), args, out, 0x458145, call,
                              car_id.value != 0 ? SourceSaveStringBorrow{4, car} : SourceSaveStringBorrow{8, time},
                              car_id.value != 0 ? SourceSaveStringBorrow{8, time} : SourceSaveStringBorrow{});
        argument = out.first(call.initialized_prefix);
        title = 0x228;
    }
    else
        throw FormatError("4580FC background is only source modes4/5");
    // 4392B0's two zero coordinates ->4392C0 (320,50), nonzero string argument.
    auto output = f.source.source_bytes(0x5df414, 256);
    SourceScriptPathCall call;
    const std::array<W, 1> args{0};
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), language(f, title),
                          args, output, 0x4392f3, call, {4, argument});
    draw_frontend_box_borrowed_439040(
        f, {2, 4, 0x43a00000, 0x42480000, {}, {0x3fa00000, 0xffffffff, 3, 0, 0x3f800000, 0x41, 0xffffff10, 0}},
        output.first(call.initialized_prefix));
}
} // namespace sgn::retail
