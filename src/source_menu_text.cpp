#include "sgn/source_menu_text.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
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
std::uint8_t byte(std::span<const std::uint8_t> text, std::size_t at)
{
    if (at >= text.size())
        throw FormatError("frontend text reads beyond source owner");
    return text[at];
}
std::span<std::uint8_t> language_tail(FrontendState &f, FrontendLanguageTextIdentity id)
{
    if (id.value == frontend_catalog_text_tag)
        return f.source.source_bytes(0x6087a4, 4);
    if ((id.value & frontend_catalog_text_tag) != 0)
    {
        const auto offset = (id.value & ~frontend_catalog_text_tag) - 1u;
        constexpr auto bytes = frontend_language_catalog_words * 4u;
        if (offset >= bytes)
            throw FormatError("frontend text name leaves its catalog owner");
        return f.source.source_bytes(0x5e37b8u + offset, bytes - offset);
    }
    if (id.value == 0 || id.value > f.source.language_arena.size())
        throw FormatError("frontend text dereferences null or foreign language identity");
    return std::span{f.source.language_arena}.subspan(id.value - 1);
}
struct BoxShape
{
    std::uint32_t descriptor, left, right, height;
};
BoxShape shape_438dc0(std::uint32_t id)
{
    switch (id)
    {
    case 1:
        return {87, 26, 16, 32};
    case 2:
        return {90, 26, 16, 32};
    case 3:
        return {88, 32, 32, 32};
    case 4:
        return {89, 32, 32, 32};
    case 5:
        return {91, 80, 48, 64};
    default:
        throw FormatError("438DC0 default leaves source stack height unwritten");
    }
}
void box_strip_438eb0(FrontendState &f, std::uint32_t layer, std::uint32_t argument, std::uint32_t x, std::uint32_t y,
                      BoxShape shape, std::uint32_t row, std::uint32_t scale, std::uint32_t width, std::uint32_t flags,
                      std::uint32_t alpha)
{
    // 43D7B0's unordered lower comparison chooses zero, not NaN.
    const float a = as_float(alpha);
    const float clamped = !(a >= 0.0f) ? 0.0f : a > 1.0f ? 1.0f : a;
    if (shape.descriptor >= 387)
        throw FormatError("box descriptor outside existing frontend atlas");
    std::array<std::uint32_t, 12> d;
    for (unsigned i = 0; i < 12; ++i)
        d[i] = f.source.frontend_descriptors[shape.descriptor * 12 + i];
    d[0] = 2;
    d[11] = 255;
    if (flags & 0x100u)
        d[10] = 0x58;
    else if (flags & 0x200u)
        d[10] = 0x70;
    else if (flags & 0x400u)
    {
        d[10] = 0x60;
        // The clamp bounds this unspilled x87 product to[0,255]; integer
        // conversion truncates directly without an invented float32 spill.
        d[11] = static_cast<std::uint32_t>(double(clamped) * 255.0);
    }
    else
        d[10] = 0x20;
    d[1] = x;
    d[2] = y;
    d[4] = store(double(as_signed(shape.height)) * double(as_float(scale)));
    const double v_offset = double(as_signed(shape.height)) * 4.0 * double(as_signed(row));
    d[6] = store(v_offset + double(as_float(d[6])));
    d[8] = store(v_offset + double(as_float(d[8])));
    d[3] = store(double(as_signed(shape.left)) * double(as_float(scale)));
    d[7] = store(double(as_signed(shape.left)) * 4.0);
    clip_frontend_rectangle_438370(f.source, d, layer, argument);
    d[1] = store(double(as_float(d[3])) + double(as_float(d[1])));
    d[3] = width;
    d[5] = d[7];
    d[7] = store(double(as_signed(256u - shape.right)) * 4.0);
    clip_frontend_rectangle_438370(f.source, d, layer, argument);
    d[1] = store(double(as_float(d[3])) + double(as_float(d[1])));
    d[5] = d[7];
    d[7] = 0x44800000u;
    d[3] = store(double(as_signed(shape.right)) * double(as_float(scale)));
    clip_frontend_rectangle_438370(f.source, d, layer, argument);
}
} // namespace
void queue_frontend_text_438990(FrontendSourceStorage &s, std::uint32_t layer, std::uint32_t argument, std::uint32_t x,
                                std::uint32_t y, std::uint32_t style, std::span<const std::uint8_t> text,
                                std::uint32_t scale, std::uint32_t color, std::uint32_t flags,
                                std::uint32_t digit_advance)
{
    std::uint32_t length = 0;
    while (byte(text, length) != 0)
        ++length; // literal source strlen before allocation
    const auto bytes = length + 1u;
    const auto id = allocate_frontend_command_438550(s, bytes + 56u);
    if (id == 0)
        return;
    s.source_word(id + 8) = x;
    s.source_word(id + 16) = style;
    s.source_word(id + 12) = y;
    s.source_word(id + 4) = 0;
    auto destination = s.source_bytes(id + 56, bytes);
    // Forward source copy, including NUL. Allocation padding and+24..+37
    // unwritten payload bytes remain as they were before this command.
    for (std::uint32_t i = 0; i < bytes; ++i)
        destination[i] = byte(text, i);
    s.source_word(id + 24) = color;
    s.source_word(id + 20) = scale;
    s.source_word(id + 32) = digit_advance;
    s.source_word(id + 28) = flags;
    link_frontend_command_438590(s, layer, argument, id);
}
double draw_frontend_aligned_borrowed_438ab0(FrontendState &f, const SourceFrontendTextCall<3> &call,
                                             std::span<std::uint8_t> text)
{
    f.shared_platform()->require_owner_thread();
    // 4793D8 PEfloat0.85; decompiled references are reads only.
    const auto step = store(double(as_float(call.tail[0])) * 16.0 * double(as_float(0x3f59999au)));
    std::uint32_t lines = 1;
    for (std::size_t i = 0; byte(text, i) != 0; ++i)
        if (text[i] == '|' || text[i] == 255u)
            ++lines;
    auto y = call.y;
    if (call.tail[2] & 2u)
        y = store(double(as_float(y)) - double(as_signed(lines - 1u)) * double(as_float(step)) * 0.5);
    std::size_t start = 0, cursor = 0;
    if (byte(text, 0) != 0)
    {
        for (;;)
        {
            const auto saved = byte(text, cursor);
            if (saved == '|' || saved == 255u || saved == 0)
            {
                text[cursor] = 0; // actual source temporary mutation before438990
                queue_frontend_text_438990(f.source, call.ecx, 1, call.x, y, call.edx, text.subspan(start),
                                           call.tail[0], call.tail[1], call.tail[2], 0);
                text[cursor] = saved; // no RAII rollback on a host abort
                if (saved == 0)
                    break;
                y = store(double(as_float(step)) + double(as_float(y)));
                start = cursor + 1;
            }
            ++cursor;
            if (byte(text, start) == 0)
                break; // source checks line-start, not scanning cursor
        }
    }
    return double(as_signed(lines)) * double(as_float(step));
}
void draw_frontend_box_borrowed_439040(FrontendState &f, const SourceFrontendTextCall<8> &call,
                                       std::span<std::uint8_t> text)
{
    f.shared_platform()->require_owner_thread();
    // Tail: fontscale,color,shape,row,boxscale,flags,signedwidth,alpha.
    const auto &t = call.tail;
    const auto text_height = store(double(as_float(t[0])) * 16.0);
    const auto shape = shape_438dc0(t[2]);
    const auto scaled_height = store(double(as_signed(shape.height * 28u) / 32) * double(as_float(t[4])));
    double measured = -1.0;
    auto width = t[6];
    const auto measure = [&] {
        return measure_frontend_text_438910(f.world().ui.fonts.metrics(), text, as_float(t[0]));
    };
    if (width == 0)
    {
        measured = measure();
        width = store(measured);
    }
    else if (as_signed(width) < 0)
    {
        const auto minimum = 0u - width;
        measured = measure();
        width = store(measured > double(as_signed(minimum)) ? measured : double(as_signed(minimum)));
    }
    else
        width = store(double(as_signed(width)));
    const auto x =
        store(double(as_float(call.x)) -
              (double(as_signed(shape.left + shape.right)) * double(as_float(t[4])) + double(as_float(width))) * 0.5);
    const auto y = store(double(as_float(call.y)) - double(as_float(scaled_height)) * 0.5);
    auto text_x = store(double(as_signed(shape.left)) * double(as_float(t[4])) + double(as_float(x)));
    const auto text_y = store((double(as_float(scaled_height)) - double(as_float(text_height))) * 0.5 +
                              double(as_float(y)) + double(as_float(t[4])));
    if (t[5] & 1u)
    {
        if (!(measured >= 0.0))
            measured = measure();
        text_x = store((double(as_float(width)) - measured) * 0.5 + double(as_float(text_x)));
    }
    else if (t[5] & 4u)
    {
        if (!(measured >= 0.0))
            measured = measure();
        text_x = store(double(as_float(width)) - measured + double(as_float(text_x)));
    }
    if (t[5] & 0x40u)
        box_strip_438eb0(f, 15, 0, store(double(as_float(x)) + 8.0), store(double(as_float(y)) + 8.0), shape, 2, t[4],
                         width, 0x200, 0);
    box_strip_438eb0(f, call.ecx, 3, x, y, shape, 1, t[4], width, 0x200, 0);
    box_strip_438eb0(f, call.ecx, 2, x, y, shape, t[3], t[4], width, 0x100, 0);
    // 438960 pushes digit spacing0 and forwards to438990; text flags are0.
    queue_frontend_text_438990(f.source, call.ecx, 1, text_x, text_y, call.edx, text, t[0], t[1], 0, 0);
    if (as_float(t[7]) > 0.0f)
        box_strip_438eb0(f, call.ecx, 0, x, y, shape, 3, t[4], width, 0x400, t[7]);
}
double draw_frontend_aligned_text_438ab0(FrontendState &f, const SourceFrontendTextCall<3> &call)
{
    return draw_frontend_aligned_borrowed_438ab0(f, call, language_tail(f, call.text));
}
void draw_frontend_text_box_439040(FrontendState &f, const SourceFrontendTextCall<8> &call)
{
    draw_frontend_box_borrowed_439040(f, call, language_tail(f, call.text));
}
void SourceMenuTextChildren::text_box_439040(FrontendState &f, const SourceFrontendTextCall<8> &call)
{
    draw_frontend_text_box_439040(f, call);
}
void SourceMenuTextChildren::aligned_text_438ab0(FrontendState &f, const SourceFrontendTextCall<3> &call)
{
    (void)draw_frontend_aligned_text_438ab0(f, call);
}
} // namespace sgn::retail
