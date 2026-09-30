#include "sgn/source_title_draw.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
extern "C" std::uint32_t source_frontend_sprite_alpha_438759(std::uint32_t) noexcept;
namespace
{
using sgn::bit_reinterpret::as_float;
std::uint32_t stored(double v)
{
    return std::bit_cast<std::uint32_t>(vehicle_f32(v));
}
} // namespace
void draw_frontend_sprite_438710(FrontendState &frontend, std::uint32_t layer, std::uint32_t id,
                                 std::array<std::uint32_t, 4> stack, SourceFrontendDrawServices &calls)
{
    draw_frontend_sprite_438740(frontend, layer, id, {stack[0], stack[1], stack[2], stack[3], 0x3f800000}, calls);
}
void draw_frontend_sprite_438740(FrontendState &frontend, std::uint32_t layer, std::uint32_t id,
                                 std::array<std::uint32_t, 5> stack, SourceFrontendDrawServices &calls)
{
    if (id == 0)
        return; // 438749, no descriptor access
    if (id >= 387)
        throw FormatError("frontend sprite descriptor outside existing bank");
    std::array<std::uint32_t, 12> descriptor;
    // 441320 copies the current initialized descriptor; never synthesize material IDs.
    for (unsigned i = 0; i < 12; ++i)
        descriptor[i] = frontend.source.frontend_descriptors[id * 12u + i];
    descriptor[0] = 2;
    // 438759 multiplies the caller alpha by PE46E1E4=255 then full462860.
    // No clamp: PageArcadeScore supplies its actual fading badge word.
    descriptor[11] = source_frontend_sprite_alpha_438759(stack[4]);
    const auto flags = stack[3];
    if ((flags & 0x100u) != 0)
        descriptor[10] = 0x58;
    else if ((flags & 0x200u) != 0)
        descriptor[10] = 0x70;
    else
        descriptor[10] = ((flags & 0x400u) | 0x200u) >> 4;
    const double scale = as_float(stack[2]);
    const double width = double(as_float(descriptor[3])) * scale;
    descriptor[1] = stack[0];
    descriptor[2] = stack[1];
    descriptor[3] = stored(width);
    descriptor[4] = stored(double(as_float(descriptor[4])) * scale);
    if ((flags & 1u) != 0)
        descriptor[1] = stored(double(as_float(stack[0])) - double(as_float(descriptor[3])) * 0.5);
    else if ((flags & 4u) != 0)
        descriptor[1] = stored(double(as_float(stack[0])) - double(as_float(descriptor[3])));
    if ((flags & 2u) != 0)
        descriptor[2] = stored(double(as_float(stack[1])) - double(as_float(descriptor[4])) * 0.5);
    else if ((flags & 8u) != 0)
        descriptor[2] = stored(double(as_float(stack[1])) - double(as_float(descriptor[4])));
    if ((flags & 0x10u) != 0)
    {
        const auto old = descriptor[5];
        descriptor[5] = descriptor[7];
        descriptor[7] = old;
    }
    if ((flags & 0x20u) != 0)
    {
        const auto old = descriptor[6];
        descriptor[6] = descriptor[8];
        descriptor[8] = old;
    }
    calls.rectangle_438370(frontend, descriptor, layer, 0); // 43881D..438824
}
void draw_frontend_text_438ff0(FrontendState &frontend, std::uint32_t ecx, std::uint32_t edx,
                               std::array<std::uint32_t, 11> stack, SourceFrontendDrawServices &calls)
{
    // 43901C: ECX is the original third stack word, not the incoming ECX.
    const auto text = frontend_language_text_441170(frontend.source.language_strings(), stack[2]);
    SourceFrontendTextCall<8> call{ecx, edx, stack[0], stack[1], text, {}};
    for (unsigned i = 0; i < 8; ++i)
        call.tail[i] = stack[i + 3];
    calls.text_box_439040(frontend, call);
}
void draw_frontend_text_438ba0(FrontendState &frontend, std::uint32_t ecx, std::uint32_t edx,
                               std::array<std::uint32_t, 6> stack, SourceFrontendDrawServices &calls)
{
    const auto text = frontend_language_text_441170(frontend.source.language_strings(), stack[2]);
    calls.aligned_text_438ab0(frontend, {ecx, edx, stack[0], stack[1], text, {stack[3], stack[4], stack[5]}});
}
void SourceMenuTitleDrawChildren::sprite_438710(FrontendState &f, std::uint32_t ecx, std::uint32_t edx,
                                                std::array<std::uint32_t, 4> stack)
{
    draw_frontend_sprite_438710(f, ecx, edx, stack, *this);
}
void SourceMenuTitleDrawChildren::text_438ff0(FrontendState &f, std::uint32_t ecx, std::uint32_t edx,
                                              std::array<std::uint32_t, 11> stack)
{
    draw_frontend_text_438ff0(f, ecx, edx, stack, *this);
}
void SourceMenuTitleDrawChildren::text_438ba0(FrontendState &f, std::uint32_t ecx, std::uint32_t edx,
                                              std::array<std::uint32_t, 6> stack)
{
    draw_frontend_text_438ba0(f, ecx, edx, stack, *this);
}
} // namespace sgn::retail
