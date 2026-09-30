#include "sgn/source_language_page.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>

extern "C" std::uint32_t sgn_menu_option_fild_fstp(std::uint32_t) noexcept;
extern "C" void sgn_language_flag_xy_452ee1(const std::uint32_t *, std::uint32_t *) noexcept;

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::retail::frontend_page_words::page_pool;
Word read(FrontendState &f, Word page, Word offset)
{
    const auto b = frontend_page_object_bytes(page_pool(f), page, offset + 4);
    Word value = 0;
    for (unsigned i = 0; i != 4; ++i)
        value |= std::to_integer<Word>(b[offset + i]) << (8 * i);
    return value;
}
void write(FrontendState &f, Word page, Word offset, Word value)
{
    const auto b = frontend_page_object_bytes(page_pool(f), page, offset + 4);
    for (unsigned i = 0; i != 4; ++i)
        b[offset + i] = std::byte((value >> (8 * i)) & 255u);
}
Word language_setting(FrontendState &f)
{
    return persistence_word(f.source.persistence.bytes, 0x90);
}
void language_setting(FrontendState &f, Word value)
{
    persistence_word(f.source.persistence.bytes, 0x90, value);
}
void copy_eight_words(FrontendState &f, Word destination, Word source)
{
    for (Word offset = 0; offset != 32; offset += 4)
    {
        const auto value = f.source.source_word(source + offset);
        f.source.source_word(destination + offset) = value;
    }
}
Word initialize_frontend_language_page_452c00(FrontendState &f, Word page, SourceNavigationPageServices &pages,
                                              SourceMainPageServices &calls, SourceMenuUpdateChildren &language)
{
    write(f, page, 0x28, language_setting(f));                 // 452C07..0F
    const auto initial_count = f.source.source_word(0x5e38dc); // before the three stores
    write(f, page, 0x18, 0);
    write(f, page, 0x20, 0);
    write(f, page, 0x24, 0);
    Word row = 0;
    if (as_signed(initial_count) > 0)
    {
        do
        {
            const Word destination = 0x624bb8u + row * 32u;
            copy_eight_words(f, destination, 0x4879c8);             // 452C3B
            f.source.source_word(destination) = row;                // 452C43
            f.source.source_word(destination + 12u) = row + 0x2ddu; // 452C46
            const auto count = f.source.source_word(0x5e38dc);      // 452C49, before increment
            ++row;
            if (as_signed(row) >= as_signed(count))
                break;
        } while (true);
    }
    copy_eight_words(f, 0x624bb8u + row * 32u, 0x4879e8); // 452C69, sentinel after actual rows
    const auto child = allocate_frontend_page_object_43d470(page_pool(f), 0xc8);
    const auto close = f.source.source_word(0x4794e8); // 452C75 before clear
    for (Word offset = 0; offset != 0xc8; offset += 4)
        write(f, child, offset, 0);
    const auto event = f.source.source_word(0x4794ec); // 452C88 fresh after clear
    write(f, child, 12, close);
    write(f, child, 4, 41);
    write(f, child, 0, 0x487a40);
    write(f, child, 8, 0);
    write(f, child, 16, event);
    if (initialize_frontend_menu_text_43c860(f, child, 0x487a08, pages) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), child);
        write(f, page, 0x14, 0);
    }
    else
        write(f, page, 0x14, child);
    const auto retained_child = read(f, page, 0x14); // ESI is retained through both callbacks and selection
    (void)calls.menu_text_event(f, retained_child, 24, 0);
    (void)calls.menu_text_event(f, retained_child, 22, 0);
    const auto requested = language_setting(f); // 452CDD, before reading5E37B4
    write(f, page, 0x1c, f.source.source_word(0x5e37b4));
    if (requested == 0)
    {
        // Complete445AD0 is XOR EAX,EAX;RET. No Windows-language fallback.
        language_setting(f, 0);
        write(f, page, 0x1c, 0); // 452CF6..CFB then JZ452D19
    }
    else
    {
        const auto selected = language.select_language_440c40(f, requested);
        if (selected < 0)
        {
            language_setting(f, 0);
            write(f, page, 0x18, 0);
        }
        else
            write(f, page, 0x18, std::bit_cast<Word>(selected));
    }
    if (as_signed(f.source.source_word(0x5e38dc)) < 2)
        write(f, page, 0x1c, 1);
    const auto selected = read(f, page, 0x18); // captured before child event28
    (void)calls.menu_text_event(f, retained_child, 28, selected);
    const auto position = sgn_menu_option_fild_fstp(selected);
    write(f, page, 0x24, position);
    write(f, page, 0x20, position); // 452D44,452D47
    return 1;
}
} // namespace

