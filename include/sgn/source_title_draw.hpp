#pragma once
#include "sgn/source_title_page.hpp"
#include "sgn/retail_frontend_language.hpp"

namespace sgn::retail
{
template <std::size_t TailWords> struct SourceFrontendTextCall
{
    std::uint32_t ecx, edx, x, y;
    FrontendLanguageTextIdentity text;         // borrowed existing language identity, not copied text
    std::array<std::uint32_t, TailWords> tail; // original stack args4..N
};
struct SourceFrontendDrawServices
{
    virtual ~SourceFrontendDrawServices() = default;
    // Must execute the source menu clip/command path, not the race polygon queue.
    virtual void rectangle_438370(FrontendState &, const std::array<std::uint32_t, 12> &, std::uint32_t layer,
                                  std::uint32_t argument) = 0;
    // Resolve text on the given frontend and use its platform's existing font
    // metrics/materials/queue. Do not construct fonts or copy a language arena.
    virtual void text_box_439040(FrontendState &, const SourceFrontendTextCall<8> &) = 0;
    virtual void aligned_text_438ab0(FrontendState &, const SourceFrontendTextCall<3> &) = 0;
};
// Full438740 consumes alpha as its fifth stack word, without a clamp.
void draw_frontend_sprite_438740(FrontendState &, std::uint32_t layer, std::uint32_t descriptor,
                                 std::array<std::uint32_t, 5> stack, SourceFrontendDrawServices &);
// Full438710 appends alpha1 and forwards to the same438740 implementation.
void draw_frontend_sprite_438710(FrontendState &, std::uint32_t layer, std::uint32_t descriptor,
                                 std::array<std::uint32_t, 4> stack, SourceFrontendDrawServices &);
void draw_frontend_text_438ff0(FrontendState &, std::uint32_t, std::uint32_t, std::array<std::uint32_t, 11>,
                               SourceFrontendDrawServices &);
void draw_frontend_text_438ba0(FrontendState &, std::uint32_t, std::uint32_t, std::array<std::uint32_t, 6>,
                               SourceFrontendDrawServices &);
class SourceMenuTitleDrawChildren : public SourceMenuTitleChildren, public SourceFrontendDrawServices
{
  public:
    using SourceMenuTitleChildren::SourceMenuTitleChildren;
    void sprite_438710(FrontendState &, std::uint32_t, std::uint32_t, std::array<std::uint32_t, 4>) final;
    void text_438ff0(FrontendState &, std::uint32_t, std::uint32_t, std::array<std::uint32_t, 11>) final;
    void text_438ba0(FrontendState &, std::uint32_t, std::uint32_t, std::array<std::uint32_t, 6>) final;
};
} // namespace sgn::retail
