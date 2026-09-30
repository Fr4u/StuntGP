#include "sgn/source_menu_widget_draw.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_graphics_page.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
namespace sgn::retail
{
extern "C" void sgn_menu_widget_y_43ce02(const std::uint32_t *, std::uint32_t *);
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
W store(double v)
{
    return std::bit_cast<W>(vehicle_f32(v));
}
using sgn::retail::frontend_page_words::page_word;
} // namespace
void draw_frontend_menu_text_event3(FrontendState &f, W child, W count, W descriptor, SourceFrontendDrawServices &calls)
{
    f.shared_platform()->require_owner_thread();
    const auto dw = [&](W offset) { return f.source.source_word(descriptor + offset); };
    const auto x = page_word(f, child, 0xb8), scale = dw(0x14);
    // 438950(1.0) returns16. Source multiplies then doubles before the single FSTP32.
    const auto spacing = store((16.0 * double(as_float(scale))) * 2.0);
    const auto last = count - 1u;
    volatile double spread = double(as_signed(last)) * double(as_float(spacing));
    auto y = store(double(as_float(page_word(f, child, 0xbc))) - spread * 0.5);
    if (as_signed(count) > 0)
    {
        const auto flags = dw(0x20) | 0x40u, box_scale = dw(0x1c);
        auto row = dw(0x28);
        W index = 0;
        do
        {
            auto color = f.source.source_word(row + 4u);
            W style = 0;
            if (as_signed(f.source.source_word(row)) < 0)
                style = 6;
            else if (page_word(f, child, 0xb4) != 0)
            {
                if (index == page_word(f, child, 0xa8))
                {
                    style = 5;
                    color = 0xffff0000u;
                }
                else if (index == page_word(f, child, 0xa4))
                {
                    style = 4;
                    color = f.source.source_word(row + 8u);
                }
            }
            const auto animation = page_word(f, child, 0x24u + index * 4u);
            const auto alpha = store(double(as_float(animation)) * 4.0);
            volatile double py = double(as_float(dw(8))) * double(as_float(animation));
            const auto actual_y = store(py * 480.0 + double(as_float(y)));
            volatile double px = double(as_float(dw(4))) * double(as_float(animation));
            const auto actual_x = store(px * 640.0 + double(as_float(x)));
            const std::array<W, 11> text{actual_x,  actual_y, f.source.source_word(row + 0xc),
                                         box_scale, color,    f.source.source_word(row + 0x14),
                                         style,     scale,    flags,
                                         dw(0x24),  alpha};
            // NEW HOST (R23): host GRAPHICS row texts use the same439040 box.
            if (!draw_host_menu_text_438ff0_r23(f, dw(0), dw(0x18), text))
                draw_frontend_text_438ff0(f, dw(0), dw(0x18), text, calls);
            y = store(double(as_float(spacing)) + double(as_float(y)));
            ++index;
            row += 32u;
            // 43CDA6 reads count again after the draw child; original entry count is not reused here.
        } while (as_signed(index) < as_signed(page_word(f, child, 0x14)));
    }
    const auto selected = page_word(f, child, 0xa4);
    if (as_signed(selected) >= 0 && page_word(f, child, 0xb0) != 0 && page_word(f, child, 0xb4) != 0)
    {
        const auto description = f.source.source_word(dw(0x28) + selected * 32u + 0x10u);
        const std::array<W, 11> help{
            0x43a00000u, 0x43d20000u, description, 0x3fa00000u, 0xffffffffu, 4, 0, 0x3f800000u, 0x41, 0xfffffe70u, 0};
        if (description != 0 && !draw_host_menu_text_438ff0_r23(f, 2, 4, help)) // NEW HOST (R23) help lines
            draw_frontend_text_438ff0(f, 2, 4, help, calls);
    }
    const auto sprite_x = store(150.0 - double(as_float(page_word(f, child, 0xc0))) * 320.0);
    const auto phase = page_word(f, child, 0xc4);
    const auto integer = static_cast<W>(frontend_ftol_f32_462860(phase));
    const std::array<W, 2> phase_input{phase, integer};
    W sprite_y;
    // Retain source FTOL lowDWORD and the original FSIN instruction, with an unspilled fraction.
    auto row = dw(0x28) + selected * 32u;
    const auto overlay = f.source.source_word(row + 0x1c);
    sgn_menu_widget_y_43ce02(phase_input.data(), &sprite_y);
    if (overlay != 0)
        draw_frontend_sprite_438710(f, 12, overlay, {sprite_x, sprite_y, 0x3f800000u, 0x203}, calls);
    const auto sprite = f.source.source_word(row + 0x18);
    if (sprite != 0)
    {
        const auto flags = f.source.source_word(row + 0x1c) != 0 ? 0x103u : 3u;
        draw_frontend_sprite_438710(f, 11, sprite, {sprite_x, sprite_y, 0x3f800000u, flags}, calls);
    }
}
void draw_frontend_main_background_4392b0(FrontendState &f, W ecx, W edx, SourceFrontendDrawServices &calls)
{
    f.shared_platform()->require_owner_thread();
    if (edx != 0)
        throw FormatError("4392C0 formatted tooltip to5DF414 is not bound by the main-page zero-argument adapter");
    // 4392B0 pushes two zero coordinates;4392C0 adds320 and50, then439040 directly.
    const auto text = frontend_language_text_441170(f.source.language_strings(), ecx);
    calls.text_box_439040(
        f,
        SourceFrontendTextCall<8>{
            2, 4, 0x43a00000u, 0x42480000u, text, {0x3fa00000u, 0xffffffffu, 3, 0, 0x3f800000u, 0x41, 0xffffff10u, 0}});
}
void SourceMenuWidgetDrawChildren::draw_menu_text_event3(FrontendState &f, W child, W count, W descriptor)
{
    draw_frontend_menu_text_event3(f, child, count, descriptor, *this);
}
void SourceMenuWidgetDrawChildren::background_4392b0(FrontendState &f, W ecx, W edx)
{
    draw_frontend_main_background_4392b0(f, ecx, edx, *this);
}
} // namespace sgn::retail
