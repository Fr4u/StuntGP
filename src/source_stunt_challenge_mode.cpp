#include "sgn/source_stunt_challenge_mode.hpp"
#include "sgn/source_challenge_launch.hpp"
#include "sgn/source_start_mode2.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/retail_championship.hpp"
#include "sgn/race_session_scene.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <algorithm>
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::source_word;
W read(FrontendState &f, W address, W size = 4)
{
    const auto b = f.source.source_bytes(address, size);
    W value = 0;
    for (W i = 0; i < size; ++i)
        value |= W(b[i]) << (8u * i);
    return value;
}
void put(FrontendState &f, W address, W value, W size = 4)
{
    const auto b = f.source.source_bytes(address, size);
    for (W i = 0; i < size; ++i)
        b[i] = static_cast<std::uint8_t>(value >> (8u * i));
}
void format_previous_score_44d54e(FrontendState &f, W captured_score)
{
    const auto initials = copy_frontend_initials_43d690(f, 0x5e1360);
    const auto id = frontend_language_text_441170(f.source.language_strings(), 0x261);
    const auto format = resolve_frontend_language_text(f.source.language_strings(), id);
    const std::array<W, 2> args{initials, captured_score};
    const auto platform = f.shared_platform();
    SourceScriptPathCall call;
    format_save_ui_46280c(platform->state().boot_config, platform->source_application_literals(), format, args,
                          f.source.source_bytes(0x61c0c0, 256), 0x44d56f, call,
                          {4, f.source.source_bytes(initials, 4)});
}
void insert_score_437790(FrontendState &f, W initials, W score)
{
    // Actual caller:100 rows,8 bytes each at5E135C. Signed strict comparison.
    W row = 0;
    for (; row < 100u; ++row)
        if (as_signed(score) > as_signed(read(f, 0x5e135cu + row * 8u)))
            break;
    if (row == 100u)
        return;
    for (W last = 99; last > row; --last)
    {
        const auto destination = 0x5e135cu + last * 8u;
        const auto score_word = read(f, destination - 8u);
        put(f, destination, score_word);
        const auto name_word = read(f, destination - 4u);
        put(f, destination + 4u, name_word);
    }
    const auto target = 0x5e135cu + row * 8u;
    put(f, target + 7u, 1, 1);
    put(f, target, score);
    const auto first = read(f, initials, 2);
    put(f, target + 4u, first, 2);
    const auto third = read(f, initials + 2u, 1);
    put(f, target + 6u, third, 1);
}
} // namespace

void advance_frontend_stunt_challenge_mode_44d4c0(FrontendState &f, SourcePageFactoryServices &factory,
                                                  SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    auto push = [&](W key, W kind) { (void)push_frontend_page_43dfa0(f, key, kind, factory, pages, music); };
    switch (source_word(f, 0x5e3794))
    {
    case 0:
        source_word(f, 0x5e3798) = 0;
        source_word(f, 0x5e379c) = 1;
        source_word(f, 0x5e3794) = 1;
        return;
    case 1:
        source_word(f, 0x5e3794) = 2;
        push(35, 9);
        return;
    case 2: {
        if (source_word(f, 0x5e3a30) != 0)
        {
            (void)pop_frontend_page_43eef0(f, pages, music);
            return;
        }
        source_word(f, 0x5e3794) = 3;
        source_word(f, 0x5e3b4c) = 95;
        source_word(f, 0x5e3b50) = 0xffffffff;
        source_word(f, 0x5e3b54) = frontend_language_text_441170(f.source.language_strings(), 0x260).value;
        const auto score = read(f, 0x5e135c);
        W row = 1;
        if (score != 0)
        {
            format_previous_score_44d54e(f, score);
            source_word(f, 0x5e3b58) = 0xffff8080;
            source_word(f, 0x5e3b5c) = 0x61c0c0;
            row = 2;
        }
        source_word(f, 0x5e3b50u + row * 8u) = 0xffc8c8ff;
        source_word(f, 0x5e3b54u + row * 8u) = frontend_language_text_441170(f.source.language_strings(), 0x262).value;
        ++row;
        source_word(f, 0x5e3b48) = 0;
        source_word(f, 0x5e3bf0) = row;
        push(36, 38);
        return;
    }
    case 3: {
        if (source_word(f, 0x5e3b48) != 0)
        {
            source_word(f, 0x5e3794) = 1;
            return;
        }
        const auto selected = source_word(f, 0x5e3798);
        source_word(f, 0x61c0a4) = 21;
        const auto initials = 0x5e3a34u + (selected << 6u);
        source_word(f, 0x61c0a8) = 25;
        source_word(f, 0x61c0b0) = 90;
        const auto first = read(f, initials, 2);
        put(f, 0x61c0a0, first, 2);
        const auto third = read(f, initials + 2u, 1), record = read(f, 0x5e135c);
        put(f, 0x61c0a2, third, 1);
        source_word(f, 0x61c0ac) = record;
        prepare_frontend_challenge_43ae00(f, 0x61c0a0, 0x61c0b8);
        source_word(f, 0x5e3794) = 4;
        push(0, 25);
        return;
    }
    case 4:
        if (source_word(f, 0x61c0b8) == 0)
        {
            const auto selected = source_word(f, 0x5e3798), score = source_word(f, 0x61c0bc);
            insert_score_437790(f, 0x5e3a34u + (selected << 6u), score);
            const auto actual_score = source_word(f, 0x61c0bc), threshold = source_word(f, 0x5fcaac);
            if (as_signed(actual_score) >= as_signed(threshold))
            {
                const auto bytes = std::as_writable_bytes(f.source.source_bytes(0x5df57a, 1));
                unlock_campaign_car_437880(std::span<std::byte, 1>{bytes.data(), 1});
            }
        }
        source_word(f, 0x5e3bf4) = 0x1fd;
        source_word(f, 0x5e3bf8) = 0;
        source_word(f, 0x5e3bfc) = 2;
        source_word(f, 0x5e3c00) = 100;
        source_word(f, 0x5e3c04) = 0x5e135c;
        source_word(f, 0x5e3794) = 5;
        push(37, 39);
        return;
    case 5:
        clear_persistence_record_changes(f.source.persistence_view());
        source_word(f, 0x5e3794) = 6;
        push(38, 40);
        return;
    case 6:
        source_word(f, 0x5e3794) = 7;
        request_frontend_save_page_43d500(f, 0, factory, pages, music);
        return;
    case 7:
        (void)pop_frontend_page_43eef0(f, pages, music);
        return;
    default:
        return; // original unsigned JA rejects every value outside0..7
    }
}
} // namespace sgn::retail
