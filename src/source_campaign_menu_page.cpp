#include "sgn/source_campaign_menu_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/source_selection_data.hpp"
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
CampaignConfigurationCatalog catalog(FrontendState &f)
{
    return {bank<0x57e4>(f, 0x5f3af4), bank<0x660>(f, 0x5f943c)};
}
W price_437de0(FrontendState &f, W sponsor, W car, W configuration)
{
    // Actual0x218-byte source local; existing436490 produces its value+EC.
    std::array<std::byte, 0x218> local;
    compose_campaign_configuration_436490(catalog(f), sponsor, bank<0xec>(f, configuration),
                                          bank<300>(f, 0x5f3af4u + car * 300u), local);
    W value;
    std::memcpy(&value, local.data() + 0xec, 4);
    return value;
}
void refresh_presentation_450db0(FrontendState &f, W page)
{
    if (page_word(f, page, 0x20) != 0)
    {
        build_campaign_presentation_44bd00({bank<0x160>(f, 0x5f92dc), bank<0x148>(f, 0x5f9aa4), catalog(f),
                                            bank<4>(f, 0x47966c), bank<0x80>(f, 0x5ff328)});
        set_page_word(f, page, 0x20, 0);
    }
}
void refresh_labels_450dd0(FrontendState &f)
{
    const auto race = source_word(f, 0x5f92e4);
    source_word(f, 0x48680c) = as_signed(race) >= 2 ? 0x9du : 0x9cu;
    source_word(f, 0x486840) = as_signed(source_word(f, 0x5e38f8)) > 0 ? 2u : 0xfffffffeu;
}
W initialize_campaign_menu_450c10(FrontendState &f, W page, SourceNavigationPageServices &pages,
                                  SourceMainPageServices &calls)
{
    set_page_word(f, page, 0x28, 0);
    auto child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    clear(f, child, 200);
    const auto close = source_word(f, 0x4794e8), event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, close);
    set_page_word(f, child, 16, event);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4868d8);
    set_page_word(f, child, 8, 0);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4867d0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x18, 0);
    }
    else
        set_page_word(f, page, 0x18, child);
    (void)calls.menu_text_event(f, page_word(f, page, 0x18), 24, 0);
    child = allocate_frontend_page_object_43d470(page_pool(f), 200);
    const auto second_close = source_word(f, 0x4794e8);
    clear(f, child, 200);
    const auto second_event = source_word(f, 0x4794ec);
    set_page_word(f, child, 12, second_close);
    set_page_word(f, child, 4, 41);
    set_page_word(f, child, 0, 0x4868e4);
    set_page_word(f, child, 8, 0);
    set_page_word(f, child, 16, second_event);
    if (initialize_frontend_menu_text_43c860(f, child, 0x4868a0, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        set_page_word(f, page, 0x14, 0);
    }
    else
        set_page_word(f, page, 0x14, child);
    const auto retained = page_word(f, page, 0x14);
    (void)calls.menu_text_event(f, retained, 24, 0);
    (void)calls.menu_text_event(f, retained, 22, 0);
    set_page_word(f, page, 0x24, 0);
    set_page_word(f, page, 0x1c, 0);
    set_page_word(f, page, 0x20, 1);
    refresh_presentation_450db0(f, page);
    const auto old_car = source_word(f, 0x5f92ec), sponsor = source_word(f, 0x5f93e4);
    const auto old_value = price_437de0(f, sponsor, old_car, 0x5f92f0);
    source_word(f, 0x5e38f8) = 0;
    for (W car = 1, configuration = 0x5f2798; configuration < 0x5f3a08u; ++car, configuration += 236u)
    {
        const auto value = price_437de0(f, source_word(f, 0x5f93e4), car, configuration);
        const auto mode = source_word(f, 0x5e3790);
        const auto available = source_car_available_437230(f, car, mode);
        if (available == 0 && read(f, 0x5df5d4) == 0)
            continue;
        const auto cash = source_word(f, 0x5f92e8);
        if (as_signed(cash + old_value) < as_signed(value) || car == source_word(f, 0x5f92ec))
            continue;
        const auto count = source_word(f, 0x5e38f8);
        source_word(f, 0x5e38fcu + count * 4u) = car;
        source_word(f, 0x5e38f8) = source_word(f, 0x5e38f8) + 1u;
    }
    refresh_labels_450dd0(f);
    return 1;
}
void open_exit_451680(FrontendState &f, W page, SourceMainPageServices &calls)
{
    (void)calls.menu_text_event(f, page_word(f, page, 0x14), 25, 0);
    const auto child = page_word(f, page, 0x18);
    (void)calls.menu_text_event(f, child, 19, 0);
    (void)calls.menu_text_event(f, child, 28, 2);
    set_page_word(f, page, 0x28, 1);
}
void close_exit_4516c0(FrontendState &f, W page, SourceMainPageServices &calls)
{
    (void)calls.menu_text_event(f, page_word(f, page, 0x14), 24, 0);
    (void)calls.menu_text_event(f, page_word(f, page, 0x18), 20, 0);
    set_page_word(f, page, 0x28, 0);
}
auto language(FrontendState &f, W id)
{
    return resolve_frontend_language_text(f.source.language_strings(),
                                          frontend_language_text_441170(f.source.language_strings(), id));
}
void label(FrontendState &f, W x, W y, W id, W color, W flags)
{
    queue_frontend_text_438990(f.source, 12, 1, x, y, 4, language(f, id), 0x3fa00000, color, flags, 0);
}
void raw(FrontendState &f, W x, W y, W address, W capacity, W color, W flags, W spacing = 0)
{
    if (spacing != 0)
        f.world().ui.fonts.metrics().digit_advance_bits = spacing; // 438A20 ->4291A0
    queue_frontend_text_438990(f.source, 12, 0, x, y, 4, f.source.source_bytes(address, capacity), 0x3fa00000, color,
                               flags, spacing);
}
void format(FrontendState &f, std::span<const std::uint8_t> pattern, std::span<const W> args, W return_site)
{
    const auto platform = f.shared_platform();
    SourceScriptPathCall call;
    (void)format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), pattern, args,
                                f.source.source_bytes(0x624a14, 256), return_site, call);
}
void draw_standings_450e50(FrontendState &f, W race, W maximum, W center_x, W center_y,
                           SourceMenuPageDrawServices &panels)
{
    const auto left = st(double(as_float(center_x)) - 160.0), top = st(double(as_float(center_y)) - 123.0);
    const auto right = st(double(as_float(center_x)) + 160.0), bottom = st(double(as_float(center_y)) + 123.0);
    const SourceMenuRect rect{left, top, right, bottom};
    panels.frame_439450(f, 10, 110, rect, 0x40);
    panels.fill_439380(f, 13, 2, rect, 0, 0);
    W y = st(double(as_float(top)) + 24.0);
    const std::array<W, 2> heading{race, as_signed(maximum) >= as_signed(race) ? maximum : race};
    format(f, language(f, 0xa7), heading, 0x450ef9);
    const auto center = st((double(as_float(right)) + double(as_float(left))) * 0.5);
    raw(f, center, y, 0x624a14, 256, 0xffffffffu, 3);
    y = st(double(as_float(y)) + 26.0);
    const auto name_x = st(double(as_float(left)) + 18.0);
    label(f, name_x, y, 0xaa, 0xffff8080u, 2);
    const auto sponsor_x = st(double(as_float(left)) + 80.0);
    label(f, sponsor_x, y, 0xab, 0xffff8080u, 2);
    const auto points_x = st(double(as_float(left)) + 244.0);
    label(f, points_x, y, 0xac, 0xffff8080u, 6);
    const auto wins_x = st(double(as_float(left)) + 190.0);
    label(f, wins_x, y, 0xad, 0xffff8080u, 6);
    const auto rank_x = st(double(as_float(left)) + 290.0);
    label(f, rank_x, y, 0xae, 0xffff8080u, 3);
    y = st(double(as_float(y)) + 20.0);
    for (W row = 0x5f9ab8; row < 0x5f9b78u; row += 32u)
    {
        const auto color = source_word(f, row - 8u);
        raw(f, name_x, y, row, 8, color, 2);
        label(f, sponsor_x, y, source_word(f, row + 8u) + 0x1eau, color, 2);
        const std::array<W, 1> points{source_word(f, row - 16u)};
        format(f, f.source.source_bytes(0x4868f0, 4), points, 0x45109f);
        raw(f, points_x, y, 0x624a14, 256, color, 6, 0x41600000);
        const std::array<W, 1> wins{source_word(f, row - 12u)};
        format(f, f.source.source_bytes(0x4868f4, 4), wins, 0x4510da);
        raw(f, wins_x, y, 0x624a14, 256, color, 6, 0x41600000);
        label(f, rank_x, y, source_word(f, row - 4u) + 2u, color, 3);
        y = st(double(as_float(y)) + 16.0);
    }
    y = st(double(as_float(y)) + 10.0);
    const auto footer = st(double(as_float(left)) + 88.0);
    label(f, center, y, 0xa8, 0xffffffffu, 3);
    y = st(double(as_float(y)) + 26.0);
    const auto caption_x = st(double(as_float(footer)) + 32.0);
    label(f, caption_x, y, 0xaf, 0xffc8c8ffu, 6);
    const auto currency_x = st(double(as_float(footer)) + 64.0);
    label(f, currency_x, y, 0x1b, 0xffc8c8ffu, 3);
    const auto value_x = st(double(as_float(footer)) + 160.0);
    raw(f, value_x, y, 0x5f9b68, 64, 0xffc8c8ffu, 6, 0x41600000);
    y = st(double(as_float(y)) + 20.0);
    label(f, caption_x, y, 0xb0, 0xffc8c8ffu, 6);
    label(f, currency_x, y, 0x1b, 0xffc8c8ffu, 3);
    raw(f, value_x, y, 0x5f9bac, 64, 0xffc8c8ffu, 6, 0x41600000);
}
} // namespace
void draw_frontend_campaign_standings_450e50(FrontendState &f, W race, W maximum, W x, W y,
                                             SourceMenuPageDrawServices &panels)
{
    draw_standings_450e50(f, race, maximum, x, y, panels);
}
W create_frontend_campaign_menu_page_43e000(FrontendState &f, W key, SourceNavigationPageServices &pages,
                                            SourceMainPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 44);
    clear(f, page, 44);
    const auto event = source_word(f, 0x4868d4);
    set_page_word(f, page, 8, 0);
    const auto close = source_word(f, 0x4868d0);
    set_page_word(f, page, 12, close);
    set_page_word(f, page, 16, event);
    set_page_word(f, page, 4, 12);
    set_page_word(f, page, 0, 0x47976c);
    if (initialize_campaign_menu_450c10(f, page, pages, calls) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    set_page_word(f, page, 8, key);
    return page;
}
void destroy_frontend_campaign_menu_page_450e10(FrontendState &f, W page)
{
    f.shared_platform()->require_owner_thread();
    const auto first = page_word(f, page, 0x14);
    if (page_word(f, first, 12) != 0x43c970u)
        throw FormatError("450E19 reached an unreconstructed MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), first);
    const auto second = page_word(f, page, 0x18);
    set_page_word(f, page, 0x14, 0);
    if (page_word(f, second, 12) != 0x43c970u)
        throw FormatError("450E2F reached an unreconstructed MenuText close identity");
    release_frontend_page_object_43d4c0(page_pool(f), second);
    set_page_word(f, page, 0x18, 0);
}
W dispatch_frontend_campaign_menu_page_4512b0(FrontendState &f, W page, W event, SourcePageFactoryServices &factory,
                                              SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                              SourceMainPageServices &calls, SourceMenuPageDrawServices &panels)
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
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        break;
    case 16:
        if (page_word(f, page, 0x28) != 0)
        {
            const auto child = page_word(f, page, 0x18);
            (void)calls.menu_text_event(f, child, event, 0);
            if (calls.menu_text_event(f, child, 26, 0) == 1)
                close_exit_4516c0(f, page, calls);
            else
            {
                const auto race = source_word(f, 0x5f92e4); // 451349 before flag store; both CMP outcomes pop
                (void)race;
                source_word(f, 0x5f9a9c) = 2;
                (void)pop_frontend_page_43eef0(f, pages, music);
            }
        }
        else
        {
            const auto child = page_word(f, page, 0x14), choice = calls.menu_text_event(f, child, 26, 0);
            switch (choice)
            {
            case 0:
                (void)calls.menu_text_event(f, child, event, 0);
                source_word(f, 0x5f9a9c) = 4;
                (void)pop_frontend_page_43eef0(f, pages, music);
                break;
            case 1:
                (void)calls.menu_text_event(f, child, event, 0);
                // Real source selector7. Its separately unreconstructed page must
                // remain a required factory boundary, never a successful no-op.
                (void)push_frontend_page_43dfa0(f, 22, 7, factory, pages, music);
                set_page_word(f, page, 0x20, 1);
                break;
            case 2:
                if (as_signed(source_word(f, 0x5e38f8)) > 0)
                {
                    set_page_word(f, page, 0x1c, 1);
                    (void)calls.menu_text_event(f, child, event, 0);
                    (void)push_frontend_page_43dfa0(f, 20, 10, factory, pages, music);
                }
                else
                    pages.feedback_440880(f, 9);
                break;
            case 3:
                (void)calls.menu_text_event(f, child, event, 0);
                source_word(f, 0x5e3c1c) = 0;
                source_word(f, 0x5e3c20) = 0;
                source_word(f, 0x5e3c28) = 3;
                source_word(f, 0x5e3c24) = 0;
                set_page_word(f, page, 0x24, 1);
                (void)push_frontend_page_43dfa0(f, 0, 27, factory, pages, music);
                break;
            case 4:
                open_exit_451680(f, page, calls);
                (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
                break;
            default:
                break;
            }
        }
        break;
    case 17:
        if (page_word(f, page, 0x28) != 0)
        {
            close_exit_4516c0(f, page, calls);
            calls.feedback_440890(f, event);
        }
        else
        {
            calls.feedback_440890(f, event);
            open_exit_451680(f, page, calls);
        }
        break;
    case 11:
    case 12:
        (void)calls.menu_text_event(f, page_word(f, page, page_word(f, page, 0x28) != 0 ? 0x18u : 0x14u), event, 0);
        break;
    case 2:
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x18), event, 0);
        break;
    case 1: {
        refresh_labels_450dd0(f);
        if (page_word(f, page, 0x24) != 0)
        {
            const auto saved = source_word(f, 0x5e3c40);
            set_page_word(f, page, 0x24, 0);
            if (saved != 0)
            {
                const auto child = page_word(f, page, 0x14);
                source_word(f, 0x5f9aa0) = 0;
                (void)calls.menu_text_event(f, child, 28, 0);
            }
        }
        if (page_word(f, page, 0x1c) != 0)
        {
            const auto cancelled = source_word(f, 0x5e38f4);
            set_page_word(f, page, 0x1c, 0);
            if (cancelled != 0)
                return 0;
            const auto player = source_word(f, 0x5e3798), sponsor = source_word(f, 0x5f93e4),
                       car = source_word(f, 0x5e3954u + player * 4u);
            const auto old_value = price_437de0(f, sponsor, source_word(f, 0x5f92ec), 0x5f92f0);
            const auto configuration = 0x5f26acu + car * 236u;
            const auto value = price_437de0(f, source_word(f, 0x5f93e4), car, configuration),
                       cash = source_word(f, 0x5f92e8) + old_value;
            source_word(f, 0x5f92ec) = car;
            source_word(f, 0x5f92e8) = cash;
            set_page_word(f, page, 0x20, 1);
            for (W i = 0; i < 59u; ++i)
            {
                const auto value_word = read(f, configuration + i * 4u);
                source_word(f, 0x5f92f0u + i * 4u) = value_word;
            }
            source_word(f, 0x5f92e8) -= value;
        }
        refresh_presentation_450db0(f, page);
        break;
    }
    case 3: {
        calls.background_4392b0(f, 155, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x14), event, 0);
        (void)calls.menu_text_event(f, page_word(f, page, 0x18), event, 0);
        // 4516F0 is exactly RET. No manufactured callback or hidden draw state.
        const auto maximum = source_word(f, 0x5f943c), race = source_word(f, 0x5f92e4);
        draw_standings_450e50(f, race, maximum, 0x43400000, 0x43700000, panels);
        break;
    }
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
