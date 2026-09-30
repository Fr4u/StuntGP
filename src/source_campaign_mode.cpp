#include "sgn/source_campaign_mode.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/retail_campaign_launch.hpp"
#include "sgn/source_challenge_launch.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/retail_campaign_presentation.hpp"
#include "sgn/retail_championship.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
W read(FrontendState &f, W address, W count = 4)
{
    const auto b = f.source.source_bytes(address, count);
    W value = 0;
    for (W i = 0; i < count; ++i)
        value |= W(b[i]) << (i * 8u);
    return value;
}
void put(FrontendState &f, W address, W value, W count = 4)
{
    const auto b = f.source.source_bytes(address, count);
    for (W i = 0; i < count; ++i)
        b[i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
template <std::size_t N> auto bank(FrontendState &f, W address)
{
    return std::as_writable_bytes(std::span<std::uint8_t, N>{f.source.source_bytes(address, N)});
}
void copy_words(FrontendState &f, W source, W destination, W count)
{
    for (W i = 0; i < count; ++i)
    {
        const auto value = read(f, source + i * 4u);
        put(f, destination + i * 4u, value);
    }
}
CampaignConfigurationCatalog catalog(FrontendState &f)
{
    return {bank<0x57e4>(f, 0x5f3af4), bank<0x660>(f, 0x5f943c)};
}
W price_437de0(FrontendState &f, W sponsor, W car, W configuration)
{
    // Full437DE0: the actual caller evaluates car21 before its availability gate.
    // Reuse436490 directly; the older presentation helper admits only car0..20.
    std::array<std::byte, 0x218> local;
    compose_campaign_configuration_436490(catalog(f), sponsor, bank<0xec>(f, configuration),
                                          bank<300>(f, 0x5f3af4u + car * 300u), local);
    W value;
    std::memcpy(&value, local.data() + 0xec, 4);
    return value;
}
void release_configuration_437e20(FrontendState &f)
{
    (void)read(f, 0x5f92f4); // 437E20 reads live+18; nonzero tail436400 is exactly RET.
}
void presentation_44bd00(FrontendState &f)
{
    build_campaign_presentation_44bd00({bank<0x160>(f, 0x5f92dc), bank<0x148>(f, 0x5f9aa4), catalog(f),
                                        bank<4>(f, 0x47966c), bank<0x80>(f, 0x5ff328)});
}
W rank_44ccf0(FrontendState &f)
{
    return campaign_player_rank_44ccf0(bank<0xc0>(f, 0x5f9aa4));
}
W text(FrontendState &f, W id)
{
    return frontend_language_text_441170(f.source.language_strings(), id).value;
}
void format_award(FrontendState &f, W destination, W id, W value, W return_site)
{
    const auto identity = frontend_language_text_441170(f.source.language_strings(), id);
    const auto bytes = resolve_frontend_language_text(f.source.language_strings(), identity);
    // Same46280C/46473F as the other source UI callers: live destination,
    // source partial writes, actual App literals and original return address.
    const auto platform = f.shared_platform();
    SourceScriptPathCall call;
    const std::array<W, 1> arguments{value};
    (void)format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), bytes,
                                arguments, f.source.source_bytes(destination, 260), return_site, call);
}

