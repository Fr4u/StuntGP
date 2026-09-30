#pragma once
#include "sgn/source_title_draw.hpp"

namespace sgn::retail
{
std::uint32_t allocate_frontend_command_438550(FrontendSourceStorage &, std::uint32_t bytes) noexcept;
void link_frontend_command_438590(FrontendSourceStorage &, std::uint32_t ecx, std::uint32_t edx, std::uint32_t command);
void queue_frontend_rectangle_438500(FrontendSourceStorage &, std::uint32_t ecx, std::uint32_t edx,
                                     const std::array<std::uint32_t, 12> &descriptor);
void clip_frontend_rectangle_438370(FrontendSourceStorage &, const std::array<std::uint32_t, 12> &descriptor,
                                    std::uint32_t layer, std::uint32_t argument);
class SourceMenuCommandQueueChildren : public SourceMenuTitleDrawChildren
{
  public:
    using SourceMenuTitleDrawChildren::SourceMenuTitleDrawChildren;
    void rectangle_438370(FrontendState &, const std::array<std::uint32_t, 12> &, std::uint32_t layer,
                          std::uint32_t argument) final;
};
} // namespace sgn::retail
