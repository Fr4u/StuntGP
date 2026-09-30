#pragma once
#include "sgn/source_menu_command_consumption.hpp"

namespace sgn::retail
{
void queue_frontend_text_438990(FrontendSourceStorage &, std::uint32_t layer, std::uint32_t list_argument,
                                std::uint32_t x, std::uint32_t y, std::uint32_t style,
                                std::span<const std::uint8_t> text, std::uint32_t scale, std::uint32_t color,
                                std::uint32_t flags, std::uint32_t digit_advance);
double draw_frontend_aligned_text_438ab0(FrontendState &, const SourceFrontendTextCall<3> &);
void draw_frontend_text_box_439040(FrontendState &, const SourceFrontendTextCall<8> &);
class SourceMenuTextChildren : public SourceMenuConsumptionChildren
{
  public:
    using SourceMenuConsumptionChildren::SourceMenuConsumptionChildren;
    void text_box_439040(FrontendState &, const SourceFrontendTextCall<8> &) final;
    void aligned_text_438ab0(FrontendState &, const SourceFrontendTextCall<3> &) final;
};
} // namespace sgn::retail

namespace sgn::retail
{
// Same source implementations, with borrowed mutable original text instead of language identity.
double draw_frontend_aligned_borrowed_438ab0(FrontendState &, const SourceFrontendTextCall<3> &,
                                             std::span<std::uint8_t>);
void draw_frontend_box_borrowed_439040(FrontendState &, const SourceFrontendTextCall<8> &, std::span<std::uint8_t>);
} // namespace sgn::retail
