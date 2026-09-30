#include "sgn/host_texture_upscale.hpp"
#include "sgn/host_parallel.hpp"
#include "sgn/host_texture_disk_cache.hpp"
#include "sgn/host_texture_processing.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <list>
#include <mutex>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

// NEW HOST (R28) sharp HUD. See host_texture_upscale.hpp. Everything here is a pure function of the
// source words (plus the class/factor decision), so the same upload always yields the same texture.
namespace sgn::host_texture_upscale
{
namespace
{
thread_local std::optional<TextureClass> pending_hint;

// ------------------------------------------------------------------------------------------------
// Classification
// ------------------------------------------------------------------------------------------------
std::string normalized(std::string_view name)
{
    std::string text;
    text.reserve(name.size());
    for (char c : name)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        text.push_back(c == '/' ? '\\' : c);
    }
    // Loader names are relative ("Overlays\Dial.PC", "\FrontEnd\Tiles\tile01.PC"); a full source path
    // may carry the graphics root. Keep what follows the last "graphics24[max]\" component.
    for (const std::string_view root : {std::string_view("graphics24\\"), std::string_view("graphics24max\\")})
        if (const auto at = text.rfind(root);
            at != std::string::npos && (at == 0 || text[at - 1] == '\\' || text[at - 1] == ':'))
            text.erase(0, at + root.size());
    const auto first = text.find_first_not_of('\\');
    text.erase(0, first == std::string::npos ? text.size() : first);
    return text;
}
bool component(std::string_view text, std::string_view name)
{
    return text.size() > name.size() && text.starts_with(name) && text[name.size()] == '\\';
}

// ------------------------------------------------------------------------------------------------
// Texel helpers. A1R5G5B5: bit 15 alpha, R 14..10, G 9..5, B 4..0.
// ------------------------------------------------------------------------------------------------
struct Rgba
{
    float r, g, b, a; // straight colour in [0,1] (5-bit code / 31), a is 0 or 1 on input
};
Rgba decode(std::uint16_t w)
{
    return {static_cast<float>((w >> 10) & 31u) / 31.0f, static_cast<float>((w >> 5) & 31u) / 31.0f,
            static_cast<float>(w & 31u) / 31.0f, (w & 0x8000u) ? 1.0f : 0.0f};
}
std::uint32_t expand5(std::uint32_t code)
{
    return (code * 255u + 15u) / 31u; // decode_tm (004234E0 host upload) 5-bit to 8-bit expansion
}
// 8-bit xBRZ channel value -> [0,1]. A value that is the expansion of a 5-bit code maps to exactly
// code / 31 (so an unblended texel keeps its code under the dither); a blended value maps to v / 255.
constexpr std::array<float, 256> from_8bit = [] {
    std::array<float, 256> table{};
    for (std::uint32_t v = 0; v < 256; ++v)
    {
        const std::uint32_t code = (v * 31u + 127u) / 255u;
        table[v] = (code * 255u + 15u) / 31u == v ? static_cast<float>(code) / 31.0f : static_cast<float>(v) / 255.0f;
    }
    return table;
}();
// Final result per output texel: straight colour and the thresholded alpha bit.
struct OutTexel
{
    float r, g, b;
    bool opaque;
};
constexpr std::array<std::uint8_t, 16> bayer4{0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
std::uint16_t quantize(const OutTexel &t, std::uint32_t x, std::uint32_t y)
{
    // Ordered dither of at most +-0.47 LSB: an exact code (v = code / 31, float error far below the
    // remaining 0.03 LSB margin) never moves.
    const float d = (static_cast<float>(bayer4[(y & 3u) * 4u + (x & 3u)]) + 0.5f) / 16.0f - 0.5f;
    const auto code = [d](float v) {
        // The argument is at least 0.03, so truncation is floor().
        return std::min(static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 31.0f + 0.5f + d), 31u);
    };
    return static_cast<std::uint16_t>((t.opaque ? 0x8000u : 0u) | (code(t.r) << 10) | (code(t.g) << 5) | code(t.b));
}
std::uint8_t byte(float v)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}
std::size_t wrap_or_clamp(std::int64_t i, std::uint32_t n, bool wrap)
{
    const auto count = static_cast<std::int64_t>(n);
    if (wrap)
        return static_cast<std::size_t>(((i % count) + count) % count);
    return static_cast<std::size_t>(std::clamp<std::int64_t>(i, 0, count - 1));
}
// Source coordinate of output texel o (centre-aligned): s = (o + 0.5) / k - 0.5.
double source_position(std::uint32_t o, std::uint32_t k)
{
    return (static_cast<double>(o) + 0.5) / static_cast<double>(k) - 0.5;
}
std::atomic<unsigned> worker_limit{8};
// The factor limit of the renderer's latest upload (for prewarm(); R28b). 1 = no upscale.
std::atomic<std::uint32_t> seen_factor_limit{1};
// Runs body(first, last) over the rows [0, count) in contiguous bands of at least 32 rows on up to
// worker_limit threads. Bodies only compute and write their own rows of pre-sized buffers, so the result
// is identical for any thread count. R30: the bands run on host_parallel's persistent pool instead of
// threads created for every pass (a 2D upscale made about six passes, each starting up to 7 threads).
template <class Body> void parallel_rows(std::uint32_t count, const Body &body)
{
    host_parallel::for_bands(count, 32u, worker_limit.load(), body);
}

