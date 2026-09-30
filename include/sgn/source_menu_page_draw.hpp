#pragma once
#include "sgn/source_menu_navigation_binding.hpp"
namespace sgn::retail
{
using SourceMenuRect = std::array<std::uint32_t, 4>;
// Required unreconstructed lower children. They must execute their source
// paths on the same frontend queue; no no-op/default implementation.
struct SourceMenuPageDrawServices
{
    virtual ~SourceMenuPageDrawServices() = default;
    virtual void unavailable_43ff90(FrontendState &, std::uint32_t player) = 0;
    virtual void frame_439450(FrontendState &, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect,
                              std::uint32_t flags) = 0;
    virtual void panel_4398a0(FrontendState &, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect,
                              std::uint32_t flags, std::array<std::uint32_t, 6> bars) = 0;
    virtual void fill_439380(FrontendState &, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect,
                             std::uint32_t flags, std::uint32_t alpha) = 0;
};
void draw_frontend_tiled_438830(FrontendState &, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect,
                                std::array<std::uint32_t, 2> scroll);
void draw_frontend_outside_43feb0(FrontendState &, std::uint32_t descriptor, SourceMenuRect,
                                  std::array<std::uint32_t, 2> scroll);
void shrink_frontend_rect_43ff40(SourceMenuRect &);
void draw_frontend_pages_43f6b0(FrontendState &, SourceMenuPageDrawServices &, SourceNavigationPageServices &);
class SourceMenuPageDrawChildren : public SourceMenuNavigationBindingChildren, public SourceMenuPageDrawServices
{
  public:
    using SourceMenuNavigationBindingChildren::SourceMenuNavigationBindingChildren;
    void draw_pages_43f6b0(FrontendState &) final;
    void fill_439380(FrontendState &, std::uint32_t, std::uint32_t, SourceMenuRect, std::uint32_t,
                     std::uint32_t) override;
};
} // namespace sgn::retail
