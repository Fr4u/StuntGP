#include "sgn/source_setup_page.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/source_frontend_packed_word.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
using sgn::retail::frontend_page_words::page_pool;
Word read(FrontendState &f, Word page, Word offset)
{
    const auto bytes = frontend_page_object_bytes(page_pool(f), page, offset + 4);
    Word value = 0;
    for (unsigned i = 0; i != 4; ++i)
        value |= std::to_integer<Word>(bytes[offset + i]) << (8 * i);
    return value;
}
void write(FrontendState &f, Word page, Word offset, Word value)
{
    const auto bytes = frontend_page_object_bytes(page_pool(f), page, offset + 4);
    for (unsigned i = 0; i != 4; ++i)
        bytes[offset + i] = std::byte((value >> (8 * i)) & 255u);
}
} // namespace

std::uint32_t create_frontend_setup_page_43e000(FrontendState &f, Word music_key)
{
    f.shared_platform()->require_owner_thread();
    const auto page = allocate_frontend_page_object_43d470(page_pool(f), 0x18); // 43E0C9
    const auto event = f.source.source_word(0x488e84);                          // 43E0CE: before REP clear
    for (Word offset = 0; offset != 0x18; offset += 4)
        write(f, page, offset, 0);                     // 43E0DF
    const auto close = f.source.source_word(0x488e80); // 43E0E1: fresh read after clear
    write(f, page, 4, 23);                             // 43E0E7
    write(f, page, 12, close);                         // 43E0EE
    write(f, page, 16, event);                         // 43E0F1
    write(f, page, 0, 0x479688);                       // 43E0F8: PageSetup name identity
    write(f, page, 8, 0);                              // 43E0FE
    write(f, page, 20, 0);                             // entire45A610 initializer: this store, return1
    write(f, page, 8, music_key);                      // 43ED94 after successfully returned initializer
    return page;
}

std::uint32_t dispatch_frontend_setup_page_45a630(FrontendState &f, Word page, Word event,
                                                  SourcePageFactoryServices &factory,
                                                  SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    auto word = [&](Word address) -> Word & { return f.source.source_word(address); };
    if (event == 19)
    {
        word(0x5ff46c) = 1;                                  // 45A647 ->4400A0
        set_frontend_packed_word_43dd10(f.source, 0x112a43); // 45A651
        word(0x5ff448) = 0;                                  // 45A65C ->43DD40, ECX=0, stack{0,0}
        word(0x5ff44c) = 0;
        word(0x5ff450) = 0;
        return 0;
    }
    if (event != 1)
        return 0; // other events do not read the page's stage
    switch (read(f, page, 20))
    {
    case 0:
        word(0x5e3c20) = 0;                                               // 45A682
        word(0x5e3c24) = 0;                                               // 45A687
        word(0x5e3c1c) = 1;                                               // 45A696
        word(0x5e3c28) = 1;                                               // 45A69C
        word(0x5e3c38) = 0xffffffffu;                                     // 45A6A2
        word(0x5e3c3c) = 0xffffffffu;                                     // 45A6A7
        (void)push_frontend_page_43dfa0(f, 0, 27, factory, pages, music); // 45A6AC
        write(f, page, 20, 1);                                            // 45A6B1: only after the child returns
        return 0;
    case 1: {
        const auto language = persistence_word(f.source.persistence.bytes, 0x5df5f0u - 0x5df560u); // 45A6BB
        const auto next = 2u + static_cast<Word>(language != 0); // SETNZ, not signed-positive
        write(f, page, 20, next);                                // 45A6D2: before the language page is constructed
        word(0x5e37b4) = 1;                                      // 45A6D5
        (void)push_frontend_page_43dfa0(f, 0, 29, factory, pages, music); // 45A6DF
        return 0;
    }
    case 2:
        word(0x5e3c1c) = 1;                                               // 45A6F2
        word(0x5e3c20) = 1;                                               // 45A6F8
        word(0x5e3c28) = 3;                                               // 45A70A
        word(0x5e3c24) = 0;                                               // 45A710
        word(0x5e3c38) = 0;                                               // 45A715
        word(0x5e3c3c) = 0xffffffffu;                                     // 45A71A
        (void)push_frontend_page_43dfa0(f, 0, 27, factory, pages, music); // 45A724
        write(f, page, 20, 3);                                            // 45A729: only after the child returns
        return 0;
    case 3:
        write(f, page, 20, 4); // 45A733; no title transition during this event
        return 0;
    case 4:
        (void)pages.select_page_43ef10(f, 1, 1); // 45A74A; discards EAX, then returns0
        return 0;
    default:
        return 0; // 45A66E unsigned JA
    }
}
} // namespace sgn::retail