// ------------------------------------------------------------------------------------------------
// Smooth scaler: separable Lanczos-3 on premultiplied RGBA, anti-ringing clamp to the 2x2 bilinear
// footprint, alpha threshold 0.5 on the filtered coverage.
// ------------------------------------------------------------------------------------------------
double lanczos3(double x)
{
    if (x == 0.0)
        return 1.0;
    if (std::abs(x) >= 3.0)
        return 0.0;
    const double px = std::numbers::pi * x;
    return 3.0 * std::sin(px) * std::sin(px / 3.0) / (px * px);
}
// Resampling plan of one axis for factor k: for every output coordinate o, the six Lanczos source
// indices (clamped or wrapped) with their weights, and the 2x2 footprint cell: source x0 = floor(s),
// stored as x0 + 1 in [0, n]. The nearest source texel is always o / k (|s - o / k| < 0.5).
struct Axis
{
    std::vector<std::array<std::uint32_t, 6>> index;
    std::vector<std::array<float, 6>> weight;
    std::vector<std::uint32_t> cell;
};
Axis make_axis(std::uint32_t n, std::uint32_t k, bool wrap)
{
    std::vector<std::array<float, 6>> phase_weight(k);
    std::vector<std::int64_t> phase_base(k);
    for (std::uint32_t p = 0; p < k; ++p)
    {
        const double d = source_position(p, k); // in (-0.5, 0.5)
        const double base = std::floor(d);
        std::array<double, 6> w{};
        double sum = 0.0;
        for (std::size_t t = 0; t < 6; ++t)
        {
            w[t] = lanczos3(d - (base + static_cast<double>(t) - 2.0));
            sum += w[t];
        }
        for (std::size_t t = 0; t < 6; ++t)
            phase_weight[p][t] = static_cast<float>(w[t] / sum);
        phase_base[p] = static_cast<std::int64_t>(base);
    }
    Axis axis;
    const std::size_t count = std::size_t(n) * k;
    axis.index.resize(count);
    axis.weight.resize(count);
    axis.cell.resize(count);
    for (std::size_t o = 0; o < count; ++o)
    {
        const auto q = static_cast<std::int64_t>(o / k);
        const auto p = o % k;
        for (std::size_t t = 0; t < 6; ++t)
            axis.index[o][t] = static_cast<std::uint32_t>(
                wrap_or_clamp(q + phase_base[p] + static_cast<std::int64_t>(t) - 2, n, wrap));
        axis.weight[o] = phase_weight[p];
        axis.cell[o] = static_cast<std::uint32_t>(q + phase_base[p] + 1);
    }
    return axis;
}
// Value ranges of one 2x2 footprint: premultiplied RGBA, and the straight colour of its opaque texels.
struct Footprint
{
    Rgba low{1, 1, 1, 1}, high{0, 0, 0, 0};
    std::array<float, 3> opaque_low{1, 1, 1}, opaque_high{0, 0, 0};
    bool any_opaque{};
};
std::vector<OutTexel> smooth_scale(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                                   std::uint32_t k, bool wrap)
{
    const std::uint32_t ow = width * k, oh = height * k;
    std::vector<Rgba> source(words.size()), premultiplied(words.size());
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        source[i] = decode(words[i]);
        const auto &s = source[i];
        premultiplied[i] = {s.r * s.a, s.g * s.a, s.b * s.a, s.a};
    }
    const auto ax = make_axis(width, k, wrap), ay = make_axis(height, k, wrap);
    // Footprint cells (cx, cy) in [0, width] x [0, height] cover source texels cx-1..cx, cy-1..cy.
    const std::size_t cells_x = std::size_t(width) + 1;
    std::vector<Footprint> cells(cells_x * (std::size_t(height) + 1));
    std::vector<std::array<std::size_t, 2>> columns(cells_x);
    for (std::size_t cx = 0; cx < cells_x; ++cx)
        columns[cx] = {wrap_or_clamp(static_cast<std::int64_t>(cx) - 1, width, wrap),
                       wrap_or_clamp(static_cast<std::int64_t>(cx), width, wrap)};
    parallel_rows(height + 1, [&](std::uint32_t first, std::uint32_t last) {
        for (std::size_t cy = first; cy < last; ++cy)
        {
            const std::array<std::size_t, 2> rows_of_cell{
                wrap_or_clamp(static_cast<std::int64_t>(cy) - 1, height, wrap) * width,
                wrap_or_clamp(static_cast<std::int64_t>(cy), height, wrap) * width};
            for (std::size_t cx = 0; cx < cells_x; ++cx)
            {
                auto &c = cells[cy * cells_x + cx];
                for (const auto row_start : rows_of_cell)
                    for (const auto column : columns[cx])
                    {
                        const auto i = row_start + column;
                        const auto &p = premultiplied[i];
                        c.low = {std::min(c.low.r, p.r), std::min(c.low.g, p.g), std::min(c.low.b, p.b),
                                 std::min(c.low.a, p.a)};
                        c.high = {std::max(c.high.r, p.r), std::max(c.high.g, p.g), std::max(c.high.b, p.b),
                                  std::max(c.high.a, p.a)};
                        const auto &s = source[i];
                        if (s.a > 0.0f)
                        {
                            c.any_opaque = true;
                            c.opaque_low = {std::min(c.opaque_low[0], s.r), std::min(c.opaque_low[1], s.g),
                                            std::min(c.opaque_low[2], s.b)};
                            c.opaque_high = {std::max(c.opaque_high[0], s.r), std::max(c.opaque_high[1], s.g),
                                             std::max(c.opaque_high[2], s.b)};
                        }
                    }
            }
        }
    });
    // Horizontal pass: width -> ow on every source row.
    std::vector<Rgba> rows(std::size_t(ow) * height);
    parallel_rows(height, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
        {
            const Rgba *in = premultiplied.data() + std::size_t(y) * width;
            Rgba *row = rows.data() + std::size_t(y) * ow;
            for (std::uint32_t x = 0; x < ow; ++x)
            {
                const auto &index = ax.index[x];
                const auto &weight = ax.weight[x];
                Rgba sum{0, 0, 0, 0};
                for (std::size_t t = 0; t < 6; ++t)
                {
                    const auto &s = in[index[t]];
                    sum.r += weight[t] * s.r;
                    sum.g += weight[t] * s.g;
                    sum.b += weight[t] * s.b;
                    sum.a += weight[t] * s.a;
                }
                row[x] = sum;
            }
        }
    });
    // Vertical pass, then the anti-ringing clamp and the alpha threshold.
    std::vector<OutTexel> out(std::size_t(ow) * oh);
    parallel_rows(oh, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
        {
            const auto &index = ay.index[y];
            const auto &weight = ay.weight[y];
            std::array<const Rgba *, 6> in{};
            for (std::size_t t = 0; t < 6; ++t)
                in[t] = rows.data() + std::size_t(index[t]) * ow;
            const Footprint *cell_row = cells.data() + std::size_t(ay.cell[y]) * cells_x;
            const Rgba *nearest_row = source.data() + std::size_t(y / k) * width;
            OutTexel *result = out.data() + std::size_t(y) * ow;
            for (std::uint32_t x = 0; x < ow; ++x)
            {
                Rgba sum{0, 0, 0, 0};
                for (std::size_t t = 0; t < 6; ++t)
                {
                    const auto &s = in[t][x];
                    sum.r += weight[t] * s.r;
                    sum.g += weight[t] * s.g;
                    sum.b += weight[t] * s.b;
                    sum.a += weight[t] * s.a;
                }
                // Anti-ringing: premultiplied values stay inside the 2x2 footprint range; the straight
                // colour stays inside the range of the footprint's opaque texels.
                const auto &c = cell_row[ax.cell[x]];
                const float a = std::clamp(sum.a, c.low.a, c.high.a);
                OutTexel texel{};
                texel.opaque = a >= 0.5f;
                if (c.any_opaque && a > 0.0f)
                {
                    const float inverse = 1.0f / a;
                    texel.r =
                        std::clamp(std::clamp(sum.r, c.low.r, c.high.r) * inverse, c.opaque_low[0], c.opaque_high[0]);
                    texel.g =
                        std::clamp(std::clamp(sum.g, c.low.g, c.high.g) * inverse, c.opaque_low[1], c.opaque_high[1]);
                    texel.b =
                        std::clamp(std::clamp(sum.b, c.low.b, c.high.b) * inverse, c.opaque_low[2], c.opaque_high[2]);
                }
                else
                {
                    // Fully transparent footprint: keep the nearest source texel's own (key) colour.
                    const auto &s = nearest_row[x / k];
                    texel.r = s.r;
                    texel.g = s.g;
                    texel.b = s.b;
                }
                result[x] = texel;
            }
        }
    });
    return out;
}

