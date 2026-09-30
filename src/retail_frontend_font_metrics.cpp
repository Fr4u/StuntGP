#include "sgn/retail_frontend_font_metrics.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace sgn::retail
{
namespace
{
struct Tga
{
    std::span<const std::uint8_t> raw;
    unsigned width, height, start;
};
Tga tga(std::span<const std::uint8_t> raw)
{
    if (raw.size() < 18 || raw[2] != 2 || raw[16] != 24)
        throw std::invalid_argument("frontend font requires source type2 RGB24 TGA");
    const auto width = unsigned(raw[12]) + (unsigned(raw[13]) << 8),
               height = unsigned(raw[14]) + (unsigned(raw[15]) << 8), start = 18u + raw[0];
    if (width == 0 || width > 256 || height < 160 || height > 65535 ||
        std::size_t(start) + std::size_t(width) * height * 3 > raw.size())
        throw std::invalid_argument("frontend font TGA bounds");
    return {raw, width, height, start};
}
void font(std::span<std::uint32_t, 161> record, Tga input, unsigned prior_space)
{
    // 429180 runs before the glyph loop. Preserve unprocessed records.
    record[1 + prior_space] = 8;
    record[0] = input.width;
    const auto cell = input.height / 160;
    unsigned pack_x = 0, pack_y = 0;
    for (unsigned glyph = 0; glyph < 160; ++glyph)
    {
        unsigned left = input.width, right = 0, width = 0;
        for (unsigned y = 0; y < cell; ++y)
        {
            for (unsigned x = 0; x < input.width; ++x)
            {
                // 4622C0 tests image type (BL=2), so its accepted path always
                // reverses rows, even if the TGA descriptor says otherwise.
                const auto row = input.height - 1 - (glyph * cell + y);
                const auto at = input.start + 3u * (row * input.width + x);
                const auto blue = input.raw[at], green = input.raw[at + 1], red = input.raw[at + 2];
                if ((red == 255 && green == 0 && blue == 255) || (red < 96 && green < 96 && blue < 96))
                    continue;
                left = std::min(left, x);
                right = std::max(right, x);
            }
            width = std::max(width, (right >= left ? right - left : 0u) + 1);
        }
        if (pack_x + width > 256)
        {
            pack_x = 0;
            pack_y += cell;
            if (pack_y >= 256)
                break;
        }
        record[1 + glyph] = width;
        pack_x += width + 1;
    }
    record[1 + 153] = record[1 + prior_space]; // 428E0D..428E2C
}
bool overlap(const void *a, std::size_t an, const void *b, std::size_t bn)
{
    if (!an || !bn)
        return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a), y = reinterpret_cast<std::uintptr_t>(b);
    return x <= y ? y - x < an : x - y < bn;
}
} // namespace
void initialize_frontend_font_metrics(FrontendFontMetricsViews views, FrontendFontInputs inputs,
                                      std::int32_t source_width)
{
    if (inputs.index.size() < 160 || views.glyph_map[32] >= 160)
        throw std::invalid_argument("frontend font source index or prior space outside record");
    const unsigned count = source_width >= 1024 ? 2 : 1;
    std::array<Tga, 2> images{tga(source_width >= 1152 ? inputs.i32 : inputs.i32_b05), {}};
    if (count == 2)
        images[1] = tga(inputs.i32_b025);
    const std::array<std::pair<const void *, std::size_t>, 6> owners{
        {{views.records.data(), views.records.size_bytes()},
         {views.index.data(), views.index.size_bytes()},
         {views.glyph_map.data(), views.glyph_map.size_bytes()},
         {&views.current, 4},
         {&views.count, 4},
         {&views.digit_advance_bits, 4}}};
    const std::array<std::span<const std::uint8_t>, 4> resources{inputs.index, inputs.i32, inputs.i32_b05,
                                                                 inputs.i32_b025};
    for (unsigned i = 0; i < owners.size(); ++i)
    {
        for (unsigned j = 0; j < i; ++j)
            if (overlap(owners[i].first, owners[i].second, owners[j].first, owners[j].second))
                throw std::invalid_argument("frontend font owners overlap");
        for (auto raw : resources)
            if (overlap(owners[i].first, owners[i].second, raw.data(), raw.size()))
                throw std::invalid_argument("frontend font resources overlap output");
    }
    views.digit_advance_bits = 0;
    views.current = 1;
    views.count = 0;
    std::copy_n(inputs.index.begin(), 160, views.index.begin());
    for (unsigned i = 0; i < count; ++i)
    {
        views.current = i + 1;
        font(std::span<std::uint32_t, 161>(views.records.data() + 161 * i, 161), images[i], views.glyph_map[32]);
        ++views.count;
    }
    std::fill(views.glyph_map.begin(), views.glyph_map.end(), std::uint8_t{});
    for (unsigned i = 0; i < 160; ++i)
        views.glyph_map[views.index[i]] = static_cast<std::uint8_t>(i);
    views.glyph_map[32] = 153;
    views.current = 1;
}
double measure_frontend_text_438910(FrontendFontMetricsViews views, std::span<const std::uint8_t> text, float scale)
{
    const auto end = std::find(text.begin(), text.end(), std::uint8_t{});
    if (end == text.end() || !std::isfinite(scale) || views.count == 0 || views.count > 2)
        throw std::invalid_argument("frontend measure input or font count");
    unsigned selected = scale > 1.0f ? 1u : 0u;
    if (selected >= views.count)
        selected = views.count - 1;
    const auto *record = views.records.data() + 161 * selected;
    const auto denominator = std::bit_cast<std::int32_t>((record[0] * 5u) << 7);
    if (denominator == 0)
        throw std::invalid_argument("frontend font measure zero divisor");
    const auto digit = std::bit_cast<float>(views.digit_advance_bits);
    if (std::isinf(digit))
        throw std::invalid_argument("frontend font infinite digit advance");
    double accumulated = 0;
    unsigned count = 0;
    for (auto at = text.begin(); at != end && *at != 10; ++at)
    {
        const auto glyph = views.glyph_map[*at];
        if (glyph >= 160)
            throw std::invalid_argument("frontend font glyph outside record");
        if (digit == 0 || std::isnan(digit) || *at < '0' || *at > '9')
            accumulated += std::bit_cast<std::int32_t>(record[1 + glyph]);
        else
            accumulated += static_cast<double>(digit);
        ++count;
    }
    const double product = static_cast<double>(scale) * 10240.0;
    const double factor = product / static_cast<double>(denominator);
    const double width = accumulated * factor;
    views.current = selected + 1;
    return width + static_cast<double>(count);
}
} // namespace sgn::retail
