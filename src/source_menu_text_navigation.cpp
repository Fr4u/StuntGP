#include "sgn/source_menu_text_navigation.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_options_page.hpp"
#include "sgn/source_video_options_page.hpp"
#include "sgn/host_graphics_page.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::page_word;
using sgn::retail::frontend_page_words::set_page_word;
Word first(FrontendState &f, Word descriptor)
{ // 43C900, no row-count clamp
    Word at = f.source.source_word(descriptor + 0x28u), index = 0;
    auto text = f.source.source_word(at + 0xcu);
    while (text != 0)
    {
        if (as_signed(f.source.source_word(at)) >= 0)
            return index;
        text = f.source.source_word(at + 0x2cu);
        at += 32u;
        ++index;
    }
    return 0xffffffffu;
}
Word previous(FrontendState &f, Word descriptor, Word selected)
{ // 43CEF0
    if (selected == 0)
        return 0;
    auto index = selected - 1u;
    if (as_signed(index) < 0)
        return 0xffffffffu;
    auto at = f.source.source_word(descriptor + 0x28u) + index * 32u;
    do
    {
        if (as_signed(f.source.source_word(at)) >= 0)
            return index;
        --index;
        at -= 32u;
    } while (as_signed(index) >= 0);
    return 0xffffffffu;
}
Word next(FrontendState &f, Word descriptor, Word selected)
{ // 43CF20
    auto at = f.source.source_word(descriptor + 0x28u);
    auto index = selected + 1u;
    at += index * 32u;
    auto text = f.source.source_word(at + 0xcu);
    while (text != 0)
    {
        if (as_signed(f.source.source_word(at)) >= 0)
            return index;
        text = f.source.source_word(at + 0x2cu);
        at += 32u;
        ++index;
    }
    return 0xffffffffu;
}
Word approach(Word value, Word target)
{
    return std::bit_cast<Word>(approach_frontend_value_43d810(std::bit_cast<float>(value), std::bit_cast<float>(target),
                                                              std::bit_cast<float>(Word{0x3a83126f})));
}
} // namespace
SourceMenuMainNavigationChildren::SourceMenuMainNavigationChildren(
    MenuKeyboardAcquisition &keys, Win32MenuDirectInputStartup &input, CampaignScoreMusic &music,
    SessionOuterFrameServices &frames, PolygonSubmissionSink &sink, RaceTextureReplacement &textures,
    std::uint8_t &character)
    : SourceMenuPageDrawingChildren(keys, input, music, frames, sink, textures, character), music_(music)
{
}
Word SourceMenuMainNavigationChildren::menu_text_event(FrontendState &f, Word child, Word event, Word argument)
{
    return dispatch_frontend_menu_text_43c980(f, child, event, argument, *this, *this, *this);
}
void SourceMenuMainNavigationChildren::other_page_event(FrontendState &f, Word page, Word callback, Word event,
                                                        Word argument)
{
    if (callback == 0x453210)
    {
        (void)dispatch_frontend_main_page_453210(f, page, event, argument, *this, *this, music_, *this);
        return;
    }
    if (callback == 0x454da0)
    {
        (void)dispatch_frontend_options_page_454da0(f, page, event, *this, *this, music_, *this);
        return;
    }
    if (callback == 0x4556b0)
    {
        (void)dispatch_frontend_video_options_page_4556b0(f, page, event, *this, *this, music_, *this);
        return;
    }
    if (callback == 0x43c980)
    {
        (void)menu_text_event(f, page, event, argument);
        return;
    }
    if (callback == host_graphics_page_event_r23)
    { // NEW HOST (R23) GRAPHICS page
        (void)dispatch_host_graphics_page_r23(f, page, event, *this, music_, *this);
        return;
    }
    remaining_page_event(f, page, callback, event, argument);
}
Word dispatch_frontend_menu_text_43c980(FrontendState &f, Word child, Word event, Word argument,
                                        SourceNavigationPageServices &pages, SourceMainPageServices &calls,
                                        SourceMenuTextDrawServices &draw)
{
    f.shared_platform()->require_owner_thread();
    // Reached virtual method resolution, never cast a source address to a host function.
    if (page_word(f, child, 0x10) != 0x43c980)
        throw FormatError("Unreconstructed MenuText event callback identity");
    const auto count = page_word(f, child, 0x14),
               descriptor = page_word(f, child, 0x18); // 43C991/994 even unsupportedevent
    switch (event)
    {
    case 1:
        if (as_signed(page_word(f, child, 0xa4)) < 0)
            select_frontend_menu_text_43c930(f, child, first(f, descriptor), pages);
        break;
    case 2: {
        set_page_word(f, child, 0x1c, approach(page_word(f, child, 0x1c), page_word(f, child, 0x20)));
        Word tail = 0;
        if (as_signed(count - 1u) > 0)
        {
            tail = count - 1u;
            for (Word i = 0; i < tail; ++i)
                set_page_word(f, child, 0x24u + i * 4u, page_word(f, child, 0x28u + i * 4u));
        }
        set_page_word(f, child, 0x24u + tail * 4u, page_word(f, child, 0x1c));
        if (as_signed(page_word(f, child, 0xa8)) >= 0)
        {
            const auto age = page_word(f, child, 0xac) + 1u;
            set_page_word(f, child, 0xac, age);
            if (as_signed(age) > 6)
                set_page_word(f, child, 0xa8, 0xffffffffu);
        }
        // Same admitted PC53/round-nearest value policy as existing43D810.
        const auto phase = std::bit_cast<float>(page_word(f, child, 0xc4));
        const auto current = page_word(f, child, 0xc0);
        set_page_word(f, child, 0xc4,
                      std::bit_cast<Word>(vehicle_f32(double(phase) + double(std::bit_cast<float>(Word{0x3c75c28f})))));
        const auto value = approach(current, 0);
        const auto selected = page_word(f, child, 0xa4), rows = f.source.source_word(descriptor + 0x28u);
        set_page_word(f, child, 0xc0, value);
        if (as_signed(f.source.source_word(rows + selected * 32u)) < 0)
        {
            const auto selected_now = first(f, page_word(f, child, 0x18));
            select_frontend_menu_text_43c930(f, child, selected_now, pages);
            pages.feedback_440880(f, 9);
        }
        break;
    }
    case 3:
        draw.draw_menu_text_event3(f, child, count, descriptor);
        break;
    case 11:
    case 12: {
        const auto selected = page_word(f, child, 0xa4);
        if (event == 11 ? as_signed(selected) <= 0 : (as_signed(selected) < 0 || selected == count - 1u))
            break;
        const auto changed = event == 11 ? previous(f, descriptor, selected) : next(f, descriptor, selected);
        if (as_signed(changed) < 0)
            break;
        select_frontend_menu_text_43c930(f, child, changed, pages);
        calls.feedback_440890(f, event);
        return 1;
    }
    case 16: {
        const auto selected = page_word(f, child, 0xa4);
        if (as_signed(selected) < 0)
            break;
        set_page_word(f, child, 0xa8, selected);
        set_page_word(f, child, 0xac, 0);
        calls.feedback_440890(f, event);
        break;
    }
    case 19:
        set_page_word(f, child, 0x20, 0);
        pages.feedback_440880(f, 18);
        break;
    case 20:
        set_page_word(f, child, 0x20, 0x3f800000);
        pages.feedback_440880(f, 19);
        break;
    case 21: {
        const auto x = f.source.source_word(argument + 4u), y = f.source.source_word(argument + 8u);
        set_page_word(f, child, 0xb8, x);
        set_page_word(f, child, 0xbc, y);
        break;
    }
    case 22:
        set_page_word(f, child, 0xb0, 1);
        break;
    case 23:
        set_page_word(f, child, 0xb0, 0);
        break;
    case 24:
        set_page_word(f, child, 0xb4, 1);
        break;
    case 25:
        set_page_word(f, child, 0xb4, 0);
        break;
    case 26: {
        const auto selected = page_word(f, child, 0xa4);
        if (as_signed(selected) < 0)
            return 0xffffffffu;
        return f.source.source_word(f.source.source_word(descriptor + 0x28u) + selected * 32u);
    }
    case 27:
        return page_word(f, child, 0xa4);
    case 28:
        if (as_signed(argument) < 0 || as_signed(argument) >= as_signed(count))
            break;
        if (as_signed(f.source.source_word(f.source.source_word(descriptor + 0x28u) + argument * 32u)) < 0)
            break;
        select_frontend_menu_text_43c930(f, child, argument, pages);
        return 1;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