// ------------------------------------------------------------------------------------------------
// Pixel-art scaler: xBRZ 1.8 rules (Zenju's algorithm, reimplemented; ARGB colour distance, default
// configuration: luminance weight 1, equal-colour tolerance 30, centre-direction bias 4, dominant
// direction threshold 3.6, steep direction threshold 2.2), factor 2 or 4.
// ------------------------------------------------------------------------------------------------
enum Blend : std::uint8_t
{
    blend_none = 0,
    blend_normal = 1,
    blend_dominant = 2
};
struct Pixel
{
    std::uint32_t argb; // canonical: every transparent texel is 0
    float y, cb, cr;    // YCbCr (BT.2020 coefficients) of the 8-bit expanded colour
    bool opaque;
};
struct XbrzConfig
{
    static constexpr double luminance_weight = 1.0, equal_color_tolerance = 30.0, center_direction_bias = 4.0,
                            dominant_direction_threshold = 3.6, steep_direction_threshold = 2.2;
};
double distance(const Pixel &p, const Pixel &q)
{
    // ColorDistanceARGB with 1-bit alpha: min(a1,a2) * distYCbCr + 255 * |a1 - a2|.
    if (p.opaque != q.opaque)
        return 255.0;
    if (!p.opaque)
        return 0.0;
    const double y = XbrzConfig::luminance_weight * (static_cast<double>(p.y) - q.y);
    const double cb = static_cast<double>(p.cb) - q.cb, cr = static_cast<double>(p.cr) - q.cr;
    return std::sqrt(y * y + cb * cb + cr * cr);
}
// distance(p, q) squared (sqrt is monotonic and correctly rounded, so max/compare agree exactly).
double distance_squared(const Pixel &p, const Pixel &q)
{
    if (p.opaque != q.opaque)
        return 255.0 * 255.0;
    if (!p.opaque)
        return 0.0;
    const double y = XbrzConfig::luminance_weight * (static_cast<double>(p.y) - q.y);
    const double cb = static_cast<double>(p.cb) - q.cb, cr = static_cast<double>(p.cr) - q.cr;
    return y * y + cb * cb + cr * cr;
}
bool equal_color(const Pixel &p, const Pixel &q)
{
    return distance(p, q) < XbrzConfig::equal_color_tolerance;
}
struct Corners
{
    std::uint8_t f, g, j, k; // blend of the corner between F G / J K for each of the four pixels
};
// Corner between F, G, J, K of the 4x4 kernel a..p around F (row-major). jg and fk are the reference
// sums dist(i,f) + dist(f,c) + dist(n,k) + dist(k,h) + bias * dist(j,g) and dist(e,j) + dist(j,o) +
// dist(b,g) + dist(g,l) + bias * dist(f,k).
Corners preprocess_corners(const Pixel &f, const Pixel &g, const Pixel &j, const Pixel &k, double jg, double fk)
{
    Corners result{};
    if ((f.argb == g.argb && j.argb == k.argb) || (f.argb == j.argb && g.argb == k.argb))
        return result;
    if (jg < fk)
    {
        const bool dominant = XbrzConfig::dominant_direction_threshold * jg < fk;
        if (f.argb != g.argb && f.argb != j.argb)
            result.f = dominant ? blend_dominant : blend_normal;
        if (k.argb != j.argb && k.argb != g.argb)
            result.k = dominant ? blend_dominant : blend_normal;
    }
    else if (fk < jg)
    {
        const bool dominant = XbrzConfig::dominant_direction_threshold * fk < jg;
        if (j.argb != f.argb && j.argb != k.argb)
            result.j = dominant ? blend_dominant : blend_normal;
        if (g.argb != f.argb && g.argb != k.argb)
            result.g = dominant ? blend_dominant : blend_normal;
    }
    return result;
}
// Blend info: 2 bits per corner, top-left (bits 0-1), top-right, bottom-right, bottom-left.
std::uint8_t top_right(std::uint8_t b)
{
    return static_cast<std::uint8_t>((b >> 2) & 3u);
}
std::uint8_t bottom_right(std::uint8_t b)
{
    return static_cast<std::uint8_t>((b >> 4) & 3u);
}
std::uint8_t bottom_left(std::uint8_t b)
{
    return static_cast<std::uint8_t>((b >> 6) & 3u);
}
std::uint8_t rotate_blend(std::uint8_t b, unsigned rotation)
{
    for (unsigned r = 0; r < rotation; ++r)
        b = static_cast<std::uint8_t>(((b << 2) | (b >> 6)) & 0xffu);
    return b;
}
std::uint32_t alpha_of(std::uint32_t p)
{
    return p >> 24;
}
// gradientARGB<M, N>: intermediate colour of two colours with alpha (no alpha blending).
void alpha_gradient(std::uint32_t &back, std::uint32_t front, unsigned m, unsigned n)
{
    const unsigned weight_front = alpha_of(front) * m, weight_back = alpha_of(back) * (n - m),
                   weight_sum = weight_front + weight_back;
    if (weight_sum == 0)
    {
        back = 0;
        return;
    }
    const auto channel = [&](unsigned shift) {
        return (((front >> shift) & 255u) * weight_front + ((back >> shift) & 255u) * weight_back) / weight_sum;
    };
    back = ((weight_sum / n) << 24) | (channel(16) << 16) | (channel(8) << 8) | channel(0);
}
// Output block of one source texel, addressed in a rotated frame (i = row, j = column).
struct OutputBlock
{
    std::uint32_t *origin;
    std::size_t stride;
    unsigned scale, rotation;
    std::uint32_t &ref(unsigned i, unsigned j) const
    {
        for (unsigned r = 0; r < rotation; ++r)
        {
            const unsigned old_i = scale - 1 - j, old_j = i;
            i = old_i;
            j = old_j;
        }
        return origin[std::size_t(i) * stride + j];
    }
};
void blend_line_shallow(std::uint32_t col, const OutputBlock &out)
{
    const unsigned s = out.scale;
    if (s == 2)
    {
        alpha_gradient(out.ref(1, 0), col, 1, 4);
        alpha_gradient(out.ref(1, 1), col, 3, 4);
        return;
    }
    alpha_gradient(out.ref(s - 1, 0), col, 1, 4);
    alpha_gradient(out.ref(s - 2, 2), col, 1, 4);
    alpha_gradient(out.ref(s - 1, 1), col, 3, 4);
    alpha_gradient(out.ref(s - 2, 3), col, 3, 4);
    out.ref(s - 1, 2) = col;
    out.ref(s - 1, 3) = col;
}
void blend_line_steep(std::uint32_t col, const OutputBlock &out)
{
    const unsigned s = out.scale;
    if (s == 2)
    {
        alpha_gradient(out.ref(0, 1), col, 1, 4);
        alpha_gradient(out.ref(1, 1), col, 3, 4);
        return;
    }
    alpha_gradient(out.ref(0, s - 1), col, 1, 4);
    alpha_gradient(out.ref(2, s - 2), col, 1, 4);
    alpha_gradient(out.ref(1, s - 1), col, 3, 4);
    alpha_gradient(out.ref(3, s - 2), col, 3, 4);
    out.ref(2, s - 1) = col;
    out.ref(3, s - 1) = col;
}
void blend_line_steep_and_shallow(std::uint32_t col, const OutputBlock &out)
{
    if (out.scale == 2)
    {
        alpha_gradient(out.ref(1, 0), col, 1, 4);
        alpha_gradient(out.ref(0, 1), col, 1, 4);
        alpha_gradient(out.ref(1, 1), col, 5, 6);
        return;
    }
    alpha_gradient(out.ref(3, 1), col, 3, 4);
    alpha_gradient(out.ref(1, 3), col, 3, 4);
    alpha_gradient(out.ref(3, 0), col, 1, 4);
    alpha_gradient(out.ref(0, 3), col, 1, 4);
    alpha_gradient(out.ref(2, 2), col, 1, 3);
    out.ref(3, 3) = col;
    out.ref(3, 2) = col;
    out.ref(2, 3) = col;
}
void blend_line_diagonal(std::uint32_t col, const OutputBlock &out)
{
    const unsigned s = out.scale;
    if (s == 2)
    {
        alpha_gradient(out.ref(1, 1), col, 1, 2);
        return;
    }
    alpha_gradient(out.ref(s - 1, s / 2), col, 1, 2);
    alpha_gradient(out.ref(s - 2, s / 2 + 1), col, 1, 2);
    out.ref(s - 1, s - 1) = col;
}
void blend_corner(std::uint32_t col, const OutputBlock &out)
{
    // A round corner: 1 - pi/4 of the corner texel (2x), or its 4x equivalent.
    if (out.scale == 2)
    {
        alpha_gradient(out.ref(1, 1), col, 21, 100);
        return;
    }
    alpha_gradient(out.ref(3, 3), col, 68, 100);
    alpha_gradient(out.ref(3, 2), col, 9, 100);
    alpha_gradient(out.ref(2, 3), col, 9, 100);
}
// 3x3 kernel a..i (row-major) seen after `rotation` clockwise quarter turns.
constexpr std::array<std::array<std::uint8_t, 9>, 4> kernel_rotation{{{0, 1, 2, 3, 4, 5, 6, 7, 8},
                                                                      {6, 3, 0, 7, 4, 1, 8, 5, 2},
                                                                      {8, 7, 6, 5, 4, 3, 2, 1, 0},
                                                                      {2, 5, 8, 1, 4, 7, 0, 3, 6}}};
