#include "sgn/retail_campaign_score_navigation.hpp"
#include "sgn/little_endian_words.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
} // namespace
void validate_campaign_score_navigation(CampaignScoreNavigationViews view)
{
    const auto nav = view.navigation;
    const auto pool = view.page.pool;
    const auto current = read_le32(nav, 8), pending = read_le32(nav, 0x10), depth = read_le32(nav, 0x58);
    // Host domain checks precede source mutations. The real state10 caller is
    // the PageGameLoop; other current-page callbacks need their own recovery.
    if (current)
    {
        const auto parent = frontend_page_object_bytes(pool, current, 24);
        if (read_le32(parent, 4) != 0x1F || read_le32(parent, 0x10) != 0x452FE0)
            throw std::invalid_argument("campaign score navigation has an unreconstructed current page");
    }
    if (pending)
        (void)frontend_page_object_bytes(pool, pending, 24);
    if ((current || pending) && depth >= 16)
        throw std::invalid_argument("campaign score navigation exceeds source page stack");
    const auto available = read_le32(pool, 0x2080);
    if (available < 2 || available > 16)
        throw std::invalid_argument("campaign score navigation lacks source pool capacity");
    const auto next = read_le32(pool, 0x2088 + (available - 1) * 4) + 8;
    const auto child = read_le32(pool, 0x2088 + (available - 2) * 4) + 8;
    (void)frontend_page_object_bytes(pool, next, 512);
    (void)frontend_page_object_bytes(pool, child, 512);
    if (next == child || next == current || next == pending || child == current || child == pending)
        throw std::invalid_argument("campaign score free blocks alias retained source objects");
}
std::uint32_t navigate_to_campaign_score_43dfa0(CampaignScoreNavigationViews view, CampaignScoreMusic &music)
{
    validate_campaign_score_navigation(view);
    const auto nav = view.navigation;
    const auto current = read_le32(nav, 8), pending = read_le32(nav, 0x10), depth = read_le32(nav, 0x58);
    // 43DFA0 sets the retention flag before calling the factory. Therefore its
    // 43EE40 call never destroys the old pending object on this exact path.
    if (pending)
    {
        write_le32(nav, 0x14, 1);
        write_le32(nav, 0x18 + depth * 4, pending);
        write_le32(nav, 0x58, depth + 1);
    }
    else if (current)
    {
        write_le32(nav, 0xC, 1);
        write_le32(nav, 0x18 + depth * 4, current);
        write_le32(nav, 0x58, depth + 1);
    }
    const auto page = create_campaign_score_page_43e000(view.page, 44);
    write_le32(nav, 0x10, 0); // 43EE64..43EE69
    const auto state = std::bit_cast<std::int32_t>(read_le32(nav, 0));
    if (state < 1 || state > 3)
        return 0;
    // Factory43ED94 has published page+8=44;43EEE0 resolves its music selector.
    if (view.requested_music != view.last_music)
    {
        music.start(view.requested_music, 1); // 43DC30 tailcall45F8F0
        view.last_music = view.requested_music;
    }
    write_le32(nav, 0x10, page);
    write_le32(nav, 0x14, 0);
    // If current exists, source43EEC3 calls452FE0 withEDX14 and stack argument0.
    // That callback's actual tests(EDX1,EDX13) both fail; its path writes nothing.
    write_le32(nav, 0, 2);
    return 1;
}
} // namespace sgn::retail
