#include "sgn/source_menu_fill.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
extern "C" std::uint32_t source_menu_fill_alpha_x87(std::uint32_t bits);
extern "C" void source_menu_fill_geometry_x87(std::uint32_t *descriptor, const std::uint32_t *rectangle,
                                              std::uint32_t row);

void draw_frontend_fill_439380(FrontendState &frontend, std::uint32_t layer, std::uint32_t row,
                               SourceMenuRect rectangle, std::uint32_t flags, std::uint32_t alpha)
{
    frontend.shared_platform()->require_owner_thread();
    // 43938C..439394:441320 copies descriptor118, exactly12 DWORDs.
    std::array<std::uint32_t, 12> descriptor;
    for (std::size_t i = 0; i < descriptor.size(); ++i)
        descriptor[i] = frontend.source.frontend_descriptors[118 * 12 + i];
    descriptor[0] = 2;
    descriptor[11] = 255;
    // 4393A3..4393F2: first matching bit wins, no combined blending policy.
    if ((flags & 0x100u) != 0)
        descriptor[10] = 0x58;
    else if ((flags & 0x200u) != 0)
        descriptor[10] = 0x70;
    else if ((flags & 0x400u) != 0)
    {
        descriptor[11] = source_menu_fill_alpha_x87(alpha);
        descriptor[10] = 0x60;
    }
    else
        descriptor[10] = 0x20;
    source_menu_fill_geometry_x87(descriptor.data(), rectangle.data(), row);
    clip_frontend_rectangle_438370(frontend.source, descriptor, layer, 0);
}
void SourceMenuPageDrawChildren::fill_439380(FrontendState &f, std::uint32_t layer, std::uint32_t row,
                                             SourceMenuRect rectangle, std::uint32_t flags, std::uint32_t alpha)
{
    draw_frontend_fill_439380(f, layer, row, rectangle, flags, alpha);
}
} // namespace sgn::retail
