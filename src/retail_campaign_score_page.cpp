#include "sgn/retail_campaign_score_page.hpp"
#include "sgn/little_endian_words.hpp"
#include <algorithm>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using sgn::little_endian::read_le32;
using sgn::little_endian::write_le32;
std::uint32_t free_identity(FrontendPagePool pool, std::uint32_t distance)
{
    const auto available = read_le32(pool, 0x2080);
    if (available < distance || available > 16)
        throw std::invalid_argument("campaign score page lacks source pool capacity");
    const auto object = read_le32(pool, 0x2088 + (available - distance) * 4) + 8;
    (void)frontend_page_object_bytes(pool, object, 512);
    return object;
}
} // namespace
void initialize_campaign_score_page_45a360(CampaignScorePageViews view, std::uint32_t page)
{
    const auto parent = frontend_page_object_bytes(view.pool, page, 24);
    if (free_identity(view.pool, 1) == page)
        throw std::invalid_argument("campaign score child aliases its parent");
    const auto child_id = allocate_frontend_page_object_43d470(view.pool, 0xC8);
    const auto child = frontend_page_object_bytes(view.pool, child_id, 0xC8);
    std::fill(child.begin(), child.end(), std::byte{0});
    write_le32(child, 0xC, 0x43C970);
    write_le32(child, 0x10, 0x43C980);
    write_le32(child, 4, 0x29);
    write_le32(child, 0, 0x488E70);
    write_le32(child, 8, 0);
    // 43C860 with immutable488E38. The pinned488DF8 table has exactly one
    // nonzero text entry, id0/text0x17, and no selection-sound pointer.
    write_le32(child, 0xB8, 0x43A00000);
    write_le32(child, 0x18, 0x488E38);
    write_le32(child, 0xBC, 0x43D20000);
    write_le32(child, 0x1C, 0x3F800000);
    write_le32(child, 0x20, 0x3F800000);
    write_le32(child, 0xA4, 0);
    write_le32(child, 0xA8, 0xFFFFFFFF);
    write_le32(child, 0xAC, 0);
    write_le32(child, 0xB0, 0);
    write_le32(child, 0xB4, 0);
    write_le32(child, 0x14, 1);
    write_le32(child, 0x24, 0x3F800000);
    // 43C900 selects row0. Full43C930 for this source descriptor.
    write_le32(child, 0xC0, 0x3F800000);
    write_le32(child, 0xC4, 0);
    write_le32(child, 0xA4, 0);
    write_le32(parent, 0x14, child_id);
    // Actual43C980 indirect calls at45A3C9 and45A3D4: EDX18, then16.
    // Their stack argument is0; it is not their event selector.
    write_le32(child, 0xB4, 1);
    write_le32(child, 0xB0, 1);
    write_le32(view.action, 0, 0);
}
std::uint32_t create_campaign_score_page_43e000(CampaignScorePageViews view, std::uint32_t music_key)
{
    const auto parent_id = free_identity(view.pool, 1), child_id = free_identity(view.pool, 2);
    if (parent_id == child_id)
        throw std::invalid_argument("campaign score free list repeats a source block");
    const auto page = allocate_frontend_page_object_43d470(view.pool, 24);
    const auto bytes = frontend_page_object_bytes(view.pool, page, 24);
    std::fill(bytes.begin(), bytes.end(), std::byte{0});
    write_le32(bytes, 4, 0x25);
    write_le32(bytes, 0xC, 0x45A3F0);
    write_le32(bytes, 0x10, 0x45A410);
    write_le32(bytes, 0, 0x479898);
    write_le32(bytes, 8, 0);
    initialize_campaign_score_page_45a360(view, page);
    write_le32(bytes, 8, music_key); // 43ED94, common factory epilogue
    return page;
}
void destroy_campaign_score_children_45a3f0(FrontendPagePool pool, std::uint32_t page)
{
    const auto parent = frontend_page_object_bytes(pool, page, 24);
    const auto child_id = read_le32(parent, 0x14);
    if (child_id == page)
        throw std::invalid_argument("campaign score child aliases its parent");
    const auto child = frontend_page_object_bytes(pool, child_id, 0xC8);
    if (read_le32(child, 0xC) != 0x43C970)
        throw std::invalid_argument("campaign score child has an unreconstructed destructor");
    // 43C970 is a single original RET, followed by actual43D4C0.
    release_frontend_page_object_43d4c0(pool, child_id);
    write_le32(parent, 0x14, 0);
}
} // namespace sgn::retail
