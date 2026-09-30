#include "sgn/retail_text_submission.hpp"
#include "sgn/retail_screen_polygon_submission.hpp"
#include "sgn/host_wide_overlay.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <limits>

namespace sgn::retail
{
namespace
{
constexpr std::uint64_t sign = 0x8000000000000000ull, exponent = 0x7ff0000000000000ull, quiet = 0x0008000000000000ull;
constexpr std::uint32_t font_base = 0x577bd0, font_bytes = 0x1ba0, font_end = font_base + 2 * font_bytes,
                        map_base = font_end + 8;
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
using sgn::format_check::require;
double indefinite()
{
    return std::bit_cast<double>(sign | exponent | quiet);
}
bool nan(double v)
{
    return (std::bit_cast<std::uint64_t>(v) & ~sign) > exponent;
}
// Memory arithmetic operands retain signaling classification until the operation.
double memory(float v)
{
    const auto w = as_word(v);
    if ((w & 0x7fffffffu) > 0x7f800000u)
        return std::bit_cast<double>((std::uint64_t(w & 0x80000000u) << 32) | exponent |
                                     (std::uint64_t(w & 0x7fffffu) << 29));
    return double(v);
}
double load(float v)
{
    const auto d = memory(v);
    return nan(d) ? std::bit_cast<double>(std::bit_cast<std::uint64_t>(d) | quiet) : d;
}
enum class Op
{
    add,
    subtract,
    multiply,
    divide
};
double arithmetic(double a, double b, Op op)
{
    const auto ua = std::bit_cast<std::uint64_t>(a), ub = std::bit_cast<std::uint64_t>(b), ma = ua & ~sign,
               mb = ub & ~sign;
    if (ma > exponent || mb > exponent)
    {
        auto chosen = ma > exponent ? ua : ub;
        if (ma > exponent && mb > exponent)
        {
            const bool aq = (ua & quiet) != 0, bq = (ub & quiet) != 0;
            if (aq != bq)
                chosen = aq ? ua : ub;
            else
                chosen = ma == mb ? (ua < ub ? ua : ub) : ma > mb ? ua : ub;
        }
        return std::bit_cast<double>(chosen | quiet);
    }
    switch (op)
    {
    case Op::add:
        if (ma == exponent && mb == exponent && ((ua ^ ub) & sign))
            return indefinite();
        return a + b;
    case Op::subtract:
        if (ma == exponent && mb == exponent && !((ua ^ ub) & sign))
            return indefinite();
        return a - b;
    case Op::multiply:
        if ((ma == 0 && mb == exponent) || (ma == exponent && mb == 0))
            return indefinite();
        return a * b;
    case Op::divide:
        if ((ma == 0 && mb == 0) || (ma == exponent && mb == exponent))
            return indefinite();
        if (mb == 0)
            return std::bit_cast<double>(((ua ^ ub) & sign) | exponent);
        return a / b;
    }
    return indefinite();
}
double add(double a, double b)
{
    return arithmetic(a, b, Op::add);
}
double sub(double a, double b)
{
    return arithmetic(a, b, Op::subtract);
}
double mul(double a, double b)
{
    return arithmetic(a, b, Op::multiply);
}
double div(double a, double b)
{
    return arithmetic(a, b, Op::divide);
}
float rounded(double v)
{
    return static_cast<float>(v);
}
using sgn::bit_reinterpret::as_signed;
bool below(float a, float b)
{
    return nan(memory(a)) || nan(memory(b)) || a < b;
}
bool above(float a, float b)
{
    return !nan(memory(a)) && !nan(memory(b)) && a > b;
}
unsigned selected_font(const RetailFontResources &f)
{
    require(f.current >= 1 && f.current <= 2, "text current font outside two physical owners");
    return f.current - 1;
}
std::uint32_t guest_current(const RetailFontResources &f)
{
    require(f.current <= 2, "text current font pointer projection outside pinned bank");
    return f.current ? font_base + (f.current - 1) * font_bytes : 0;
}
std::uint8_t source_byte(const RetailFontResources &f, std::uint32_t address)
{
    if (address >= font_base && address < font_end)
    {
        const auto at = address - font_base;
        return static_cast<std::uint8_t>(f.source_word(at / font_bytes, (at % font_bytes) & ~3u) >> ((at & 3u) * 8));
    }
    if (address >= font_end && address < font_end + 4)
        return static_cast<std::uint8_t>(guest_current(f) >> ((address - font_end) * 8));
    if (address >= font_end + 4 && address < map_base)
        return static_cast<std::uint8_t>(f.count >> ((address - font_end - 4) * 8));
    if (address >= map_base && address < map_base + 256)
        return f.glyph_map[address - map_base];
    if (address >= map_base + 256 && address < map_base + 260)
        return static_cast<std::uint8_t>(f.digit_advance_bits >> ((address - map_base - 256) * 8));
    throw FormatError("text source byte outside existing font/map owners");
}
std::uint32_t font_word(const RetailFontResources &f, unsigned font, std::uint32_t at)
{
    require(font < 2, "text physical font outside two owners");
    const auto address = font_base + font * font_bytes + at;
    if (at < font_bytes)
        return f.source_word(font, at);
    // A source glyph byte can address the next canonical record. Preserve that
    // alias while refusing reads beyond the owners available in this module.
    require(at <= 44u * 255u + 0x60u, "text glyph offset outside source byte index domain");
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
        v |= std::uint32_t(source_byte(f, address + i)) << (i * 8);
    return v;
}
std::uint8_t text_byte(std::span<const std::uint8_t> text, std::size_t at)
{
    require(at < text.size(), "text read beyond supplied source bytes");
    return text[at];
}
double normalized_scale(const RetailFontResources &f, unsigned font, float scale)
{
    const auto width = font_word(f, font, 8);
    const auto denominator = as_signed((width * 5u) << 7);
    return div(mul(load(scale), 10240.0), double(denominator));
}
bool digit_spacing(const RetailFontResources &f, std::uint8_t c)
{
    const auto d = as_float(f.digit_advance_bits);
    return !nan(memory(d)) && d != 0 && c >= '0' && c <= '9';
}
void disable_specular_42f380(TextSubmission f, PolygonSubmissionSink &sink)
{
    if (f.legacy_driver_restriction == 1)
        return;
    f.specular_enabled = 0;
    sink.set_render_state(29, 0);
    sink.set_render_state(24, 160);
}
void enable_specular_42f340(TextSubmission f, PolygonSubmissionSink &sink)
{
    if (f.legacy_driver_restriction == 1)
        return;
    f.specular_enabled = 1;
    sink.set_render_state(29, 1);
    sink.set_render_state(24, 1);
}
} // namespace

double measure_text_4290d0(RetailFontResources &f, std::span<const std::uint8_t> text, float scale)
{
    require(text.size() <= std::numeric_limits<std::uint32_t>::max(), "text extent exceeds source32-bit counter");
    double total = 0;
    std::uint32_t count = 0;
    for (;;)
    {
        const auto c = text_byte(text, count);
        if (c == 0 || c == 10)
            break;
        const auto glyph = f.glyph_map[c];
        if (digit_spacing(f, c))
            total = add(total, memory(as_float(f.digit_advance_bits)));
        else
            total = add(total, double(as_signed(font_word(f, selected_font(f), 0x28 + 44u * glyph))));
        ++count;
    }
    return add(mul(total, normalized_scale(f, selected_font(f), scale)), double(as_signed(count)));
}
void begin_text_432300(TextSubmission f, unsigned font, std::uint32_t flags, PolygonSubmissionSink &sink)
{
    require(font < 2, "text begin font outside two owners");
    sink.set_render_state(24, 1);
    const bool alternate = (flags & 0x14u) != 0;
    apply_screen_polygon_blend_432190(alternate ? 0x70u : (flags & 8u) ? 0x60u : 0x28u, sink);
    const auto material = font_word(f.fonts, font, alternate ? 0x24u : 0x20u);
    if (f.selected_material != material)
    {
        require(material < f.materials.size(), "text atlas material outside shared namespace");
        sink.set_texture(0, f.materials[material].primary_texture);
        f.selected_material = material;
    }
    disable_specular_42f380(f, sink);
}
double submit_text_glyph_4323f0(TextSubmission f, unsigned font, std::uint8_t c, TextGlyphParameters p,
                                PolygonSubmissionSink &sink)
{
    const auto at = 0x20 + 44u * f.fonts.glyph_map[c];
    const double width = mul(double(font_word(f.fonts, font, at + 8)), memory(p.scale));
    const float height = rounded(mul(double(font_word(f.fonts, font, at + 12)), memory(p.scale)));
    const auto x1 = as_word(rounded(add(width, memory(p.x)))), y1 = as_word(rounded(add(load(height), memory(p.y))));
    const auto u0 = font_word(f.fonts, font, at + 28), v0 = font_word(f.fonts, font, at + 32),
               u1 = font_word(f.fonts, font, at + 36), v1 = font_word(f.fonts, font, at + 40);
    // The retail owner is local stack scratch, not source58D818. UV1 words8/9
    // are source-unwritten; zero is a host initialization policy only.
    std::array<PolygonSubmissionVertex, 4> vertices{};
    // NEW HOST (wide_hud R20): anchored mapping; identity (same floats) outside a scope or on 4:3.
    const auto map = host_wide_overlay::overlay_mapping(f.viewport);
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &v = vertices[i].words;
        v[0] = (i == 1 || i == 2) ? x1 : as_word(p.x);
        v[1] = i >= 2 ? y1 : as_word(p.y);
        v[2] = v[3] = as_word(p.depth);
        v[4] = p.color | 0xff000000u;
        v[5] = 0;
        v[6] = (i == 1 || i == 2) ? u1 : u0;
        v[7] = i >= 2 ? v1 : v0;
        v[0] = as_word(rounded(add(mul(sub(load(as_float(v[0])), 320.0), memory(map.scale_x)), memory(map.center_x))));
        v[1] = as_word(rounded(add(mul(sub(load(as_float(v[1])), 240.0), memory(map.scale_y)), memory(map.center_y))));
    }
    if (p.flags & 2)
    {
        const auto amount =
            mul(mul(load(as_float(font_word(f.fonts, font, 0x18))), memory(as_float(font_word(f.fonts, font, 0x14)))),
                memory(height));
        for (unsigned i = 0; i < 4; ++i)
        {
            auto &x = vertices[i].words[0];
            x = as_word(rounded(i < 2 ? add(load(as_float(x)), amount) : sub(load(as_float(x)), amount)));
        }
    }
    sink.draw(6, 0x2c4, vertices, 0x14, f.viewport);
    if (digit_spacing(f.fonts, c))
        return add(mul(load(as_float(f.fonts.digit_advance_bits)), memory(p.scale)), 1.0);
    return add(mul(double(font_word(f.fonts, font, at + 8)), memory(p.scale)), 1.0);
}
void end_text_432380(TextSubmission f, PolygonSubmissionSink &sink)
{
    sink.set_render_state(19, 2);
    sink.set_render_state(20, 1);
    sink.set_render_state(27, 0);
    sink.set_texture_stage_state(0, 2, 2);
    sink.set_render_state(24, 160);
    if (f.quality == 1)
        enable_specular_42f340(f, sink);
}
void submit_text_4291f0(TextSubmission f, std::span<const std::uint8_t> text, std::uint32_t alignment,
                        TextGlyphParameters p, PolygonSubmissionSink &sink)
{
    // NEW HOST (wide_hud R20): logical clip widened to the view edges inside an
    // anchor scope on a non-4:3 canvas; otherwise the same clip_virtual floats.
    const auto clip = host_wide_overlay::overlay_mapping(f.viewport).clip;
    if (below(p.y, clip[2]) || above(p.y, clip[3]))
        return;
    auto font = selected_font(f.fonts);
    const float original_y = p.y, right = clip[1];
    const float left = rounded(sub(memory(clip[0]), mul(double(font_word(f.fonts, font, 8)), memory(p.scale))));
    float aligned_x = p.x;
    if (alignment == 1 || alignment == 2)
    {
        auto measured = measure_text_4290d0(f.fonts, text, p.scale);
        if (alignment == 1)
            measured = mul(measured, 0.5);
        aligned_x = rounded(sub(memory(p.x), measured));
        font = selected_font(f.fonts);
    }
    const float scale = rounded(normalized_scale(f.fonts, font, p.scale));
    const float space =
        rounded(add(mul(double(font_word(f.fonts, font, 0x28 + 44u * f.fonts.glyph_map[32])), memory(scale)), 1.0));
    f.fonts.source_word(font, 0x18) = as_word(rounded(mul(double(f.viewport.height), memory(as_float(0x385a740eu)))));
    const auto pass = [&](float x, float y, float depth, std::uint32_t color) {
        std::size_t cursor = 0;
        for (;;)
        {
            const auto c = text_byte(text, cursor);
            if (c == 0 || c == 10 || above(x, right))
                break;
            const auto current = selected_font(f.fonts);
            if (below(x, left))
            {
                // MOVSX is literal only on this left-clipped path. Negative
                // map offsets alias font1 tail/current pointer/count owners.
                const auto signed_c = std::bit_cast<std::int8_t>(c);
                const auto glyph = source_byte(f.fonts, static_cast<std::uint32_t>(std::int64_t(map_base) + signed_c));
                x = rounded(add(
                    add(mul(double(font_word(f.fonts, current, 0x28 + 44u * glyph)), memory(scale)), memory(x)), 1.0));
            }
            else if (c == 32)
                x = rounded(add(load(space), memory(x)));
            else
                x = rounded(add(submit_text_glyph_4323f0(f, current, c, {x, y, depth, color, scale, p.flags}, sink),
                                memory(x)));
            ++cursor;
        }
    };
    if (p.flags & 4)
    {
        const float depth = rounded(sub(load(p.depth), memory(as_float(0x3a83126fu))));
        begin_text_432300(f, font, p.flags, sink);
        font = selected_font(f.fonts);
        const double offset = mul(load(scale), memory(as_float(font_word(f.fonts, font, 0x1c))));
        pass(rounded(add(load(aligned_x), offset)), rounded(add(load(original_y), offset)), depth, 0xffffffffu);
    }
    font = selected_font(f.fonts);
    begin_text_432300(f, font, p.flags & ~4u, sink);
    pass(aligned_x, original_y, p.depth, p.color);
    end_text_432380(f, sink);
}
} // namespace sgn::retail