std::uint32_t create_frontend_language_page_43e000(FrontendState &f, Word music_key,
                                                   SourceNavigationPageServices &pages, SourceMainPageServices &calls,
                                                   SourceMenuUpdateChildren &language)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x2c); // 43E98E
    const auto close = f.source.source_word(0x487a38);                          // before REP clear
    for (Word offset = 0; offset != 0x2c; offset += 4)
        write(f, page, offset, 0);
    write(f, page, 8, 0);                              // 43E9A6
    const auto event = f.source.source_word(0x487a3c); // 43E9A9
    write(f, page, 12, close);
    write(f, page, 4, 29);
    write(f, page, 0, 0x479808);
    write(f, page, 16, event);
    if (initialize_frontend_language_page_452c00(f, page, pages, calls, language) == 0)
    {
        release_frontend_page_object_43d4c0(page_pool(f), page);
        return 0;
    }
    write(f, page, 8, music_key); // 43ED94, after complete initializer
    return page;
}

void destroy_frontend_language_page_452d50(FrontendState &f, Word page)
{
    f.shared_platform()->require_owner_thread();
    const auto child = read(f, page, 0x14); // retained ESI across actual child destructor
    if (read(f, child, 12) != 0x43c970u)
        throw FormatError("452D50 reached an unreconstructed MenuText destructor");
    // 43C970 is RET. Source452D5E frees the retained child, not a reread of parent+14.
    release_frontend_page_object_43d4c0(page_pool(f), child);
    write(f, page, 0x14, 0);
}

std::uint32_t dispatch_frontend_language_page_452d90(FrontendState &f, Word page, Word event,
                                                     SourceNavigationPageServices &pages, CampaignScoreMusic &music,
                                                     SourceMainPageServices &calls, SourceTitlePageServices &draw,
                                                     SourceMenuUpdateChildren &language)
{
    f.shared_platform()->require_owner_thread();
    const auto choose = [&](Word selected) {
        const auto flag = f.source.source_word(0x5e37dcu + selected * 36u); // 452EA2, wrapping DWORD arithmetic
        (void)select_source_menu_language_452d70(f, flag, language);        // existing full helper
        (void)pop_frontend_page_43eef0(f, pages, music);
    };
    switch (event)
    {
    case 1:
        if (read(f, page, 0x1c) != 0)
            choose(read(f, page, 0x18));
        break;
    case 2: {
        const auto child = read(f, page, 0x14);
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selected = calls.menu_text_event(f, child, 26, 0);
        const auto current = read(f, page, 0x20); // 452E68 before FILD
        const auto target = sgn_menu_option_fild_fstp(selected);
        write(f, page, 0x24, target); // 452E79
        const auto next = approach_frontend_value_43d810(std::bit_cast<float>(current), std::bit_cast<float>(target),
                                                         std::bit_cast<float>(Word{0x3a83126f}));
        write(f, page, 0x20, std::bit_cast<Word>(next));
        break;
    }
    case 3: {
        (void)calls.menu_text_event(f, read(f, page, 0x14), event, 0); // widget draw happens first
        if (as_signed(f.source.source_word(0x5e38dc)) <= 0)
            break;
        Word index = 0, flag_address = 0x5e37dc;
        do
        {
            const auto phase = read(f, page, 0x20);               // 452EF6
            const auto flag = f.source.source_word(flag_address); // 452EF9, retained across both sprites
            const auto divisor = f.source.source_word(0x5e38dc);  // 452F05 fresh read
            const std::array<Word, 3> numeric{index, phase, divisor};
            std::array<Word, 2> xy;
            sgn_language_flag_xy_452ee1(numeric.data(), xy.data());
            draw.sprite_438710(f, 13, flag + 20u, {xy[0], xy[1], 0x3f800000u, 0x203});
            draw.sprite_438710(f, 12, flag + 12u, {xy[0], xy[1], 0x3f800000u, 0x103});
            const auto count = f.source.source_word(0x5e38dc); // 452F5E after both actual draw children
            ++index;
            flag_address += 36u;
            if (as_signed(index) >= as_signed(count))
                break;
        } while (true);
        break;
    }
    case 16: {
        const auto child = read(f, page, 0x14);
        (void)calls.menu_text_event(f, child, event, 0);
        const auto selected = calls.menu_text_event(f, child, 26, 0);
        write(f, page, 0x18, selected);
        choose(selected);
        break;
    }
    case 17: {
        const auto previous = read(f, page, 0x28);
        if (as_signed(previous) > 0)
        {
            (void)select_source_menu_language_452d70(f, previous, language);
            calls.feedback_440890(f, event);
            (void)pop_frontend_page_43eef0(f, pages, music);
        }
        else
            pages.feedback_440880(f, 9);
        break;
    }
    case 19:
        f.source.source_word(0x5ff46c) = 1;
        set_frontend_packed_word_43dd10(f.source, 0x114311);
        f.source.source_word(0x5ff448) = 93;
        f.source.source_word(0x5ff44c) = 0x3e99999a;
        f.source.source_word(0x5ff450) = 0xbf000000;
        [[fallthrough]];
    case 11:
    case 12:
    case 20:
        (void)calls.menu_text_event(f, read(f, page, 0x14), event, 0);
        break;
    default:
        break;
    }
    return 0;
}
} // namespace sgn::retail
