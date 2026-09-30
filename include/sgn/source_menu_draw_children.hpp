#pragma once
#include "sgn/source_menu_page_draw.hpp"
namespace sgn::retail
{
void draw_frontend_frame_439450(FrontendState &, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect,
                                std::uint32_t flags);
void draw_frontend_panel_4398a0(FrontendState &, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect,
                                std::uint32_t flags, std::array<std::uint32_t, 6> bars);
void draw_frontend_unavailable_43ff90(FrontendState &, std::uint32_t player);
class SourceMenuPageDrawingChildren : public SourceMenuPageDrawChildren
{
  public:
    using SourceMenuPageDrawChildren::SourceMenuPageDrawChildren;
    void unavailable_43ff90(FrontendState &, std::uint32_t) final;
    void frame_439450(FrontendState &, std::uint32_t, std::uint32_t, SourceMenuRect, std::uint32_t) final;
    void panel_4398a0(FrontendState &, std::uint32_t, std::uint32_t, SourceMenuRect, std::uint32_t,
                      std::array<std::uint32_t, 6>) final;
};
} // namespace sgn::retail
