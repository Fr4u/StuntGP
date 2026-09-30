#include "sgn/source_menu_page_draw.hpp"
#include "sgn/retail_frontend_clip.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/frontend_page_words.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_signed;
std::uint32_t store(double v)
{
    return std::bit_cast<std::uint32_t>(vehicle_f32(v));
}
using sgn::retail::frontend_page_words::source_word;
constexpr SourceMenuRect full{0, 0, 0x44200000u, 0x43f00000u};
SourceMenuRect interpolate_43d960(const SourceMenuRect &start, const SourceMenuRect &end, std::uint32_t amount)
{
    SourceMenuRect result;
    for (unsigned i = 0; i < 4; ++i)
    {
        volatile double delta = (double(as_float(end[i])) - double(as_float(start[i]))) * double(as_float(amount));
        result[i] = store(delta + double(as_float(start[i])));
    }
    return result;
}
std::uint32_t page_kind(FrontendState &f, std::uint32_t page)
{
    auto b = frontend_page_object_bytes(std::as_writable_bytes(std::span{f.source.page_pool}), page, 8);
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
        v |= std::uint32_t(std::to_integer<std::uint8_t>(b[4 + i])) << (i * 8);
    return v;
}
} // namespace
void draw_frontend_tiled_438830(FrontendState &f, std::uint32_t layer, std::uint32_t descriptor, SourceMenuRect r,
                                std::array<std::uint32_t, 2> scroll)
{
    if (descriptor == 0)
        return;
    if (descriptor >= 387)
        throw FormatError("frontend tiled descriptor outside existing bank");
    std::array<std::uint32_t, 12> d;
    for (unsigned i = 0; i < 12; ++i)
        d[i] = f.source.frontend_descriptors[descriptor * 12 + i];
    d[0] = 2;
    d[10] = 0x20;
    d[11] = 255;
    const auto xi = static_cast<std::uint32_t>(frontend_ftol_f32_462860(scroll[0]));
    const double x_fraction = double(as_float(scroll[0])) - double(as_signed(xi));
    const auto yi = static_cast<std::uint32_t>(frontend_ftol_f32_462860(scroll[1]));
    const auto y_fraction = store(double(as_float(scroll[1])) - double(as_signed(yi))); // 438893 spill, x is unspilled
    d[2] = r[1];
    d[1] = r[0];
    d[3] = store(double(as_float(r[2])) - double(as_float(r[0])));
    d[4] = store(double(as_float(r[3])) - double(as_float(r[1])));
    d[5] = store((double(as_float(r[0])) * 0.015625 - x_fraction) * 1024.0);
    d[6] = store((double(as_float(r[1])) * 0.015625 - double(as_float(y_fraction))) * 1024.0);
    d[7] = store((double(as_float(r[2])) * 0.015625 - x_fraction) * 1024.0);
    d[8] = store((double(as_float(r[3])) * 0.015625 - double(as_float(y_fraction))) * 1024.0);
    clip_frontend_rectangle_438370(f.source, d, layer, 0);
}
void draw_frontend_outside_43feb0(FrontendState &f, std::uint32_t descriptor, SourceMenuRect r,
                                  std::array<std::uint32_t, 2> scroll)
{
    draw_frontend_tiled_438830(f, 16, descriptor, {0, 0, r[0], full[3]}, scroll);
    draw_frontend_tiled_438830(f, 16, descriptor, {r[2], 0, full[2], full[3]}, scroll);
    draw_frontend_tiled_438830(f, 16, descriptor, {0, 0, full[2], r[1]}, scroll);
    draw_frontend_tiled_438830(f, 16, descriptor, {0, r[3], full[2], full[3]}, scroll);
}
void shrink_frontend_rect_43ff40(SourceMenuRect &r)
{
    const double x = (double(as_float(r[0])) + double(as_float(r[2]))) * 0.5;
    const float y = vehicle_f32((double(as_float(r[1])) + double(as_float(r[3]))) * 0.5); // 43FF5B
    r[0] = store(x - 24.0);
    r[1] = store(double(y) - 24.0);
    r[2] = store(x + 24.0);
    r[3] = store(double(y) + 24.0);
}
void draw_frontend_pages_43f6b0(FrontendState &f, SourceMenuPageDrawServices &draw, SourceNavigationPageServices &pages)
{
    f.shared_platform()->require_owner_thread();
    const std::array scroll{source_word(f, 0x5ff454), source_word(f, 0x5ff458)};
    set_frontend_clip_438190(f.source, full);
    auto clip = full;
    if (source_word(f, 0x5ff4a4))
    {
        draw.unavailable_43ff90(f, source_word(f, 0x5ff4a0));
        source_word(f, 0x5ff460) = 0;
        return;
    }
    const auto layout = source_word(f, 0x5ff46c);
    if (layout == 1)
    {
        host_wide_menu::QueueScope wide(host_wide_menu::Kind::tiled); // NEW R20 (wide_menu) host tag only
        draw_frontend_tiled_438830(f, 16, source_word(f, 0x5ff448), full, scroll);
    }
    else if (layout >= 2 && layout <= 7)
    {
        SourceMenuRect target;
        switch (layout)
        {
        case 2:
            target = {0x42400000u, 0x42ac0000u, 0x44140000u, 0x43880000u};
            break;
        case 3:
            target = {0x43340000u, 0x42ac0000u, 0x44130000u, 0x439b0000u};
            break;
        case 4:
            target = {0x42200000u, 0x42c00000u, 0x44160000u, 0x43800000u};
            break;
        case 6:
            target = {0x42f00000u, 0x42b40000u, 0x44020000u, 0x43be0000u};
            break;
        default:
            target = {0x42440000u, 0x42a40000u, 0x43c38000u, 0x43a70000u};
            break;
        }
        auto start = target;
        shrink_frontend_rect_43ff40(start);
        clip = interpolate_43d960(start, target, source_word(f, 0x5ff468));
        draw_frontend_tiled_438830(
            f, 19, 95, clip, {store(double(as_float(scroll[0])) * 0.5), store(double(as_float(scroll[1])) * 0.5)});
        {
            host_wide_menu::QueueScope wide(host_wide_menu::Kind::tiled); // NEW R20 (wide_menu) host tag only
            draw_frontend_outside_43feb0(f, source_word(f, 0x5ff448), clip, scroll);
        }
        std::array<std::uint32_t, 6> bars;
        for (unsigned i = 0; i < 6; ++i)
            bars[i] = source_word(f, 0x5ff470 + i * 4);
        draw.panel_4398a0(f, 14, 110, clip, 0x40, bars);
        const auto side = [&](SourceMenuRect rect, std::uint32_t descriptor) {
            draw.frame_439450(f, 10, descriptor, rect, 0x40);
            draw.fill_439380(f, 13, 2, rect, 0, 0);
        };
        if (layout == 3)
            side({0x43340000u, 0x43ab0000u, 0x44130000u, 0x43d30000u}, 110);
        else if (layout == 4)
            side({0x42200000u, 0x43900000u, 0x43cc0000u, 0x43c00000u}, 111);
        else if (layout == 5)
            side({0x43d38000u, 0x42a40000u, 0x4413c000u, 0x43a70000u}, 110);
        else if (layout == 7)
        {
            side({0x43d38000u, 0x42a40000u, 0x4413c000u, 0x43700000u}, 110);
            side({0x43d38000u, 0x43880000u, 0x4413c000u, 0x43a70000u}, 111);
        }
    }
    {
        host_wide_menu::QueueScope wide(host_wide_menu::Kind::screen_frame); // NEW R20 (wide_menu) host tag only
        draw.frame_439450(f, 1, 113, full, 0);
    }
    set_frontend_clip_438190(f.source, clip);
    auto page = source_word(f, 0x5ff3f0);
    if (page)
    {
        if (page_kind(f, page) != 1)
        {
            const auto text = frontend_language_text_441170(f.source.language_strings(), 1);
            if (text.value == 0 || text.value > f.source.language_arena.size())
                throw FormatError("438BE0 null or foreign language text");
            const auto bytes = std::span{f.source.language_arena}.subspan(text.value - 1);
            queue_frontend_text_438990(f.source, 2, 1, 0x43a00000u, 0x41800000u, 4, bytes, 0x3fa00000u, 0xffffffffu,
                                       0x803, 0);
            page = source_word(f, 0x5ff3f0); // 43FE70 after the completed438BE0 child
        }
        if (page)
            pages.page_event(f, page, 3, 0);
    }
}
void SourceMenuPageDrawChildren::draw_pages_43f6b0(FrontendState &f)
{
    draw_frontend_pages_43f6b0(f, *this, *this);
}
} // namespace sgn::retail
