#include "sgn/source_records_pages.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/source_save_page_draw.hpp"
#include "sgn/source_menu_text.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <array>
#include <bit>
#include <cmath>

extern "C" std::uint32_t sgn_menu_option_fild_fstp(std::uint32_t) noexcept;
extern "C" void sgn_records_first_y_45955f(const std::uint32_t *, std::uint32_t *) noexcept;
extern "C" void sgn_records_wave_4595e8(const std::uint32_t *, std::uint32_t *) noexcept;

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
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
using sgn::retail::frontend_page_words::source_word;
W read_record(FrontendState &f, W a)
{
    return persistence_word(f.source.source_bytes(a, 4), 0);
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
// Source call43D880 differs from43D810 in its factor0.05, PE46E27C.
// Same PC53, round-nearest value contract as the existing43D810 API.
W approach_43d880(W current, W target)
{
    const float difference = vehicle_f32(double(as_float(target)) - double(as_float(current)));
    const float magnitude = (!(difference >= 0.0f)) ? -difference : difference;
    if (magnitude == 0.0f || std::isnan(magnitude))
        return current;
    volatile double product = double(magnitude) * double(as_float(0x3d4ccccd));
    double step = product + double(as_float(0x3a83126f));
    if (!(double(magnitude) > step))
        return target;
    if (!(as_float(target) >= as_float(current)))
        step = -step;
    return st(step + double(as_float(current)));
}
W approach_43d810(W current, W target)
{
    return std::bit_cast<W>(approach_frontend_value_43d810(as_float(current), as_float(target), as_float(0x3a83126f)));
}
void child(FrontendState &f, W page, W descriptor, W name, SourceNavigationPageServices &pages,
           SourceMainPageServices &main)
{
    const auto c = allocate_frontend_page_object_43d470(page_pool(f), 200);
    for (W o = 0; o < 200; o += 4)
        set_page_word(f, c, o, 0);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, c, 12, close);
    set_page_word(f, c, 16, event);
    set_page_word(f, c, 4, 41);
    set_page_word(f, c, 0, name);
    set_page_word(f, c, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, c, descriptor, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), c);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, c);
    const auto retained = page_word(f, page, 0x14);
    (void)main.menu_text_event(f, retained, 24, 0);
    (void)main.menu_text_event(f, retained, 22, 0);
}
void track_rows_456fc0(FrontendState &f)
{
    std::array<W, 64> ids;
    W count = 0;
    for (W id = 1; id < 25; ++id)
        if (source_track_available_4371c0(f, id, 3) != 0)
            ids[count++] = id;
    constexpr W ranks[]{19, 1, 16, 7, 18, 17, 21, 8, 12, 13, 14, 5, 6, 24, 20, 15, 23, 9, 10, 2, 22, 11, 3, 4};
    const auto rank = [&](W id) { return id - 1u < 24u ? ranks[id - 1u] : 0u; };
    for (W remaining = count - 1u; as_signed(remaining) >= 0; --remaining)
        for (W i = 0; i < remaining; ++i)
        {
            const auto next = ids[i + 1], current = ids[i];
            if (as_signed(rank(current)) > as_signed(rank(next)))
            {
                ids[i] = next;
                ids[i + 1] = current;
            }
        }
    for (W i = 0; i < count; ++i)
    {
        const auto row = 0x625138u + i * 32u;
        for (W o = 0; o < 32; o += 4)
            source_word(f, row + o) = source_word(f, 0x488920u + o);
        const auto track = ids[i];
        source_word(f, row) = track;
        source_word(f, row + 12) = track + 0x1b7u;
        source_word(f, row + 16) = 0;
    }
    for (W o = 0; o < 32; o += 4)
        source_word(f, 0x625138u + count * 32u + o) = source_word(f, 0x488940u + o);
}
void high_score_init_459200(FrontendState &f, W page, SourceNavigationPageServices &pages, SourceMainPageServices &main)
{
    child(f, page, 0x488cf8, 0x488d30, pages, main);
    set_page_word(f, page, 0x2c, 0x3d4ccccd);
    for (W i = 0; i < 5; ++i)
        set_page_word(f, page, 0x38u + i * 4u, source_word(f, 0x5e3bf4u + i * 4u));
    const auto count = page_word(f, page, 0x44);
    set_page_word(f, page, 0x1c, 0);
    set_page_word(f, page, 0x20, 0);
    set_page_word(f, page, 0x24, 0);
    set_page_word(f, page, 0x28, 0);
    set_page_word(f, page, 0x30, 0x43480000);
    set_page_word(f, page, 0x34, 0x3f800000);
    set_page_word(f, page, 0x18, 0);
    if (as_signed(count) <= 10)
        return;
    W i = 0;
    if (as_signed(count) > 0)
    {
        W address = page_word(f, page, 0x48) + 7u;
        while (as_signed(i) < as_signed(count))
        {
            if (f.source.source_bytes(address, 1)[0] != 0)
            {
                set_page_word(f, page, 0x1c, i - 4u);
                break;
            }
            ++i;
            address += 8u;
        }
    }
    const auto first = page_word(f, page, 0x1c), last = count - 10u;
    if (as_signed(first) < 0)
        set_page_word(f, page, 0x1c, 0);
    if (as_signed(page_word(f, page, 0x1c)) > as_signed(last))
        set_page_word(f, page, 0x1c, last);
}
void configure_scores(FrontendState &f, W title, W argument, W format, W count, W records)
{
    source_word(f, 0x5e3bf4) = title;
    source_word(f, 0x5e3bf8) = argument;
    source_word(f, 0x5e3bfc) = format;
    source_word(f, 0x5e3c00) = count;
    source_word(f, 0x5e3c04) = records;
}
void appearance(FrontendState &f, W flag, W background, W x, W y)
{
    source_word(f, 0x5ff46c) = flag;
    set_frontend_packed_word_43dd10(f.source, 0x114311);
    source_word(f, 0x5ff448) = background;
    source_word(f, 0x5ff44c) = x;
    source_word(f, 0x5ff450) = y;
}
void footer_439340(FrontendState &f, W id, SourceTitlePageServices &draw)
{
    draw.text_438ff0(f, 2, 4,
                     {0x43a00000, 0x43d20000, id, 0x3fa00000, 0xffffffff, 4, 0, 0x3f800000, 0x41, 0xfffffe70, 0});
}
void records_background(FrontendState &f, W id, W argument, SourceMainPageServices &main)
{
    if (argument == 0)
    {
        main.background_4392b0(f, id, 0);
        return;
    }
    const auto text =
        resolve_frontend_language_text(f.source.language_strings(), FrontendLanguageTextIdentity{argument});
    auto out = f.source.source_bytes(0x5df414, 256);
    SourceScriptPathCall call;
    const std::array<W, 1> args{argument};
    const auto platform = f.shared_platform();
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), language(f, id), args,
                          out, 0x4392f3, call, {4, text});
    draw_frontend_box_borrowed_439040(
        f, {2, 4, 0x43a00000, 0x42480000, {}, {0x3fa00000, 0xffffffff, 3, 0, 0x3f800000, 0x41, 0xffffff10, 0}},
        out.first(call.initialized_prefix));
}
std::span<const std::uint8_t> score_number_43d5f0(FrontendState &f, W value, W minimum_digits)
{
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
        --minimum_digits;
    } while (value != 0 || as_signed(minimum_digits) > 0);
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
std::span<const std::uint8_t> initials_43d690(FrontendState &f, W address)
{
    const auto target = source_word(f, 0x479668);
    const auto bytes = f.source.source_bytes(address, 3);
    const auto first = bytes[0], second = bytes[1], third = bytes[2];
    auto out = f.source.source_bytes(target, 4);
    out[0] = first;
    out[1] = second;
    out[2] = third;
    return out;
}
void decimal(FrontendState &f, W fmt, W extent, W value, W return_site)
{
    const auto platform = f.shared_platform();
    const std::array<W, 1> args{value};
    SourceScriptPathCall call;
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(),
                          f.source.source_bytes(fmt, extent), args, f.source.source_bytes(0x627c54, 64), return_site,
                          call);
}
void copy_score_text(FrontendState &f, std::span<const std::uint8_t> text)
{
    W n = 0;
    while (n < text.size() && text[n] != 0)
        ++n;
    if (n == text.size())
        throw FormatError("4596FF source score string lacks a bounded NUL");
    ++n;
    auto out = f.source.source_bytes(0x627c54, n);
    for (W i = 0; i < n; ++i)
        out[i] = text[i];
}
void raw_text(FrontendState &f, W x, W y, std::span<const std::uint8_t> text, W color, W flags, W spacing = 0)
{
    if (spacing != 0)
        f.world().ui.fonts.metrics().digit_advance_bits = spacing; // 438A20 ->4291A0
    queue_frontend_text_438990(f.source, 17, 0, x, y, 2, text, 0x3fc00000, color, flags, spacing);
}
void draw_high_score_459530(FrontendState &f, W page)
{
    const auto count = page_word(f, page, 0x44);
    W visible = 10;
    if (as_signed(count) < as_signed(visible))
        visible = count;
    const auto scroll = page_word(f, page, 0x20);
    const auto integer = static_cast<W>(frontend_ftol_f32_462860(scroll));
    const std::array<W, 3> y_args{visible - 1u, integer, scroll};
    W y;
    sgn_records_first_y_45955f(y_args.data(), &y);
    W row = as_signed(integer) < 0 ? 0u : integer;
    W end = integer + visible + 1u;
    const auto fresh_count = page_word(f, page, 0x44);
    if (as_signed(end) > as_signed(fresh_count))
        end = fresh_count;
    W phase = page_word(f, page, 0x28);
    if (as_signed(row) >= as_signed(end))
        return;
    W record = page_word(f, page, 0x48) + row * 8u;
    const auto format = page_word(f, page, 0x40); // retained through all rows
    for (; as_signed(row) < as_signed(end); ++row, record += 8u)
    {
        phase = st(double(as_float(phase)) + double(as_float(0x3ca3d70a))); // 4595C6..E0
        decimal(f, 0x488d3c, 8, row + 1u, 0x4595e8);
        const auto odd = row & 1u;
        const std::array<W, 4> args{phase, odd, odd ? page_word(f, page, 0x34) : 0u, page_word(f, page, 0x30)};
        W wave;
        sgn_records_wave_4595e8(args.data(), &wave);
        W color = 0xffffffff;
        if (f.source.source_bytes(record + 7u, 1)[0] != 0 && (source_word(f, 0x5ff45c) & 0x20u) != 0)
            color = 0xffff8080;
        const auto center = st(double(as_float(wave)) + 320.0);
        raw_text(f, st(double(as_float(center)) - 150.0), y, f.source.source_bytes(0x627c54, 64), color, 3);
        const auto initials = initials_43d690(f, record + 4u);
        raw_text(f, st(double(as_float(center)) - 70.0), y, initials, color, 3);
        raw_text(f, center, y, f.source.source_bytes(0x488d44, 12), color, 3);
        switch (format)
        {
        case 0:
            copy_score_text(f, score_number_43d5f0(f, read_record(f, record), 7));
            break;
        case 1:
            decimal(f, 0x488d50, 8, read_record(f, record), 0x4596d5);
            break;
        case 2:
            decimal(f, 0x488d58, 8, read_record(f, record), 0x4596ec);
            break;
        case 3:
            copy_score_text(f, format_source_time_43d550(f, read_record(f, record)));
            break;
        default:
            break; // source retains the rank text for an unknown format
        }
        raw_text(f, st(double(as_float(wave)) + 480.0), y, f.source.source_bytes(0x627c54, 64), color, 6, 0x41600000);
        y = st(double(as_float(y)) + 28.0);
    }
}
} // namespace
W create_frontend_records_page_43e000(FrontendState &f, W key, W selector, SourceNavigationPageServices &pages,
                                      SourceMainPageServices &main)
{
    f.shared_platform()->require_owner_thread();
    W size, name, type, close, event, descriptor = 0, child_name = 0;
    if (selector == 21)
    {
        size = 24;
        name = 0x4796d0;
        type = 19;
        const auto page = allocate_frontend_page_object_43d470(page_pool(f), size);
        event = source_word(f, 0x488b24); // 43E2D4 before clear
        for (W o = 0; o < size; o += 4)
            set_page_word(f, page, o, 0);
        close = source_word(f, 0x488b20);
        set_page_word(f, page, 4, type);
        set_page_word(f, page, 12, close);
        set_page_word(f, page, 16, event);
        set_page_word(f, page, 0, name);
        set_page_word(f, page, 8, 0);
        child(f, page, 0x488af0, 0x488b28, pages, main);
        clear_persistence_record_changes(f.source.persistence_view()); // full43BF30, does not copy comparison
        set_page_word(f, page, 8, key);
        return page;
    }
    if (selector == 35)
    {
        const auto page = allocate_frontend_page_object_43d470(page_pool(f), 24);
        close = source_word(f, 0x488a38); // 43E381 before clear
        for (W o = 0; o < 24; o += 4)
            set_page_word(f, page, o, 0);
        set_page_word(f, page, 8, 0);
        event = source_word(f, 0x488a3c);
        set_page_word(f, page, 12, close);
        set_page_word(f, page, 4, 40);
        set_page_word(f, page, 0, 0x4796ec);
        set_page_word(f, page, 16, event);
        child(f, page, 0x488a08, 0x488a40, pages, main);
        set_page_word(f, page, 8, key);
        return page;
    }
    if (selector == 34)
    {
        size = 24;
        name = 0x4796dc;
        type = 39;
        descriptor = 0x488960;
        child_name = 0x488998;
    }
    else if (selector == 39)
    {
        size = 76;
        name = 0x479854;
        type = 34;
    }
    else
        throw FormatError("records factory received a different source selector");
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), size);
    for (W o = 0; o < size; o += 4)
        set_page_word(f, page, o, 0);
    event = source_word(f, selector == 34 ? 0x488994 : 0x488d2c);
    set_page_word(f, page, 8, 0);
    close = source_word(f, selector == 34 ? 0x488990 : 0x488d28);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 4, type);
    set_page_word(f, page, 0, name);
    if (selector == 34)
    {
        track_rows_456fc0(f);
        child(f, page, descriptor, child_name, pages, main);
    }
    else
        high_score_init_459200(f, page, pages, main);
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_records_page_child(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto c = page_word(f, page, 0x14);
    if (page_word(f, c, 12) != 0x43c970)
        throw FormatError("records page reached an unreconstructed MenuText destructor");
    release_frontend_page_object_43d4c0(page_pool(f), c);
    set_page_word(f, page, 0x14, 0);
}
W dispatch_frontend_records_page(FrontendState &f, W page, W callback, W event, SourcePageFactoryServices &factory,
                                 SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                 SourceMainPageServices &main, SourceTitlePageServices &draw)
{
    f.shared_platform()->require_owner_thread();
    if (callback == 0x459320)
    {
        switch (event)
        {
        case 19:
            appearance(f, 6, 97, 0xbecccccd, 0x3e99999a);
            (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
            set_page_word(f, page, 0x18, 1);
            break;
        case 20:
            (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
            break;
        case 17:
            main.feedback_440890(f, event);
            [[fallthrough]]; // 45939C, event16 enters4593A3
        case 16:
            (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
            (void)pop_frontend_page_43eef0(f, pages, music);
            break;
        case 11: {
            if (as_signed(page_word(f, page, 0x44)) <= 10)
                break;
            const auto index = page_word(f, page, 0x1c);
            if (index == 0)
                break;
            const auto next = index - 5u;
            set_page_word(f, page, 0x1c, next);
            if (as_signed(next) < 0)
                set_page_word(f, page, 0x1c, 0);
            main.feedback_440890(f, event);
            break;
        }
        case 12: {
            const auto count = page_word(f, page, 0x44);
            if (as_signed(count) <= 10)
                break;
            const auto index = page_word(f, page, 0x1c), last = count - 10u;
            if (index == last)
                break;
            const auto next = index + 5u;
            set_page_word(f, page, 0x1c, next);
            if (as_signed(next) > as_signed(last))
                set_page_word(f, page, 0x1c, last);
            main.feedback_440890(f, event);
            break;
        }
        case 2: {
            (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
            const auto target = sgn_menu_option_fild_fstp(page_word(f, page, 0x1c));
            const auto current = page_word(f, page, 0x20);
            set_page_word(f, page, 0x24, target);
            set_page_word(f, page, 0x20, approach_43d810(current, target));
            const auto phase =
                st(double(as_float(page_word(f, page, 0x2c))) + double(as_float(page_word(f, page, 0x28))));
            const auto active = page_word(f, page, 0x18);
            set_page_word(f, page, 0x28, phase);
            if (active != 0)
                set_page_word(f, page, 0x30, approach_43d880(page_word(f, page, 0x30), 0));
            const auto count = page_word(f, page, 0x44);
            const std::array<W, 6> range =
                as_signed(count) > 10 ? std::array<W, 6>{0, 0, 0, sgn_menu_option_fild_fstp(count), 0x41200000, target}
                                      : std::array<W, 6>{};
            for (W i = 0; i < 6; ++i)
                source_word(f, 0x5ff488u + i * 4u) = range[i];
            break;
        }
        case 3:
            draw_high_score_459530(f, page);
            set_frontend_clip_438190(f.source, {0, 0, 0x44200000, 0x43f00000});
            (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
            {
                const auto argument = page_word(f, page, 0x3c), title = page_word(f, page, 0x38);
                records_background(f, title, argument, main);
            }
            break;
        default:
            break;
        }
        return 0;
    }
    if (callback != 0x457520 && callback != 0x457110 && callback != 0x457300)
        throw FormatError("records dispatch received a different source callback");
    switch (event)
    {
    case 19:
        appearance(f, 1, 99, 0x3e99999a, 0xbf000000);
        [[fallthrough]];
    case 2:
    case 11:
    case 12:
    case 20:
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 16: {
        const auto retained = page_word(f, page, 0x14);
        (void)main.menu_text_event(f, retained, event, 0);
        const auto selected = main.menu_text_event(f, retained, 26, 0);
        if (callback == 0x457520)
        {
            switch (selected)
            {
            case 0:
                configure_scores(f, 0x1fb, 0, 2, 100, 0x5df8d4);
                (void)push_frontend_page_43dfa0(f, 16, 39, factory, pages, music);
                break;
            case 1:
                (void)push_frontend_page_43dfa0(f, 42, 34, factory, pages, music);
                break;
            case 2:
                configure_scores(f, 0x1fd, 0, 2, 100, 0x5e135c);
                (void)push_frontend_page_43dfa0(f, 37, 39, factory, pages, music);
                break;
            case 3:
                (void)push_frontend_page_43dfa0(f, 49, 35, factory, pages, music);
                break;
            default:
                break;
            }
        }
        else if (callback == 0x457110)
        {
            if (source_track_available_4371c0(f, selected, 3) != 0)
            {
                source_word(f, 0x5e3bf4) = 0x1fc;
                const auto text = frontend_language_text_441170(f.source.language_strings(), selected + 0x1b7u);
                source_word(f, 0x5e3bf8) = text.value;
                source_word(f, 0x5e3bfc) = 3;
                source_word(f, 0x5e3c00) = 10;
                source_word(f, 0x5e3c04) = 0x5dfbf8u + selected * 84u;
                (void)push_frontend_page_43dfa0(f, 42, 39, factory, pages, music);
            }
        }
        else
        {
            if (selected == 0)
            {
                configure_scores(f, 0x28b, 0, 0, 10, 0x5e12bc);
                (void)push_frontend_page_43dfa0(f, 49, 39, factory, pages, music);
            }
            else if (selected == 1)
            {
                configure_scores(f, 0x28a, 0, 1, 10, 0x5e130c);
                (void)push_frontend_page_43dfa0(f, 49, 39, factory, pages, music);
            }
        }
        break;
    }
    case 17:
        if (pop_frontend_page_43eef0(f, pages, music) != 0)
            main.feedback_440890(f, event);
        break;
    case 3: {
        const auto title = callback == 0x457520 ? 0x2cau : callback == 0x457110 ? 0x2ccu : 0x2ceu;
        main.background_4392b0(f, title, 0);
        (void)main.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        if (callback != 0x457300)
            footer_439340(f, title + 1u, draw);
        break;
    }
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