void blend_pixel(const std::array<const Pixel *, 9> &kernel, const OutputBlock &out, std::uint8_t info)
{
    const auto &map = kernel_rotation[out.rotation];
    const auto &b = *kernel[map[1]], &c = *kernel[map[2]], &d = *kernel[map[3]], &e = *kernel[map[4]],
               &f = *kernel[map[5]], &g = *kernel[map[6]], &h = *kernel[map[7]], &i = *kernel[map[8]];
    const auto blend = rotate_blend(info, out.rotation);
    if (bottom_right(blend) < blend_normal)
        return;
    const bool line_blend = [&] {
        if (bottom_right(blend) >= blend_dominant)
            return true;
        // No second blending in an adjacent rotation (insular pixels), but double blending for 90 degree
        // corners; no full blending for L-shapes (corner only).
        if (top_right(blend) != blend_none && !equal_color(e, g))
            return false;
        if (bottom_left(blend) != blend_none && !equal_color(e, c))
            return false;
        if (!equal_color(e, i) && equal_color(g, h) && equal_color(h, i) && equal_color(i, f) && equal_color(f, c))
            return false;
        return true;
    }();
    const std::uint32_t px = distance(e, f) <= distance(e, h) ? f.argb : h.argb;
    if (!line_blend)
    {
        blend_corner(px, out);
        return;
    }
    const double fg = distance(f, g), hc = distance(h, c);
    const bool shallow = XbrzConfig::steep_direction_threshold * fg <= hc && e.argb != g.argb && d.argb != g.argb;
    const bool steep = XbrzConfig::steep_direction_threshold * hc <= fg && e.argb != c.argb && b.argb != c.argb;
    if (shallow)
        steep ? blend_line_steep_and_shallow(px, out) : blend_line_shallow(px, out);
    else
        steep ? blend_line_steep(px, out) : blend_line_diagonal(px, out);
}
std::vector<Pixel> xbrz_pixels(std::span<const std::uint16_t> words)
{
    constexpr double kb = 0.0593, kr = 0.2627, kg = 1.0 - kb - kr; // ITU-R BT.2020
    constexpr double scale_b = 0.5 / (1.0 - kb), scale_r = 0.5 / (1.0 - kr);
    std::vector<Pixel> pixels(words.size());
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        const std::uint32_t w = words[i];
        auto &p = pixels[i];
        p.opaque = (w & 0x8000u) != 0;
        if (!p.opaque)
        {
            p = {0, 0, 0, 0, false};
            continue;
        }
        const auto r = expand5((w >> 10) & 31u), g = expand5((w >> 5) & 31u), b = expand5(w & 31u);
        p.argb = 0xff000000u | (r << 16) | (g << 8) | b;
        const double y = kr * r + kg * g + kb * b;
        p.y = static_cast<float>(y);
        p.cb = static_cast<float>(scale_b * (b - y));
        p.cr = static_cast<float>(scale_r * (r - y));
    }
    return pixels;
}
std::vector<std::uint32_t> xbrz_scale(const std::vector<Pixel> &src, std::uint32_t width, std::uint32_t height,
                                      unsigned k)
{
    const std::size_t ow = std::size_t(width) * k;
    std::vector<std::uint32_t> out(ow * height * k);
    const auto at = [&](std::int64_t x, std::int64_t y) -> const Pixel * {
        return &src[wrap_or_clamp(y, height, false) * width + wrap_or_clamp(x, width, false)];
    };
    // Corner between (x,y), (x+1,y), (x,y+1), (x+1,y+1) for every texel (clamped 4x4 kernel).
    // The ten distances of the corner test are all between diagonal neighbours: diag(u, v) between (u, v)
    // and (u+1, v+1), anti(u, v) between (u+1, v) and (u, v+1), for u in [-1, width], v in [-1, height].
    const std::size_t gw = std::size_t(width) + 2;
    std::vector<double> diag(gw * (std::size_t(height) + 2)), anti(diag.size());
    parallel_rows(height + 2, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t v = first; v < last; ++v)
            for (std::size_t u = 0; u < gw; ++u)
            {
                const auto x = static_cast<std::int64_t>(u) - 1, y = static_cast<std::int64_t>(v) - 1;
                diag[v * gw + u] = distance(*at(x, y), *at(x + 1, y + 1));
                anti[v * gw + u] = distance(*at(x + 1, y), *at(x, y + 1));
            }
    });
    const auto d = [&](std::int64_t x, std::int64_t y) { return diag[std::size_t(y + 1) * gw + std::size_t(x + 1)]; };
    const auto a = [&](std::int64_t x, std::int64_t y) { return anti[std::size_t(y + 1) * gw + std::size_t(x + 1)]; };
    std::vector<Corners> corners(src.size());
    parallel_rows(height, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const auto sx = static_cast<std::int64_t>(x), sy = static_cast<std::int64_t>(y);
                const double jg = a(sx - 1, sy) + a(sx, sy - 1) + a(sx, sy + 1) + a(sx + 1, sy) +
                                  XbrzConfig::center_direction_bias * a(sx, sy);
                const double fk = d(sx - 1, sy) + d(sx, sy + 1) + d(sx, sy - 1) + d(sx + 1, sy) +
                                  XbrzConfig::center_direction_bias * d(sx, sy);
                corners[std::size_t(y) * width + x] =
                    preprocess_corners(*at(sx, sy), *at(sx + 1, sy), *at(sx, sy + 1), *at(sx + 1, sy + 1), jg, fk);
            }
    });
    // Each texel writes only its own k x k output block.
    parallel_rows(height, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const auto index = std::size_t(y) * width + x;
                // The four corners of this texel; corners left of or above the image are "none", as in the
                // reference implementation's row-buffer sequence.
                unsigned info = corners[index].f << 4;
                if (y > 0)
                    info |= unsigned(corners[index - width].j) << 2;
                if (x > 0)
                    info |= unsigned(corners[index - 1].g) << 6;
                if (x > 0 && y > 0)
                    info |= corners[index - width - 1].k;
                std::uint32_t *origin = out.data() + std::size_t(y) * k * ow + std::size_t(x) * k;
                for (unsigned i = 0; i < k; ++i)
                    std::fill_n(origin + i * ow, k, src[index].argb);
                if (info == 0)
                    continue;
                std::array<const Pixel *, 9> kernel{};
                for (int dy = 0; dy < 3; ++dy)
                    for (int dx = 0; dx < 3; ++dx)
                        kernel[static_cast<std::size_t>(dy * 3 + dx)] =
                            at(static_cast<std::int64_t>(x) + dx - 1, static_cast<std::int64_t>(y) + dy - 1);
                for (unsigned rotation = 0; rotation < 4; ++rotation)
                    blend_pixel(kernel, {origin, ow, k, rotation}, static_cast<std::uint8_t>(info));
            }
    });
    return out;
}
// Local contrast of every source texel (largest xBRZ distance to its 8 neighbours): 255 at an alpha
// edge, a few units inside a 5-bit gradient.
std::vector<float> contrast_weights(const std::vector<Pixel> &src, std::uint32_t width, std::uint32_t height)
{
    constexpr float flat = 16.0f, edge = 48.0f; // below: smooth scaler only; above: xBRZ only
    std::vector<float> weight(src.size());
    parallel_rows(height, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
            for (std::uint32_t x = 0; x < width; ++x)
            {
                const auto &p = src[std::size_t(y) * width + x];
                double c2 = 0.0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const auto &q = src[wrap_or_clamp(static_cast<std::int64_t>(y) + dy, height, false) * width +
                                            wrap_or_clamp(static_cast<std::int64_t>(x) + dx, width, false)];
                        c2 = std::max(c2, distance_squared(p, q));
                    }
                const double c = std::sqrt(c2);
                weight[std::size_t(y) * width + x] =
                    std::clamp((static_cast<float>(c) - flat) / (edge - flat), 0.0f, 1.0f);
            }
    });
    return weight;
}
// Clamped linear interpolation position of every output coordinate over n source texels.
struct Linear
{
    std::vector<std::uint32_t> i0, i1;
    std::vector<float> f;
};
Linear make_linear(std::uint32_t n, std::uint32_t k)
{
    Linear l;
    const std::size_t count = std::size_t(n) * k;
    l.i0.resize(count);
    l.i1.resize(count);
    l.f.resize(count);
    for (std::uint32_t o = 0; o < count; ++o)
    {
        const double s = std::clamp(source_position(o, k), 0.0, static_cast<double>(n - 1));
        l.i0[o] = static_cast<std::uint32_t>(s);
        l.i1[o] = std::min(l.i0[o] + 1, n - 1);
        l.f[o] = static_cast<float>(s - l.i0[o]);
    }
    return l;
}
std::vector<OutTexel> pixel_art_scale(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                                      std::uint32_t k)
{
    const auto pixels = xbrz_pixels(words);
    const auto sharp = xbrz_scale(pixels, width, height, k);
    const auto smooth = smooth_scale(words, width, height, k, false);
    const auto weight = contrast_weights(pixels, width, height);
    const auto lx = make_linear(width, k), ly = make_linear(height, k);
    const std::uint32_t ow = width * k, oh = height * k;
    std::vector<OutTexel> out(std::size_t(ow) * oh);
    parallel_rows(oh, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
        {
            const float *w0 = weight.data() + std::size_t(ly.i0[y]) * width;
            const float *w1 = weight.data() + std::size_t(ly.i1[y]) * width;
            const float fy = ly.f[y];
            const std::uint16_t *source_row = words.data() + std::size_t(y / k) * width;
            for (std::uint32_t x = 0; x < ow; ++x)
            {
                const auto index = std::size_t(y) * ow + x;
                const std::uint32_t p = sharp[index];
                const auto alpha = alpha_of(p);
                OutTexel texel{};
                texel.opaque = alpha >= 128;
                if (alpha == 0)
                {
                    // Untouched transparent texel: the source texel's own colour.
                    const auto s = decode(source_row[x / k]);
                    texel.r = s.r;
                    texel.g = s.g;
                    texel.b = s.b;
                }
                else
                {
                    texel.r = from_8bit[(p >> 16) & 255u];
                    texel.g = from_8bit[(p >> 8) & 255u];
                    texel.b = from_8bit[p & 255u];
                }
                const auto &s = smooth[index];
                if (texel.opaque && s.opaque)
                {
                    const auto x0 = lx.i0[x], x1 = lx.i1[x];
                    const float fx = lx.f[x];
                    const float top = w0[x0] + (w0[x1] - w0[x0]) * fx;
                    const float bottom = w1[x0] + (w1[x1] - w1[x0]) * fx;
                    const float edge = top + (bottom - top) * fy; // 1: xBRZ, 0: smooth
                    texel.r = s.r + (texel.r - s.r) * edge;
                    texel.g = s.g + (texel.g - s.g) * edge;
                    texel.b = s.b + (texel.b - s.b) * edge;
                }
                out[index] = texel;
            }
        }
    });
    return out;
}
std::vector<OutTexel> scale(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                            std::uint32_t factor, Scaler scaler)
{
    if ((factor != 2 && factor != 4) || width == 0 || height == 0 || words.size() != std::size_t(width) * height ||
        width > 16384 / factor || height > 16384 / factor)
        throw std::invalid_argument("host texture upscale: unsupported extent or factor");
    switch (scaler)
    {
    case Scaler::pixel_art:
        return pixel_art_scale(words, width, height, factor);
    case Scaler::smooth:
        return smooth_scale(words, width, height, factor, false);
    case Scaler::smooth_wrap:
        return smooth_scale(words, width, height, factor, true);
    case Scaler::none:
        break;
    }
    throw std::invalid_argument("host texture upscale: no scaler");
}

