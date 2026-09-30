#include "sgn/source_menu_shutdown.hpp"
#include "sgn/source_main_page.hpp"
#include "sgn/source_frontend_resource_memory.hpp"
#include "sgn/race_session.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
void require_owner(FrontendState &f)
{
    const auto platform = f.shared_platform();
    platform->require_owner_thread();
    if (&platform->state() != &f.world())
        throw FormatError("menu shutdown belongs to another source world");
}
using sgn::retail::frontend_page_words::page_pool;
} // namespace
void close_frontend_pages_43de50(FrontendState &f, SourceNavigationPageServices &pages)
{
    require_owner(f);
    auto word = [&](std::uint32_t a) -> std::uint32_t & { return f.source.source_word(a); };
    if (word(0x5ff3f0) != 0 && word(0x5ff3f4) == 0)
    {
        pages.close_page(f, word(0x5ff3f0));                               // 43DE63.
        release_frontend_page_object_43d4c0(page_pool(f), word(0x5ff3f0)); // 43DE66 rereads after callback.
        word(0x5ff3f0) = 0; // 43DE71, after free; ownership flag remains unchanged.
    }
    if (word(0x5ff3f8) != 0 && word(0x5ff3fc) == 0)
    {
        pages.close_page(f, word(0x5ff3f8)); // 43DE8E, fresh state after current-page teardown.
        release_frontend_page_object_43d4c0(page_pool(f), word(0x5ff3f8));
        word(0x5ff3f8) = 0;
    }
    std::uint32_t index = 0;
    while (std::bit_cast<std::int32_t>(index) < std::bit_cast<std::int32_t>(word(0x5ff440)))
    {
        // Host boundary for the existing16-slot source bank, not a count clamp.
        if (index >= 16)
            throw FormatError("43DEB8 shutdown stack leaves its existing source owner");
        const auto at = 0x5ff400u + index * 4u;
        pages.close_page(f, word(at));
        release_frontend_page_object_43d4c0(page_pool(f), word(at)); // 43DEBD rereads after destructor.
        word(at) = 0; // 43DEC4, only this slot. Count is reread each iteration, never reset.
        ++index;
    }
}
void close_frontend_menu_43d180(FrontendState &f, SourceNavigationPageServices &pages)
{
    require_owner(f);
    f.source.initializing = 0; // 43D180 actual5FD250, NOT scene.menu_initialized60D61C.
    // 4408F0=C3 RET in pinned PE, no omitted work.
    close_frontend_pages_43de50(f, pages); // 43D18F.
    // 436FB0=C3;437EA0 jumps441200=C3. Both are complete source leaves.
    f.resource_memory().dispose_440c20(); // 43D19E, existing sole6087A0 allocation owner.
    // 43D460=C3. Final43D1A8 jumps437E50=C3; the tail is absent from pseudocode.
}
} // namespace sgn::retail