void insert_score_437790(FrontendState &f, W table, W value)
{
    // Both actual44C0A0 callers pass ten rows and name5F92E0.
    W row = 0;
    for (; row < 10u; ++row)
        if (as_signed(value) > as_signed(read(f, table + row * 8u)))
            break;
    if (row == 10u)
        return;
    for (W last = 9; last > row; --last)
    {
        const auto at = table + last * 8u;
        put(f, at, read(f, at - 8u));
        put(f, at + 4u, read(f, at - 4u));
    }
    const auto at = table + row * 8u;
    put(f, at + 7u, 1, 1);
    put(f, at, value);
    put(f, at + 4u, read(f, 0x5f92e0, 2), 2);
    put(f, at + 6u, read(f, 0x5f92e2, 1), 1);
}
} // namespace
void advance_frontend_campaign_mode_44c0a0(FrontendState &f, SourcePageFactoryServices &factory,
                                           SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    auto push = [&](W key, W kind) { (void)push_frontend_page_43dfa0(f, key, kind, factory, pages, music); };
    switch (read(f, 0x5e3794))
    {
    case 0:
        put(f, 0x5e3798, 0);
        put(f, 0x5e379c, 1);
        put(f, 0x5e37ac, 0);
        put(f, 0x5e37a8, 0);
        put(f, 0x5e3954, 0);
        put(f, 0x5f9aa0, 0);
        put(f, 0x5e3794, 1);
        return;
    case 1:
        push(17, 28);
        put(f, 0x5e3794, 2);
        return;
    case 2: {
        if (read(f, 0x5e38e0) != 0)
        {
            (void)pop_frontend_page_43eef0(f, pages, music);
            return;
        }
        for (W i = 0; i < 88u; ++i)
            put(f, 0x5f92dcu + i * 4u, 0);
        const auto fresh = read(f, 0x5e38e8), slot = read(f, 0x5e38e4);
        put(f, 0x5f92d8, slot);
        if (fresh != 0)
        {
            put(f, 0x5e3794, 3);
            return;
        }
        copy_words(f, 0x5e047cu + slot * 0x160u, 0x5f92dc, 88);
        put(f, 0x5f9a9c, 1);
        put(f, 0x5e3794, 7);
        return;
    }
    case 3:
        put(f, 0x5e38f0, 0);
        put(f, 0x5e3794, 4);
        push(18, 9);
        return;
    case 4:
        if (read(f, 0x5e3a30) != 0)
        {
            put(f, 0x5e3794, 1);
            return;
        }
        push(19, 13);
        put(f, 0x5e3794, 5);
        return;
    case 5: {
        if (read(f, 0x5e38ec) != 0)
        {
            put(f, 0x5e3794, 3);
            return;
        }
        const auto player = read(f, 0x5e3798), sponsor = read(f, 0x5e38f0);
        put(f, 0x5f92ec, 0);
        put(f, 0x5f92dc, 1);
        put(f, 0x5f92e4, 1);
        put(f, 0x5f92e8, read(f, 0x5f9470u + sponsor * 8u));
        const auto initials = 0x5e3a34u + player * 64u;
        put(f, 0x5f92e0, read(f, initials, 2), 2);
        const auto third = read(f, initials + 2u, 1);
        put(f, 0x5f93e8, 6);
        put(f, 0x5f92e2, third, 1);
        put(f, 0x5f93e4, sponsor);
        put(f, 0x5f93e0, 0);
        put(f, 0x5f93dc, 0);
        W excluded_grid = 6, excluded_sponsor = sponsor, grid = 1, rival = 1;
        for (W at = 0x5f93f0; at < 0x5f9440; at += 16u)
        {
            if (grid == excluded_grid)
                ++grid;
            if (rival == excluded_sponsor)
                ++rival;
            put(f, at - 4u, 0);
            put(f, at, 0);
            put(f, at + 4u, rival);
            put(f, at + 8u, grid);
            excluded_grid = read(f, 0x5f93e8);
            ++grid;
            ++rival;
            if (grid == excluded_grid)
                ++grid;
            excluded_sponsor = read(f, 0x5f93e4);
            if (rival == excluded_sponsor)
                ++rival;
        }
        put(f, 0x5e38f8, 0);
        auto retained_sponsor = excluded_sponsor;
        for (W car = 1, config = 0x5f2798; config < 0x5f3af4; ++car, config += 0xecu)
        {
            const auto value = price_437de0(f, retained_sponsor, car, config);
            if (source_car_available_437230(f, car, read(f, 0x5e3790)) != 0 &&
                as_signed(read(f, 0x5f92e8)) >= as_signed(value))
            {
                const auto count = read(f, 0x5e38f8);
                put(f, 0x5e38fcu + count * 4u, car);
                put(f, 0x5e38f8, read(f, 0x5e38f8) + 1u);
            }
            if (config + 0xecu < 0x5f3af4)
                retained_sponsor = read(f, 0x5f93e4);
        }
        put(f, 0x5e3794, 6);
        push(20, 10);
        return;
    }
    case 6: {
        if (read(f, 0x5e38f4) != 0)
        {
            put(f, 0x5e3794, 4);
            return;
        }
        release_configuration_437e20(f);
        const auto car = read(f, 0x5e3954u + read(f, 0x5e3798) * 4u), config = 0x5f26acu + car * 0xecu;
        const auto value = price_437de0(f, read(f, 0x5f93e4), car, config);
        put(f, 0x5f92e8, read(f, 0x5f92e8) - value);
        put(f, 0x5f92ec, read(f, 0x5e3954u + read(f, 0x5e3798) * 4u));
        copy_words(f, config, 0x5f92f0, 59);
        release_configuration_437e20(f);
        put(f, 0x5f9a9c, 1);
        put(f, 0x5e3794, 7);
        return;
    }
    case 7:
        presentation_44bd00(f);
        switch (read(f, 0x5f9a9c))
        {
        case 1:
            put(f, 0x5f9a9c, 0);
            push(21, 14);
            return;
        case 2:
            put(f, 0x5f9a9c, 0);
            put(f, 0x5e3794, 24);
            return;
        case 3:
            put(f, 0x5f9a9c, 0);
            put(f, 0x5e3794, 5);
            return;
        case 4:
            put(f, 0x5f9a9c, 0);
            put(f, 0x5e3794, 8);
            return;
        default:
            return;
        }
    case 8:
        put(f, 0x5e3964, 0);
        put(f, 0x5e3968, 0);
        put(f, 0x5e396c, read(f, 0x5f94c4u + read(f, 0x5f92e4) * 0x44u));
        put(f, 0x5e3968, 1);
        push(23, 11);
        put(f, 0x5e3794, 9);
        return;
    case 9: {
        if (read(f, 0x5e3964) != 0)
        {
            put(f, 0x5f9a9c, 1);
            put(f, 0x5e3794, 7);
            return;
        }
        put(f, 0x5f92f0, 1);
        for (W at = 0x5f92fc; at < 0x5f93dc; at += 16u)
            put(f, at, 1);
        auto world = frontend_launch_world(f);
        launch_campaign_race_44bf10(world);
        push(0, 25);
        put(f, 0x5e3794, 10);
        return;
    }
    case 10: {
        if (read(f, 0x61bed0) != 0)
        {
            put(f, 0x5e3794, 24);
            return;
        }
        const auto track = read(f, 0x61bdb4); // actual captured launch track, never UI selection
        const auto settlement = settle_campaign_race_prefix_44c0a0(
            {bank<0x160>(f, 0x5f92dc), bank<32>(f, 0x5e0f7cu + track * 32u), bank<52>(f, 0x5df568),
             bank<0x7c>(f, 0x61bed0), bank<0x70>(f, 0x5f9498), read(f, 0x5f943c), f.source.source_word(0x5e3794)});
        const auto place = read(f, 0x61bed4);
        presentation_44bd00(f);
        put(f, 0x5fcf0c, 0x5f);
        put(f, 0x5fcf10, 0xffffffff);
        put(f, 0x5fcf14, text(f, place + 0x27cu));
        const auto points = read(f, 0x5f9498u + place * 16u) * settlement.points_multiplier;
        format_award(f, 0x61b9c8, 0x283, points, 0x44c665);
        put(f, 0x5fcf18, 0xffff8080);
        put(f, 0x5fcf1c, 0x61b9c8);
        format_award(f, 0x61bdcc, 0x284, settlement.player_reward, 0x44c693);
        put(f, 0x5fcf20, 0xffff8080);
        put(f, 0x5fcf24, 0x61bdcc);
        put(f, 0x5fcf28, 0xffc8c8ff);
        const auto continuation = text(f, 0x26c), last = read(f, 0x5f943c);
        put(f, 0x5fcf2c, continuation);
        const auto round = read(f, 0x5f92e4);
        put(f, 0x5fcfb8, last);
        put(f, 0x5fcf08, 0);
        put(f, 0x5fcfb0, 4);
        put(f, 0x5fcfb4, round);
        push(44, 32);
        put(f, 0x5e3794, 11);
        return;
    }
    case 11:
        push(48, 40);
        put(f, 0x5e3794, 12);
        return;
    case 12:
        put(f, 0x5f92e4, read(f, 0x5f92e4) + 1u);
        put(f, 0x5e3794, 13);
        return;
    case 13: {
        const auto round = read(f, 0x5f92e4), last = read(f, 0x5f943c);
        if (as_signed(round) > as_signed(last))
        {
            put(f, 0x5e3794, as_signed(round) <= as_signed(last + 1u) ? 19u : 17u);
            return;
        }
        if (campaign_checkpoint_44cc90(bank<16>(f, 0x5f9440), round) == 0)
        {
            put(f, 0x5f9a9c, 1);
            put(f, 0x5e3794, 7);
            return;
        }
        put(f, 0x5e3794, 14);
        return;
    }
    case 14: {
        const auto rank = rank_44ccf0(f);
        constexpr std::array<W, 6> seconds{30, 40, 50, 60, 70, 90};
        const auto duration = rank - 1u < seconds.size() ? seconds[rank - 1u] : 90u;
        const auto third = read(f, 0x5f92e2, 1), first = read(f, 0x5f92e0, 2), record = read(f, 0x5e135c);
        put(f, 0x61baca, third, 1);
        put(f, 0x61bac8, first, 2);
        put(f, 0x61bacc, 21);
        put(f, 0x61bad0, 25);
        put(f, 0x61bad4, record);
        put(f, 0x61bad8, duration);
        put(f, 0x5e3794, 15);
        put(f, 0x5e3b4c, 0x5f);
        put(f, 0x5e3b50, 0xffffffff);
        put(f, 0x5e3b54, text(f, 0x263));
        format_award(f, 0x61b9c8, 0x264, duration, 0x44c861);
        put(f, 0x5e3b58, 0xffff8080);
        put(f, 0x5e3b5c, 0x61b9c8);
        put(f, 0x5e3b60, 0xffc8c8ff);
        put(f, 0x5e3b64, text(f, 0x265));
        put(f, 0x5e3b68, 0xffc8c8ff);
        put(f, 0x5e3b6c, text(f, 0x266));
        put(f, 0x5e3b48, 0);
        put(f, 0x5e3bf0, 4);
        push(45, 38);
        return;
    }
    case 15:
        prepare_frontend_challenge_43ae00(f, 0x61bac8, 0x61b9c0);
        put(f, 0x5e3794, 16);
        push(0, 25);
        return;
    case 16:
        if (read(f, 0x61b9c0) == 0)
        {
            const auto reward = read(f, 0x61b9c4);
            put(f, 0x5f92e8, read(f, 0x5f92e8) + reward);
            put(f, 0x5e3b4c, 0x5f);
            format_award(f, 0x61b9c8, 0x268, reward, 0x44c93a);
            put(f, 0x5e3b50, 0xffff8080);
            put(f, 0x5e3b54, 0x61b9c8);
            put(f, 0x5e3b58, 0xffc8c8ff);
            put(f, 0x5e3b5c, text(f, 0x267));
            put(f, 0x5e3b60, 0xffc8c8ff);
            put(f, 0x5e3b64, text(f, 0x269));
            put(f, 0x5e3b48, 0);
            put(f, 0x5e3bf0, 3);
            push(46, 38);
        }
        put(f, 0x5f9a9c, 1);
        put(f, 0x5e3794, 7);
        return;
    case 17:
        put(f, 0x5e3794, 21);
        return;
    case 18:
        put(f, 0x5e3964, 0);
        put(f, 0x5e3794, 9);
        put(f, 0x5e3b4c, 0x5f);
        put(f, 0x5e3b50, 0xffffffff);
        put(f, 0x5e3b54, text(f, 0x26a));
        put(f, 0x5e3b58, 0xffc8c8ff);
        put(f, 0x5e3b5c, text(f, 0x26b));
        put(f, 0x5e3b60, 0xffc8c8ff);
        put(f, 0x5e3b64, text(f, 0x26c));
        put(f, 0x5e3b48, 0);
        put(f, 0x5e3bf0, 3);
        push(47, 38);
        return;
    case 19: {
        const auto rank = rank_44ccf0(f);
        if (as_signed(rank) <= 3)
        {
            put(f, 0x5e3b34, 0x5f);
            put(f, 0x5e3b38, rank + 0x17fu);
            put(f, 0x5e3b3c, 0);
            put(f, 0x5e3b40, 0x3c75c28f);
            put(f, 0x5e3b44, 0x42a00000);
            push(44, 37);
        }
        put(f, 0x5e3794, 20);
        return;
    }
    case 20:
        put(f, 0x5e3794, rank_44ccf0(f) == 1 ? 18u : 21u);
        return;
    case 21: {
        presentation_44bd00(f);
        const auto cash = read(f, 0x5f9b64), value = read(f, 0x5f9ba8);
        insert_score_437790(f, 0x5e12bc, value + cash);
        put(f, 0x5e3bf4, 0x28b);
        put(f, 0x5e3bf8, 0);
        put(f, 0x5e3bfc, 0);
        put(f, 0x5e3c00, 10);
        put(f, 0x5e3c04, 0x5e12bc);
        push(49, 39);
        put(f, 0x5e3794, 22);
        return;
    }
    case 22:
        insert_score_437790(f, 0x5e130c, read(f, 0x5f93e0));
        put(f, 0x5e3bf4, 0x28a);
        put(f, 0x5e3bf8, 0);
        put(f, 0x5e3bfc, 1);
        put(f, 0x5e3c00, 10);
        put(f, 0x5e3c04, 0x5e130c);
        push(49, 39);
        put(f, 0x5e3794, 23);
        return;
    case 23:
        put(f, 0x5f92dc, 0);
        clear_persistence_record_changes(f.source.persistence_view());
        put(f, 0x5e3794, 24);
        return;
    case 24:
        commit_campaign_44ccc0(bank<0x160>(f, 0x5f92dc), bank<0x160>(f, 0x5e047cu + read(f, 0x5f92d8) * 0x160u));
        put(f, 0x5e3794, 25);
        request_frontend_save_page_43d500(f, 0, factory, pages, music);
        return;
    case 25:
        (void)pop_frontend_page_43eef0(f, pages, music);
        return;
    default:
        return; // 44C0B3 unsigned JA, including negative DWORD patterns
    }
}
} // namespace sgn::retail