// ------------------------------------------------------------------------------------------------
// Registry of classified uploads and the result cache
// ------------------------------------------------------------------------------------------------
std::uint64_t content_hash(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height)
{
    std::uint64_t h = 0xcbf29ce484222325ull ^ (std::uint64_t(width) << 32 | height);
    for (const auto w : words)
    {
        h ^= w;
        h *= 0x100000001b3ull;
    }
    return h;
}
struct RegistryKey
{
    std::uint32_t token, width, height;
    std::uint64_t hash;
    bool operator==(const RegistryKey &) const = default;
};
struct RegistryKeyHash
{
    std::size_t operator()(const RegistryKey &k) const noexcept
    {
        return static_cast<std::size_t>(k.hash ^ (std::uint64_t(k.token) * 0x9E3779B97F4A7C15ull));
    }
};
struct RegistryEntry
{
    TextureClass texture_class{}; // the factor is recorded per texture identity (R28b), not here
};
struct CacheEntry
{
    std::uint64_t hash{};
    std::uint32_t width{}, height{}, factor{};
    Scaler scaler{};
    std::vector<std::uint16_t> source;
    std::shared_ptr<const std::vector<std::uint16_t>> result;
    std::size_t bytes() const noexcept
    {
        return (source.size() + (result ? result->size() : 0)) * sizeof(std::uint16_t);
    }
};
constexpr std::size_t registry_limit = 65536;
constexpr std::size_t cache_budget = 192u * 1024u * 1024u;
struct State
{
    std::mutex mutex;
    std::unordered_map<RegistryKey, RegistryEntry, RegistryKeyHash> registry;
    std::list<CacheEntry> cache; // most recently used first
    std::size_t cache_bytes{};
    Statistics stats;
};
State &state()
{
    // NEW HOST (R35): never destroyed. Background prewarm and cache-write tasks may still run while the
    // process exits; a destroyed state under them was an access violation at exit.
    static State *const s = new State;
    return *s;
}
} // namespace

