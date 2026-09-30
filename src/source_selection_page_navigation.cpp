#include "sgn/source_selection_page_navigation.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
#include <cmath>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_word;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
W integer_float(W v)
{
    return as_word(vehicle_f32(double(as_signed(v))));
}
W approach(W a, W b)
{
    return as_word(approach_frontend_value_43d810(as_float(a), as_float(b), as_float(0x3a83126f)));
}
W fraction(W v)
{
    // PC53/nearest domain, matching the surrounding reconstructed43D810.
    // Both inputs are binary32; original ftol uses truncation toward zero.
    const double sum = double(as_float(v)) + double(as_float(0x3cf5c28f));
    if (!std::isfinite(sum) || sum < -2147483648.0 || sum >= 2147483648.0)
        throw FormatError("selection animation ftol outside admitted signed32 domain");
    const auto integral = static_cast<std::int32_t>(sum);
    return as_word(vehicle_f32(sum - double(integral)));
}
void range(FrontendState &f, std::array<W, 6> values)
{
    for (W i = 0; i < 6; ++i)
        f.source.source_word(0x5ff488 + i * 4u) = values[i]; // full43DF60
}
void view(FrontendState &f, W flag)
{
    f.source.source_word(0x5ff46c) = flag;
    set_frontend_packed_word_43dd10(f.source, 0x114311);
    f.source.source_word(0x5ff448) = 96;
    f.source.source_word(0x5ff44c) = 0xbe99999a;
    f.source.source_word(0x5ff450) = 0xbf000000;
}
W selected_car(FrontendState &f, W p)
{
    const auto category = page_word(f, p, 0x1c), count = page_word(f, p, 0x50 + category * 48u);
    auto index = page_word(f, p, 0x18);
    if (as_signed(index) >= as_signed(count))
        index = count - 1u;
    return page_word(f, p, 0x58 + (index + category * 12u) * 4u);
}
} // namespace
W dispatch_frontend_track_page_45c6a0(FrontendState &f, W p, W e, W, SourcePageFactoryServices &factory,
                                      SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                      SourceMainPageServices &main, SourceSelectionPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    auto word = [&](W a) -> W & { return f.source.source_word(a); };
    const auto count = word(0x5e3968); // EDI captured even before event dispatch
    auto child = [&](W event) { return main.menu_text_event(f, page_word(f, p, 0x14), event, 0); };
    auto finish = [&]() {
        calls.finalize_track_45c670(f, p);
        (void)pop_frontend_page_43eef0(f, pages, music);
    };
    switch (e)
    {
    case 1:
        if (page_word(f, p, 0x2c) != 0)
        {
            const auto completed = word(0x5e3c40);
            set_page_word(f, p, 0x2c, 0);
            if (completed != 0)
            {
                const auto retained = page_word(f, p, 0x14);
                word(0x5e3c40) = 0;
                (void)main.menu_text_event(f, retained, 28, 0);
                for (W i = 0; i < 0x1d4c; ++i)
                {
                    const auto value = word(0x5eb178 + i * 4u);
                    word(0x5e3c48 + i * 4u) = value;
                }
                word(0x5e3c44) = word(0x5e3c2c);
            }
        }
        break;
    case 2: {
        (void)child(e);
        const auto index = page_word(f, p, 0x1c);
        const auto chosen = as_signed(index) >= as_signed(count) ? count - 1u : index;
        const auto target = integer_float(chosen), current = page_word(f, p, 0x20);
        set_page_word(f, p, 0x24, target);
        set_page_word(f, p, 0x20, approach(current, target));
        set_page_word(f, p, 0x28, fraction(page_word(f, p, 0x28)));
        if (as_signed(count) > 1)
            range(f, {integer_float(count), 0x3f800000, integer_float(chosen), 0, 0, 0});
        else
            range(f, {0, 0, 0, 0, 0, 0});
        break;
    }
    case 3: {
        const auto index = page_word(f, p, 0x1c), phase = page_word(f, p, 0x20);
        calls.track_strip_45cba0(f, index, phase);
        calls.clip_438190(f, {0, 0, 0x44200000, 0x43f00000});
        main.background_4392b0(f, word(0x5e3968) == 1 ? 439 : 438, 0);
        calls.track_title_45cc40(f, word(0x5e396c + index * 4u));
        calls.track_details_45cc70(f, p);
        (void)child(e);
        if (as_signed(word(0x5e379c)) > 1)
        {
            const auto y = word(0x4894f0), x = word(0x4894ec);
            calls.track_player_45d130(f, p, x, y);
        }
        break;
    }
    case 13:
        if (as_signed(page_word(f, p, 0x1c)) >= as_signed(count))
            set_page_word(f, p, 0x1c, count - 1u);
        if (as_signed(page_word(f, p, 0x1c)) > 0)
        {
            set_page_word(f, p, 0x1c, page_word(f, p, 0x1c) - 1u);
            main.feedback_440890(f, e);
            pages.feedback_440880(f, 20);
        }
        break;
    case 14:
        if (as_signed(page_word(f, p, 0x1c)) < as_signed(count - 1u))
        {
            set_page_word(f, p, 0x1c, page_word(f, p, 0x1c) + 1u);
            main.feedback_440890(f, e);
            pages.feedback_440880(f, 20);
        }
        break;
    case 16: {
        const auto retained = page_word(f, p, 0x14);
        (void)main.menu_text_event(f, retained, e, 0);
        const auto index = page_word(f, p, 0x1c), mode = word(0x5e3790);
        word(0x5e39d4) = word(0x5e396c + index * 4u);
        if (mode < 1u || mode > 7u || mode == 4u)
            break;
        const auto result = main.menu_text_event(f, retained, 26, 0);
        if (result == 0)
        {
            finish();
            break;
        }
        if (mode == 2)
        {
            if (result == 1)
            {
                auto value = page_word(f, p, 0x30) + 1u;
                set_page_word(f, p, 0x30, value);
                if (as_signed(value) > 2)
                    set_page_word(f, p, 0x30, 0);
            }
            else if (result == 2)
            {
                ++word(0x5e39e0);
                if (as_signed(word(0x5e39e0)) > 2)
                    word(0x5e39e0) = 0;
            }
            else if (result == 3)
            {
                ++word(0x5e39dc);
                if (as_signed(word(0x5e39dc)) > 5)
                    word(0x5e39dc) = 0;
            }
        }
        else if (mode == 6 && result == 1)
        {
            auto value = page_word(f, p, 0x30) + 1u;
            set_page_word(f, p, 0x30, value);
            if (as_signed(value) > 2)
                set_page_word(f, p, 0x30, 0);
        }
        else if (mode == 3 && (result == 1 || result == 2))
        {
            word(0x5e3c1c) = 0;
            word(0x5e3c20) = 0;
            word(0x5e3c28) = result == 1 ? 4 : 5;
            word(0x5e3c24) = 0;
            const auto track = word(0x5e396c + index * 4u);
            if (result == 2)
                word(0x5e3c30) = 0x5e3c48;
            const auto current_track = result == 1 ? word(0x5e39d4) : 0u;
            word(0x5e3c2c) = track;
            if (result == 1)
                word(0x5e3c30) = 0x5eb178;
            const auto key = result == 1 ? current_track : word(0x5e39d4);
            word(0x5e3c34) = calls.track_value_437da0(f, key);
            (void)push_frontend_page_43dfa0(f, 0, 27, factory, pages, music);
            if (result == 1)
                set_page_word(f, p, 0x2c, 1);
        }
        break;
    }
    case 17:
        word(0x5e3964) = 1;
        main.feedback_440890(f, e);
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 19:
        view(f, 5);
        [[fallthrough]];
    case 11:
    case 12:
    case 20:
        (void)child(e);
        break;
    default:
        break;
    }
    return 0;
}
W dispatch_frontend_car_page_451aa0(FrontendState &f, W p, W e, W, SourceNavigationPageServices &pages,
                                    CampaignScoreMusic &music, SourceMainPageServices &main,
                                    SourceSelectionPageServices &calls)
{
    f.shared_platform()->require_owner_thread();
    auto word = [&](W a) -> W & { return f.source.source_word(a); };
    auto child = [&](W event) { return main.menu_text_event(f, page_word(f, p, 0x14), event, 0); };
    switch (e)
    {
    case 1: {
        const auto car = selected_car(f, p);
        for (W i = 0; i < 3; ++i)
        {
            const auto b = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), p,
                                                      0x18cu + car * 3u + i + 1u);
            const auto value = std::to_integer<W>(b[0x18cu + car * 3u + i]);
            set_page_word(f, p, 0x44u + i * 4u, as_word(vehicle_f32(double(value) * double(as_float(0x3c23d70a)))));
        }
        break;
    }
    case 2: {
        (void)child(e);
        const auto new_fraction = fraction(page_word(f, p, 0x34));
        const auto category = page_word(f, p, 0x1c);
        auto index = page_word(f, p, 0x18);
        set_page_word(f, p, 0x34, new_fraction);
        const auto count = page_word(f, p, 0x50 + category * 48u);
        if (as_signed(index) >= as_signed(count))
            index = count - 1u;
        const auto target = integer_float(index), current = page_word(f, p, 0x24);
        set_page_word(f, p, 0x2c, target);
        const auto target_category = integer_float(category);
        set_page_word(f, p, 0x30, target_category);
        const auto approached = approach(current, target), current_category = page_word(f, p, 0x28);
        set_page_word(f, p, 0x24, approached);
        set_page_word(f, p, 0x28, approach(current_category, target_category));
        range(f, {integer_float(count), 0x3f800000, target, integer_float(page_word(f, p, 0x110)), 0x3f800000,
                  target_category});
        for (W i = 0; i < 3; ++i)
        {
            const auto target_stat = page_word(f, p, 0x44 + i * 4u), current_stat = page_word(f, p, 0x38 + i * 4u);
            set_page_word(f, p, 0x38 + i * 4u, approach(current_stat, target_stat));
        }
        break;
    }
    case 3: {
        const auto y = page_word(f, p, 0x28), x = page_word(f, p, 0x24);
        const auto display = calls.displayed_car_451e80(f, p, x, y);
        if (as_signed(display) > 0)
        {
            const auto newx = page_word(f, p, 0x24), newy = page_word(f, p, 0x28);
            calls.car_strip_452070(f, display, newx, newy);
        }
        calls.clip_438190(f, {0, 0, 0x44200000, 0x43f00000});
        const auto selected = selected_car(f, p), phase = page_word(f, p, 0x28);
        calls.car_frame_452130(f, p, 0x428a0000, 0x42a40000, phase);
        calls.car_details_4521b0(f, p, selected);
        main.background_4392b0(f, 388, 0);
        (void)child(e);
        if (as_signed(word(0x5e379c)) > 1)
        {
            const auto playery = word(0x4869a8), playerx = word(0x4869a4);
            calls.car_player_452000(f, p, playerx, playery);
        }
        calls.car_stats_451ed0(f, p);
        break;
    }
    case 11:
    case 12: {
        const auto retained = page_word(f, p, 0x14);
        if (main.menu_text_event(f, retained, e, 0) != 0)
            pages.feedback_440880(f, 20);
        const auto category = main.menu_text_event(f, retained, 26, 0);
        set_page_word(f, p, 0x1c, category);
        break;
    }
    case 13: {
        const auto category = page_word(f, p, 0x1c), index = page_word(f, p, 0x18),
                   count = page_word(f, p, 0x50 + category * 48u);
        if (as_signed(index) >= as_signed(count))
            set_page_word(f, p, 0x18, count - 1u);
        if (as_signed(page_word(f, p, 0x18)) > 0)
        {
            set_page_word(f, p, 0x18, page_word(f, p, 0x18) - 1u);
            main.feedback_440890(f, e);
            pages.feedback_440880(f, 20);
        }
        break;
    }
    case 14: {
        const auto category = page_word(f, p, 0x1c), index = page_word(f, p, 0x18),
                   count = page_word(f, p, 0x50 + category * 48u);
        if (as_signed(index) < as_signed(count - 1u))
        {
            set_page_word(f, p, 0x18, index + 1u);
            main.feedback_440890(f, e);
            pages.feedback_440880(f, 20);
        }
        break;
    }
    case 16: {
        const auto car = selected_car(f, p), slot = word(0x5e3798);
        word(0x5e3954 + slot * 4u) = car;
        main.feedback_440890(f, e);
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    }
    case 17:
        word(0x5e38f4) = 1;
        main.feedback_440890(f, e);
        (void)pop_frontend_page_43eef0(f, pages, music);
        break;
    case 19:
        view(f, word(0x5e3790) == 5 ? 7 : 5);
        [[fallthrough]];
    case 20:
        (void)child(e);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