TextureClass classify_source_name(std::string_view source_name) noexcept
{
    try
    {
        const auto text = normalized(source_name);
        if (component(text, "fonts"))
            return TextureClass::glyph;
        if (component(text, "overlays"))
            return TextureClass::hud;
        if (component(text, "frontend"))
            return component(std::string_view(text).substr(9), "tiles") ? TextureClass::menu_tile
                                                                        : TextureClass::menu_art;
        if (component(text, "boot") || component(text, "loading"))
            return TextureClass::menu_art;
    }
    catch (...)
    {
    }
    return TextureClass::world;
}
Scaler scaler_for(TextureClass c) noexcept
{
    switch (c)
    {
    case TextureClass::glyph:
    case TextureClass::hud:
        return Scaler::pixel_art;
    case TextureClass::menu_art:
        return Scaler::smooth;
    case TextureClass::menu_tile:
        return Scaler::smooth_wrap;
    case TextureClass::world:
        break;
    }
    return Scaler::none;
}
const char *class_name(TextureClass c) noexcept
{
    switch (c)
    {
    case TextureClass::glyph:
        return "glyph";
    case TextureClass::hud:
        return "hud";
    case TextureClass::menu_art:
        return "menu_art";
    case TextureClass::menu_tile:
        return "menu_tile";
    case TextureClass::world:
        break;
    }
    return "world";
}
ScopedTextureSource::ScopedTextureSource(TextureClass c) noexcept : previous_(pending_hint)
{
    pending_hint = c;
}
ScopedTextureSource::ScopedTextureSource(std::string_view source_name) noexcept
    : ScopedTextureSource(classify_source_name(source_name))
{
}
ScopedTextureSource::~ScopedTextureSource()
{
    pending_hint = previous_;
}
std::optional<TextureClass> pending_class() noexcept
{
    return pending_hint;
}
std::uint32_t factor_for_output(std::uint32_t output_width, std::uint32_t output_height) noexcept
{
    const double m = std::min(static_cast<double>(output_width) / 640.0, static_cast<double>(output_height) / 480.0);
    return m < 1.25 ? 1u : m <= 3.0 ? 2u : 4u;
}
// The largest usable factor not above `preferred` (the result must stay within 16384 texels), or 0.
static std::uint32_t usable_factor(std::uint32_t preferred, std::uint32_t width, std::uint32_t height) noexcept
{
    auto factor = preferred;
    while (factor > 1 && (width > 16384 / factor || height > 16384 / factor))
        factor /= 2;
    return factor == 2 || factor == 4 ? factor : 0;
}
std::optional<Decision> decide(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                               std::span<const std::uint16_t> words, std::uint32_t factor_limit) noexcept
{
    seen_factor_limit = factor_limit;
    const bool enabled = factor_limit > 1;
    try
    {
        const auto hint = pending_hint;
        // World uploads (the vast majority) never hash or touch the registry.
        if ((hint && *hint == TextureClass::world) || width == 0 || height == 0 ||
            words.size() != std::size_t(width) * height)
            return std::nullopt;
        auto &s = state();
        if (!hint)
        {
            if (!enabled)
                return std::nullopt;
            const std::lock_guard lock(s.mutex);
            if (s.registry.empty())
                return std::nullopt;
        }
        const auto hash = content_hash(words, width, height);
        const RegistryKey key{token, width, height, hash};
        const std::lock_guard lock(s.mutex);
        RegistryEntry *entry{};
        if (hint)
        {
            if (s.registry.size() >= registry_limit && !s.registry.contains(key))
                s.registry.clear();
            entry = &s.registry[key];
            if (entry->texture_class != *hint)
                *entry = {*hint};
        }
        else
        {
            const auto found = s.registry.find(key);
            if (found == s.registry.end())
                return std::nullopt;
            entry = &found->second;
            ++s.stats.registry_hits;
        }
        const auto scaler = scaler_for(entry->texture_class);
        if (!enabled || scaler == Scaler::none || std::min(width, height) < 32)
            return std::nullopt;
        // R28b: the identity's recorded limit decides (a rebind or replacement repeats its original's).
        const auto factor = usable_factor(factor_limit, width, height);
        if (factor == 0)
            return std::nullopt;
        return Decision{entry->texture_class, scaler, factor, hash};
    }
    catch (...)
    {
        return std::nullopt;
    }
}
namespace
{
constexpr std::uint32_t disk_kind = 0x43535055; // "UPSC"
constexpr std::uint64_t disk_algorithm_version = 1;
constexpr auto disk_threshold = std::chrono::milliseconds(2);
// The cached entry equal to this request, moved to the front (caller holds the lock).
std::shared_ptr<const std::vector<std::uint16_t>> find_cached(State &s, std::span<const std::uint16_t> words,
                                                              std::uint32_t width, std::uint32_t height,
                                                              const Decision &decision)
{
    for (auto it = s.cache.begin(); it != s.cache.end(); ++it)
        if (it->hash == decision.content_hash && it->width == width && it->height == height &&
            it->factor == decision.factor && it->scaler == decision.scaler &&
            std::equal(it->source.begin(), it->source.end(), words.begin(), words.end()))
        {
            s.cache.splice(s.cache.begin(), s.cache, it);
            return s.cache.front().result;
        }
    return nullptr;
}
} // namespace
std::shared_ptr<const std::vector<std::uint16_t>> upscaled_words(std::span<const std::uint16_t> words,
                                                                 std::uint32_t width, std::uint32_t height,
                                                                 const Decision &decision, UpscaleSource *source)
{
    auto &s = state();
    {
        const std::lock_guard lock(s.mutex);
        if (auto found = find_cached(s, words, width, height, decision))
        {
            ++s.stats.cache_hits;
            ++s.stats.upscaled;
            if (source)
                *source = UpscaleSource::memory;
            return found;
        }
    }
    // R30: a result computed in an earlier run of this build (host_texture_disk_cache), else compute it.
    const host_texture_disk_cache::Key key{
        disk_kind,
        {decision.factor, static_cast<std::uint64_t>(decision.scaler), disk_algorithm_version, 0},
        width,
        height,
        words};
    const bool disk = host_texture_disk_cache::enabled();
    std::shared_ptr<const std::vector<std::uint16_t>> result;
    UpscaleSource origin = UpscaleSource::computed;
    std::chrono::steady_clock::duration elapsed{};
    if (disk)
        if (const auto payload = host_texture_disk_cache::load(key);
            payload && payload->size() == std::size_t(width) * height * decision.factor * decision.factor * 2)
        {
            auto loaded = std::make_shared<std::vector<std::uint16_t>>(payload->size() / 2);
            std::memcpy(loaded->data(), payload->data(), payload->size());
            result = std::move(loaded);
            origin = UpscaleSource::disk;
        }
    if (!result)
    {
        const auto start = std::chrono::steady_clock::now();
        result = std::make_shared<const std::vector<std::uint16_t>>(
            upscale_a1r5g5b5(words, width, height, decision.factor, decision.scaler));
        elapsed = std::chrono::steady_clock::now() - start;
        if (disk && elapsed >= disk_threshold)
        {
            std::vector<std::uint8_t> payload(result->size() * 2);
            std::memcpy(payload.data(), result->data(), payload.size());
            host_texture_disk_cache::store(key, std::move(payload));
        }
    }
    CacheEntry entry{decision.content_hash,
                     width,
                     height,
                     decision.factor,
                     decision.scaler,
                     std::vector<std::uint16_t>(words.begin(), words.end()),
                     result};
    const std::lock_guard lock(s.mutex);
    ++s.stats.upscaled;
    if (origin == UpscaleSource::disk)
        ++s.stats.disk_hits;
    else
    {
        s.stats.computed_texels += result->size();
        s.stats.compute_nanoseconds +=
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    }
    if (source)
        *source = origin;
    // R30: another thread (background prewarm) may have inserted the same result meanwhile: keep one.
    if (auto found = find_cached(s, words, width, height, decision))
        return found;
    s.cache_bytes += entry.bytes();
    s.cache.push_front(std::move(entry));
    while (s.cache_bytes > cache_budget && s.cache.size() > 1)
    {
        s.cache_bytes -= s.cache.back().bytes();
        s.cache.pop_back();
    }
    return result;
}
void prewarm_rgba8(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                   TextureClass texture_class) noexcept
{
    try
    {
        if (width == 0 || height == 0 || rgba.size() != std::size_t(width) * height * 4)
            return;
        // The same words MaterialTextureReplacement::upload_to_destination makes from this image (the exact
        // inverse of the decode_tm 5-bit expansion), so the later replacement upload hits the cache.
        auto words = std::make_shared<std::vector<std::uint16_t>>(std::size_t(width) * height);
        for (std::size_t i = 0; i < words->size(); ++i)
        {
            const auto channel = [&](std::size_t c) { return (std::uint32_t(rgba[i * 4 + c]) * 31u + 127u) / 255u; };
            (*words)[i] = static_cast<std::uint16_t>((rgba[i * 4 + 3] ? 0x8000u : 0u) | (channel(0) << 10) |
                                                     (channel(1) << 5) | channel(2));
        }
        // NEW HOST (R32 perf-swap): the words the replacement will upload are the upscaled ones when the
        // replacement's upscale applies (prepare_polygon_upload: limit > 1, an upscaled class, a usable
        // factor), else these literal words. Their converted levels (edge fix, 8-bit expansion, mips) are
        // prewarmed for every shape a replacement can have (host_texture_processing::prewarm_replacement).
        std::shared_ptr<const std::vector<std::uint16_t>> uploaded = words;
        std::uint32_t uploaded_width = width, uploaded_height = height;
        const auto scaler = scaler_for(texture_class);
        if (seen_factor_limit > 1 && prewarm_wanted(texture_class, width, height))
            if (const auto factor = usable_factor(seen_factor_limit, width, height); factor != 0)
            {
                uploaded = upscaled_words(*words, width, height,
                                          Decision{texture_class, scaler, factor, content_hash(*words, width, height)});
                uploaded_width = width * factor;
                uploaded_height = height * factor;
            }
        host_texture_processing::prewarm_replacement(std::move(uploaded), uploaded_width, uploaded_height);
    }
    catch (...)
    {
    }
}
bool prewarm_wanted(TextureClass texture_class, std::uint32_t width, std::uint32_t height) noexcept
{
    return scaler_for(texture_class) != Scaler::none && std::min(width, height) >= 32 && width <= 16384 &&
           height <= 16384;
}
void prewarm_words(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                   TextureClass texture_class) noexcept
{
    try
    {
        const auto scaler = scaler_for(texture_class);
        if (seen_factor_limit <= 1 || !prewarm_wanted(texture_class, width, height) ||
            words.size() != std::size_t(width) * height)
            return;
        const auto factor = usable_factor(seen_factor_limit, width, height);
        if (factor == 0)
            return;
        (void)upscaled_words(words, width, height,
                             Decision{texture_class, scaler, factor, content_hash(words, width, height)});
    }
    catch (...)
    {
    }
}
std::vector<std::uint16_t> upscale_a1r5g5b5(std::span<const std::uint16_t> words, std::uint32_t width,
                                            std::uint32_t height, std::uint32_t factor, Scaler scaler)
{
    const auto texels = scale(words, width, height, factor, scaler);
    const std::uint32_t ow = width * factor;
    std::vector<std::uint16_t> out(texels.size());
    parallel_rows(height * factor, [&](std::uint32_t first, std::uint32_t last) {
        for (std::uint32_t y = first; y < last; ++y)
            for (std::uint32_t x = 0; x < ow; ++x)
                out[std::size_t(y) * ow + x] = quantize(texels[std::size_t(y) * ow + x], x, y);
    });
    return out;
}
std::vector<std::uint8_t> upscale_rgba8(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                                        std::uint32_t factor, Scaler scaler)
{
    const auto texels = scale(words, width, height, factor, scaler);
    std::vector<std::uint8_t> out(texels.size() * 4);
    for (std::size_t i = 0; i < texels.size(); ++i)
    {
        out[i * 4] = byte(texels[i].r);
        out[i * 4 + 1] = byte(texels[i].g);
        out[i * 4 + 2] = byte(texels[i].b);
        out[i * 4 + 3] = texels[i].opaque ? 255u : 0u;
    }
    return out;
}
Statistics statistics() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    auto result = s.stats;
    result.cache_bytes = s.cache_bytes;
    result.registry_entries = s.registry.size();
    return result;
}
void set_worker_threads(unsigned count) noexcept
{
    worker_limit = std::clamp(count, 1u, 8u);
}
void clear_cache() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    s.cache.clear();
    s.cache_bytes = 0;
}
} // namespace sgn::host_texture_upscale
