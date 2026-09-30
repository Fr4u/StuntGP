#include "sgn/host_texture_processing.hpp"
#include "sgn/format_require.hpp"
#include "sgn/host_parallel.hpp"
#include "sgn/host_texture_disk_cache.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <list>
#include <mutex>

// NEW HOST (R30). See host_texture_processing.hpp. The bodies of the four conversions are the R23/R27/R28
// code formerly in polygon_d3d12.cpp; only their loops over independent rows/texels are split into bands.
namespace sgn::host_texture_processing
{
namespace
{
using sgn::format_check::require;
constexpr unsigned band_limit = 8;
constexpr std::uint32_t band_texels = 4096; // smallest band worth a pool hand-off
std::uint32_t row_grain(std::uint32_t width) noexcept
{
    return std::max(1u, band_texels / std::max(1u, width));
}
} // namespace

// NEW HOST (R27) texture edge fix (not recovered behaviour). Returns the words
// with the RGB of every alpha-0 texel replaced by the rounded mean of its
// already-filled 8-neighbours, layer by layer outward from the opaque texels
// (multi-source breadth-first dilation, clamp addressing), until every
// transparent texel reachable from an opaque one is filled. Alpha bits are
// never changed, so alpha test, A2C and mip coverage see identical alpha. Each
// layer is computed from a snapshot of the previous layers only, so the result
// is independent of scan order: a pure, deterministic function of the words.
// A texture without opaque (or without transparent) texels is returned as is.
// R30: the colours of one layer are computed in bands (each reads only the
// snapshot and writes only its own entries).
std::vector<std::uint16_t> a1r5g5b5_edge_fill(std::uint32_t width, std::uint32_t height,
                                              std::span<const std::uint16_t> words)
{
    require(words.size() == std::size_t(width) * height, "polygon edge fill extent");
    std::vector<std::uint16_t> out(words.begin(), words.end());
    std::vector<std::uint8_t> state(out.size()); // 0 unfilled, 1 filled (opaque or earlier layer), 2 queued
    std::vector<std::uint32_t> layer, next;
    const auto queue_neighbours = [&](std::uint32_t index, std::vector<std::uint32_t> &into) {
        const std::uint32_t x = index % width, y = index / width;
        for (std::uint32_t ny = y ? y - 1 : 0; ny <= y + 1 && ny < height; ++ny)
            for (std::uint32_t nx = x ? x - 1 : 0; nx <= x + 1 && nx < width; ++nx)
            {
                const auto neighbour = ny * width + nx;
                if (state[neighbour] == 0)
                {
                    state[neighbour] = 2;
                    into.push_back(neighbour);
                }
            }
    };
    std::size_t opaque = 0;
    for (std::size_t i = 0; i < out.size(); ++i)
        if (out[i] & 0x8000u)
        {
            state[i] = 1;
            ++opaque;
        }
    // R30: without transparent (or without opaque) texels nothing is filled: the words as they are, without
    // visiting every texel's neighbourhood (most world textures are fully opaque).
    if (opaque == 0 || opaque == out.size())
        return out;
    // The first layer is the set of unfilled texels next to an opaque one. The former loop queued it from
    // the opaque side; here it is collected from the (usually fewer) transparent texels, in bands. Only the
    // order of the list differs, and the result does not depend on it (see above).
    {
        struct Found
        {
            std::uint32_t first{};
            std::vector<std::uint32_t> items;
        };
        std::vector<Found> found(band_limit);
        std::atomic<unsigned> used{0};
        host_parallel::for_bands(height, row_grain(width), band_limit, [&](std::uint32_t first, std::uint32_t last) {
            auto &slot = found[used.fetch_add(1)];
            slot.first = first;
            auto &into = slot.items;
            for (std::uint32_t y = first; y < last; ++y)
                for (std::uint32_t x = 0; x < width; ++x)
                {
                    if (state[std::size_t(y) * width + x] != 0)
                        continue;
                    bool next_to_opaque = false;
                    for (std::uint32_t ny = y ? y - 1 : 0; !next_to_opaque && ny <= y + 1 && ny < height; ++ny)
                        for (std::uint32_t nx = x ? x - 1 : 0; nx <= x + 1 && nx < width; ++nx)
                            if (state[std::size_t(ny) * width + nx] == 1)
                            {
                                next_to_opaque = true;
                                break;
                            }
                    if (next_to_opaque)
                        into.push_back(y * width + x);
                }
        });
        found.resize(used.load());
        std::sort(found.begin(), found.end(), [](const Found &a, const Found &b) { return a.first < b.first; });
        for (const auto &band : found) // row order, whatever the schedule
            layer.insert(layer.end(), band.items.begin(), band.items.end());
        for (const auto index : layer)
            state[index] = 2;
    }
    std::vector<std::uint16_t> colours;
    while (!layer.empty())
    {
        colours.resize(layer.size());
        host_parallel::for_bands(
            static_cast<std::uint32_t>(layer.size()), 2048, band_limit, [&](std::uint32_t first, std::uint32_t last) {
                for (std::size_t k = first; k < last; ++k)
                {
                    const std::uint32_t x = layer[k] % width, y = layer[k] / width;
                    unsigned r = 0, g = 0, b = 0, count = 0;
                    for (std::uint32_t ny = y ? y - 1 : 0; ny <= y + 1 && ny < height; ++ny)
                        for (std::uint32_t nx = x ? x - 1 : 0; nx <= x + 1 && nx < width; ++nx)
                        {
                            const auto neighbour = ny * width + nx;
                            if (state[neighbour] != 1)
                                continue;
                            const unsigned w = out[neighbour];
                            r += (w >> 10) & 31u;
                            g += (w >> 5) & 31u;
                            b += w & 31u;
                            ++count;
                        }
                    // count >= 1: a texel is queued only next to a filled one.
                    const auto mean = [count](unsigned sum) { return (sum + count / 2) / count; };
                    colours[k] = static_cast<std::uint16_t>((mean(r) << 10) | (mean(g) << 5) | mean(b)); // alpha 0
                }
            });
        for (std::size_t k = 0; k < layer.size(); ++k)
        {
            out[layer[k]] = colours[k];
            state[layer[k]] = 1;
        }
        next.clear();
        for (const auto index : layer)
            queue_neighbours(index, next);
        layer.swap(next);
    }
    return out;
}

namespace
{
// NEW HOST (R23) mip chain of literal A1R5G5B5 words (not recovered behaviour;
// the game uploads single-level textures). Pure and deterministic: the same
// words always give the same levels, so a device-loss re-upload of retained
// recovery words rebuilds identical mips.
//  - Colour: 2x2 box in linear light (5-bit sRGB-like decode), weighted by
//    alpha so transparent texels never bleed their (key) colour into edges.
//  - Alpha: the format keeps one bit. Each level keeps the SAME fraction of
//    opaque texels as level 0 (coverage-preserving alpha scale: the bit
//    threshold is the level's own quantile of its box-filtered alpha, ties
//    ordered-dithered), so alpha-tested fences/foliage keep their density
//    instead of dissolving (or filling in) with distance.
struct MipTexel
{
    float r, g, b, a;
};
float srgb_to_linear(float x)
{
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}
// The keep-th largest alpha of a level (1 <= keep <= n): the value std::nth_element with std::greater
// places at keep - 1 (the former selection). R30: levels of at least 65536 texels use a radix select.
// Every alpha is finite and >= +0 (sums and quotients of 0/1 coverage), so the bit patterns order like
// the floats: a banded 16-bit histogram of the high bits finds the bucket holding the value, and
// nth_element then runs on that bucket's values only.
float kth_largest_alpha(const std::vector<MipTexel> &level, std::size_t keep, std::vector<float> &scratch)
{
    const std::size_t n = level.size();
    if (n < 65536)
    {
        scratch.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            scratch[i] = level[i].a;
        std::nth_element(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(keep - 1), scratch.end(),
                         std::greater<float>());
        return scratch[keep - 1];
    }
    constexpr std::size_t buckets = 65536;
    std::vector<std::vector<std::uint32_t>> counts(band_limit);
    std::atomic<unsigned> used{0};
    host_parallel::for_bands(static_cast<std::uint32_t>(n), 65536u, band_limit,
                             [&](std::uint32_t first, std::uint32_t last) {
                                 auto &count = counts[used.fetch_add(1)];
                                 count.assign(buckets, 0u);
                                 for (std::size_t i = first; i < last; ++i)
                                     ++count[std::bit_cast<std::uint32_t>(level[i].a) >> 16];
                             });
    counts.resize(used.load());
    std::vector<std::size_t> total(buckets);
    for (const auto &count : counts) // integer sums: any band order
        for (std::size_t b = 0; b < buckets; ++b)
            total[b] += count[b];
    // Highest buckets first: the bucket holding the keep-th largest, and its rank inside that bucket.
    std::size_t above = 0, bucket = buckets;
    while (bucket > 0)
    {
        --bucket;
        if (above + total[bucket] >= keep)
            break;
        above += total[bucket];
    }
    scratch.clear();
    for (std::size_t i = 0; i < n; ++i)
        if ((std::bit_cast<std::uint32_t>(level[i].a) >> 16) == bucket)
            scratch.push_back(level[i].a);
    const auto rank = keep - above; // 1..scratch.size()
    std::nth_element(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(rank - 1), scratch.end(),
                     std::greater<float>());
    return scratch[rank - 1];
}
// Level loop shared by the 16-bit (R23) and 8-bit (R28) chains: identical
// arithmetic; only the level-0 decode and the per-texel store differ.
// R30: the box filter and the store run in row/texel bands; the rank selection
// (nth_element over values, then over unique tie keys) stays sequential and its
// result is unique for its input, whatever the order of the elements.
template <class Word, class Store>
std::vector<std::vector<Word>> coverage_mip_chain(std::uint32_t width, std::uint32_t height,
                                                  std::vector<MipTexel> current, double coverage, std::uint32_t levels,
                                                  const Store &store)
{
    std::vector<std::vector<Word>> chain;
    chain.reserve(levels - 1);
    std::vector<float> ranked;
    std::uint32_t cw = width, ch = height;
    for (std::uint32_t level = 1; level < levels; ++level)
    {
        const std::uint32_t nw = (std::max)(1u, cw / 2), nh = (std::max)(1u, ch / 2), sx = cw > 1 ? 2u : 1u,
                            sy = ch > 1 ? 2u : 1u;
        std::vector<MipTexel> next(std::size_t(nw) * nh);
        host_parallel::for_bands(nh, row_grain(nw), band_limit, [&](std::uint32_t first, std::uint32_t last) {
            for (std::uint32_t y = first; y < last; ++y)
                for (std::uint32_t x = 0; x < nw; ++x)
                {
                    float wr = 0, wg = 0, wb = 0, pr = 0, pg = 0, pb = 0, alpha = 0;
                    for (std::uint32_t dy = 0; dy < sy; ++dy)
                        for (std::uint32_t dx = 0; dx < sx; ++dx)
                        {
                            const auto &t = current[std::size_t((std::min)(ch - 1, y * 2 + dy)) * cw +
                                                    (std::min)(cw - 1, x * 2 + dx)];
                            wr += t.r * t.a;
                            wg += t.g * t.a;
                            wb += t.b * t.a;
                            pr += t.r;
                            pg += t.g;
                            pb += t.b;
                            alpha += t.a;
                        }
                    const float count = static_cast<float>(sx * sy);
                    next[std::size_t(y) * nw + x] = alpha > 0
                                                        ? MipTexel{wr / alpha, wg / alpha, wb / alpha, alpha / count}
                                                        : MipTexel{pr / count, pg / count, pb / count, 0.0f};
                }
        });
        const std::size_t n = next.size();
        const auto keep = static_cast<std::size_t>(std::llround(coverage * static_cast<double>(n)));
        std::vector<std::uint8_t> opaque_bits(n, keep >= n ? 1u : 0u);
        if (keep > 0 && keep < n)
        {
            // The `keep` texels with the highest filtered alpha become opaque.
            // Box-filtered 1-bit alpha is full of exact ties (a 1-texel fence is
            // 0.5 in every other column, then 0.25 everywhere), so ties are
            // broken by an 8x8 ordered-dither rank, then by index: the level
            // keeps exactly the level-0 fraction, spread evenly.
            // R30: the same selection, computed without the two sequential nth_element passes:
            //  - threshold: the keep-th largest value, a unique value whatever the method. Large levels use
            //    a radix select (kth_largest_alpha); small levels keep the former nth_element.
            //  - ties: the former code marked the `wanted` smallest keys (dither << 32 | index, unique) by
            //    nth_element. Keys order by dither first, so that set is every tie of a dither rank below
            //    the last rank plus the lowest indices of the last rank, found by counting per rank.
            const float threshold = kth_largest_alpha(next, keep, ranked);
            struct TieBand
            {
                std::uint32_t first{};
                std::size_t above{};
                std::array<std::size_t, 64> per_rank{};
                std::vector<std::uint64_t> ties;
            };
            std::vector<TieBand> bands(band_limit);
            std::atomic<unsigned> used{0};
            host_parallel::for_bands(
                static_cast<std::uint32_t>(n), band_texels, band_limit, [&](std::uint32_t first, std::uint32_t last) {
                    auto &band = bands[used.fetch_add(1)];
                    band.first = first;
                    for (std::size_t i = first; i < last; ++i)
                    {
                        if (next[i].a > threshold)
                        {
                            opaque_bits[i] = 1u;
                            ++band.above;
                        }
                        else if (next[i].a == threshold)
                        {
                            const std::uint32_t x = static_cast<std::uint32_t>(i % nw),
                                                y = static_cast<std::uint32_t>(i / nw), v = x ^ y;
                            // 8x8 ordered-dither rank: bit-reversed interleave of (x^y, x)
                            const std::uint64_t dither = ((v & 1u) << 5) | ((x & 1u) << 4) | ((v & 2u) << 2) |
                                                         ((x & 2u) << 1) | ((v & 4u) >> 1) | ((x & 4u) >> 2);
                            band.ties.push_back((dither << 32) | i);
                            ++band.per_rank[dither];
                        }
                    }
                });
            bands.resize(used.load());
            std::sort(bands.begin(), bands.end(), [](const TieBand &a, const TieBand &b) { return a.first < b.first; });
            std::size_t above = 0, tie_count = 0;
            std::array<std::size_t, 64> per_rank{};
            for (const auto &band : bands)
            {
                above += band.above;
                tie_count += band.ties.size();
                for (unsigned r = 0; r < 64; ++r)
                    per_rank[r] += band.per_rank[r];
            }
            const auto wanted = keep - above; // 1..tie_count by the choice of threshold
            if (threshold > 0.0f)
            {
                // The last rank taken and how many of its ties (lowest indices first) complete `wanted`;
                // every tie is taken when wanted >= tie_count (as the former loop did).
                std::uint64_t last_rank = 64;
                std::size_t of_last = 0;
                if (wanted < tie_count)
                {
                    std::size_t before = 0;
                    for (unsigned r = 0; r < 64; ++r)
                    {
                        if (before + per_rank[r] >= wanted)
                        {
                            last_rank = r;
                            of_last = wanted - before;
                            break;
                        }
                        before += per_rank[r];
                    }
                }
                for (const auto &band : bands) // ascending index within each rank
                    for (const auto key : band.ties)
                    {
                        const auto rank = key >> 32;
                        if (rank < last_rank)
                            opaque_bits[static_cast<std::size_t>(key & 0xFFFFFFFFu)] = 1u;
                        else if (rank == last_rank && of_last > 0)
                        {
                            opaque_bits[static_cast<std::size_t>(key & 0xFFFFFFFFu)] = 1u;
                            --of_last;
                        }
                    }
            }
        }
        std::vector<Word> out(n);
        host_parallel::for_bands(static_cast<std::uint32_t>(n), band_texels, band_limit,
                                 [&](std::uint32_t first, std::uint32_t last) {
                                     for (std::size_t i = first; i < last; ++i)
                                         out[i] = store(next[i], opaque_bits[i] != 0);
                                 });
        chain.push_back(std::move(out));
        current = std::move(next);
        cw = nw;
        ch = nh;
    }
    return chain;
}
// NEW HOST (R28) high-precision texture path (not recovered behaviour).
// n-bit channel -> 8 bits by bit replication (the exact UNORM value: 5-bit v ->
// v<<3|v>>2, 6-bit v -> v<<2|v>>4, 4-bit v -> v*17, 1-bit -> 0/255). Only
// A1R5G5B5 words reach the polygon upload path; R5G6B5/A4R4G4B4 would use the
// same rule with 6/4 bits.
constexpr unsigned replicate_bits(unsigned value, unsigned bits) noexcept
{
    unsigned out = 0;
    for (int shift = 8 - static_cast<int>(bits); shift > -static_cast<int>(bits); shift -= static_cast<int>(bits))
        out |= shift >= 0 ? value << shift : value >> -shift;
    return out & 255u;
}
static_assert(replicate_bits(31, 5) == 255 && replicate_bits(16, 5) == 132 && replicate_bits(1, 1) == 255 &&
              replicate_bits(63, 6) == 255 && replicate_bits(15, 4) == 255 && replicate_bits(8, 4) == 136);
constexpr auto expand5 = [] {
    std::array<std::uint8_t, 32> table{};
    for (unsigned v = 0; v < 32; ++v)
        table[v] = static_cast<std::uint8_t>(replicate_bits(v, 5));
    return table;
}();
constexpr std::uint32_t radius = 4, window = 2 * radius + 1;
// Horizontal 9-wide window (AND when `all`, else OR) of one row over its edge
// replication, by doubling. p, f, s2: scratch of span + 2 * radius words.
void horizontal_window(const std::uint32_t *source, std::uint32_t *result, bool all, std::uint32_t span,
                       std::uint32_t *const p, std::uint32_t *const f, std::uint32_t *const s2)
{
    const std::uint32_t padded = span + 2 * radius;
    for (std::uint32_t x = 0; x < span; ++x)
        p[radius + x] = source[x];
    for (std::uint32_t k = 0; k < radius; ++k)
    {
        p[k] = source[0];
        p[radius + span + k] = source[span - 1];
    }
    if (all)
    {
        for (std::uint32_t i = 0; i + 1 < padded; ++i)
            f[i] = p[i] & p[i + 1];
        for (std::uint32_t i = 0; i + 3 < padded; ++i)
            s2[i] = f[i] & f[i + 2];
        for (std::uint32_t i = 0; i + 7 < padded; ++i)
            f[i] = s2[i] & s2[i + 4];
        for (std::uint32_t i = 0; i < span; ++i)
            result[i] = f[i] & p[i + 8];
    }
    else
    {
        for (std::uint32_t i = 0; i + 1 < padded; ++i)
            f[i] = p[i] | p[i + 1];
        for (std::uint32_t i = 0; i + 3 < padded; ++i)
            s2[i] = f[i] | f[i + 2];
        for (std::uint32_t i = 0; i + 7 < padded; ++i)
            f[i] = s2[i] | s2[i + 4];
        for (std::uint32_t i = 0; i < span; ++i)
            result[i] = f[i] | p[i + 8];
    }
}
} // namespace

std::vector<std::vector<std::uint16_t>> a1r5g5b5_mip_chain(std::uint32_t width, std::uint32_t height,
                                                           std::span<const std::uint16_t> words, std::uint32_t levels)
{
    require(levels > 1 && words.size() == std::size_t(width) * height, "polygon mip chain extent");
    // R30: the tables are constants of the algorithm; they are built once per process (same expressions)
    // instead of once per texture.
    struct Tables
    {
        std::array<float, 32> decode{};
        std::array<float, 31> rounding{}; // rounding[k]: linear value of 5-bit code k+0.5
        std::array<std::uint8_t, 4097> start{};
        Tables()
        {
            for (unsigned i = 0; i < 32; ++i)
                decode[i] = srgb_to_linear(static_cast<float>(i) / 31.0f);
            for (unsigned i = 0; i < 31; ++i)
                rounding[i] = srgb_to_linear((static_cast<float>(i) + 0.5f) / 31.0f);
            // encode(v) == number of rounding edges <= v (the nearest 5-bit code in the
            // encoded domain). A 4096-bucket start plus an exact forward scan; edges are
            // more than one bucket apart, so the scan takes at most one step.
            for (unsigned i = 0; i < start.size(); ++i)
            {
                const float at = static_cast<float>(i) / 4096.0f;
                start[i] = static_cast<std::uint8_t>(
                    std::count_if(rounding.begin(), rounding.end(), [at](float edge) { return edge <= at; }));
            }
        }
    };
    static const Tables tables;
    const auto &decode = tables.decode;
    const auto &rounding = tables.rounding;
    const auto &start = tables.start;
    const auto encode = [&](float linear) -> unsigned {
        unsigned code = start[(std::min)(4096u, static_cast<unsigned>(linear * 4096.0f))];
        while (code < 31 && rounding[code] <= linear)
            ++code;
        return code;
    };
    std::vector<MipTexel> current(words.size());
    std::size_t opaque = 0;
    for (const unsigned w : words)
        opaque += (w & 0x8000u) != 0 ? 1u : 0u;
    host_parallel::for_bands(static_cast<std::uint32_t>(words.size()), band_texels, band_limit,
                             [&](std::uint32_t first, std::uint32_t last) {
                                 for (std::size_t i = first; i < last; ++i)
                                 {
                                     const unsigned w = words[i];
                                     const bool a = (w & 0x8000u) != 0;
                                     current[i] = {decode[(w >> 10) & 31u], decode[(w >> 5) & 31u], decode[w & 31u],
                                                   a ? 1.0f : 0.0f};
                                 }
                             });
    const double coverage = static_cast<double>(opaque) / static_cast<double>(words.size());
    return coverage_mip_chain<std::uint16_t>(
        width, height, std::move(current), coverage, levels, [&](const MipTexel &t, bool opaque_bit) {
            return static_cast<std::uint16_t>((opaque_bit ? 0x8000u : 0u) | (encode(t.r) << 10) | (encode(t.g) << 5) |
                                              encode(t.b));
        });
}
// A1R5G5B5 -> B8G8R8A8 (memory order B, G, R, A; word = B | G<<8 | R<<16 | A<<24).
// Every texel is the exact bit replication, except inside smooth 5-bit gradients,
// which are debanded: a texel is "smooth" when its whole 9x9 window (clamped)
// is opaque, every channel spans at most one 5-bit code, and the centre row and
// the centre column each change code at most twice (a banded ramp, not 1-LSB
// dither noise or detail). There the value is the 9x9 box mean of the expanded
// codes (the gradient reconstructed at 8-bit precision), quantised with a 4x4
// ordered dither (+-0.5 LSB of 8 bits). Anything else (edges, detail, noise,
// cut-outs, transparent texels) stays the exact replication, so edges stay crisp.
// R30: the three passes (per texel, per row, per output row) run in bands; the
// window scratch and column accumulators are per band.
std::vector<std::uint32_t> a1r5g5b5_to_bgra8(std::uint32_t width, std::uint32_t height,
                                             std::span<const std::uint16_t> words)
{
    require(words.size() == std::size_t(width) * height, "polygon 8-bit expansion extent");
    const std::size_t n = words.size();
    const std::uint32_t span = width; // by-value bound: stores cannot alias it
    std::vector<std::uint32_t> out(n);
    // Per channel (0 R, 1 G, 2 B) a presence mask 1 << code, plus "opaque" as
    // an all-ones/zero mask. A window spans at most one 5-bit code exactly when
    // the OR of its masks is one bit or two adjacent bits; it is flat when it
    // is one bit. OR and AND over windows are element-wise and vectorize.
    std::array<std::vector<std::uint32_t>, 3> mask;
    std::vector<std::uint32_t> opaque(n);
    for (auto &plane : mask)
        plane.resize(n);
    host_parallel::for_bands(static_cast<std::uint32_t>(n), band_texels, band_limit,
                             [&](std::uint32_t first, std::uint32_t last) {
                                 for (std::size_t i = first; i < last; ++i)
                                 {
                                     const unsigned w = words[i];
                                     const unsigned r = (w >> 10) & 31u, g = (w >> 5) & 31u, b = w & 31u;
                                     mask[0][i] = 1u << r;
                                     mask[1][i] = 1u << g;
                                     mask[2][i] = 1u << b;
                                     opaque[i] = (w & 0x8000u) != 0 ? ~0u : 0u;
                                     out[i] = unsigned(expand5[b]) | unsigned(expand5[g]) << 8 |
                                              unsigned(expand5[r]) << 16 | (opaque[i] ? 0xFF000000u : 0u);
                                 }
                             });
    // Horizontal 9-wide windows over edge-replicated rows (replication does not
    // change a clamped window's OR/AND), by doubling: [i,i+1], [i,i+3], [i,i+7],
    // then the ninth element. Sums use zero padding and the clamped count.
    static_assert(window == 9, "the doubling window above is written for 9 texels");
    std::array<std::vector<std::uint32_t>, 3> row_mask, row_sum;
    std::vector<std::uint32_t> row_opaque(n);
    for (unsigned c = 0; c < 3; ++c)
    {
        row_mask[c].resize(n);
        row_sum[c].resize(n);
    }
    host_parallel::for_bands(height, row_grain(span), band_limit, [&](std::uint32_t first, std::uint32_t last) {
        std::vector<std::uint32_t> pad(span + 2 * radius), f(span + 2 * radius), s2(span + 2 * radius);
        for (std::uint32_t y = first; y < last; ++y)
        {
            const std::size_t row = std::size_t(y) * span;
            horizontal_window(opaque.data() + row, row_opaque.data() + row, true, span, pad.data(), f.data(),
                              s2.data());
            for (unsigned c = 0; c < 3; ++c)
            {
                horizontal_window(mask[c].data() + row, row_mask[c].data() + row, false, span, pad.data(), f.data(),
                                  s2.data());
                // Sliding sum of the expanded 8-bit values, zero outside the row.
                const unsigned shift = 10u - 5u * c;
                const auto value = [&](std::int64_t x) -> unsigned {
                    return x < 0 || x >= std::int64_t(span)
                               ? 0u
                               : expand5[(unsigned(words[row + std::size_t(x)]) >> shift) & 31u];
                };
                unsigned running = 0;
                for (std::int64_t x = -std::int64_t(radius); x <= std::int64_t(radius); ++x)
                    running += value(x);
                auto *const sum = row_sum[c].data() + row;
                for (std::uint32_t x = 0; x < span; ++x)
                {
                    sum[x] = running;
                    running = running + value(std::int64_t(x) + radius + 1) - value(std::int64_t(x) - radius);
                }
            }
        }
    });
    // Vertical combination over the clamped rows, then the per-texel decision.
    const auto changes = [&](std::size_t from, std::size_t step, std::uint32_t steps) {
        unsigned result = 0;
        for (std::uint32_t k = 0; k < steps; ++k)
            result += (words[from + k * step] & 0x7FFFu) != (words[from + (k + 1) * step] & 0x7FFFu) ? 1u : 0u;
        return result;
    };
    const auto one_step = [](std::uint32_t m) {
        const std::uint32_t low = m & (0u - m);
        return (m & ~(low | low << 1)) == 0;
    };
    constexpr float bayer[16]{0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    host_parallel::for_bands(height, row_grain(span), band_limit, [&](std::uint32_t first, std::uint32_t last) {
        // R30: the clamped 9-row window of each column slides down the band instead of being re-accumulated
        // from 9 rows for every output row. OR/AND windows: the same doubling as the horizontal pass over the
        // band's rows with edge replication (a replicated row is already inside the clamped window, and
        // OR/AND are idempotent, so the values are those of the clamped window). Sums: a running integer sum
        // (add the row entering the clamped window, subtract the row leaving it), exactly the clamped sum.
        const std::uint32_t rows = last - first, padded_rows = rows + 2 * radius;
        const auto source_row = [&](std::uint32_t j) {
            const auto k = std::int64_t(first) + std::int64_t(j) - std::int64_t(radius);
            return std::size_t(std::clamp<std::int64_t>(k, 0, std::int64_t(height) - 1)) * span;
        };
        std::array<std::vector<std::uint32_t>, 4> windowed; // 0: opaque (AND), 1..3: R, G, B masks (OR)
        std::vector<std::uint32_t> l1(std::size_t(padded_rows) * span), l2(std::size_t(padded_rows) * span);
        for (unsigned plane = 0; plane < 4; ++plane)
        {
            const std::uint32_t *const source = plane == 0 ? row_opaque.data() : row_mask[plane - 1].data();
            const bool all = plane == 0;
            const auto combine = [all, span](std::uint32_t *__restrict out, const std::uint32_t *__restrict a,
                                             const std::uint32_t *__restrict b) {
                if (all)
                    for (std::uint32_t x = 0; x < span; ++x)
                        out[x] = a[x] & b[x];
                else
                    for (std::uint32_t x = 0; x < span; ++x)
                        out[x] = a[x] | b[x];
            };
            const auto at = [span](std::vector<std::uint32_t> &v, std::uint32_t j) {
                return v.data() + std::size_t(j) * span;
            };
            for (std::uint32_t j = 0; j + 1 < padded_rows; ++j)
                combine(at(l1, j), source + source_row(j), source + source_row(j + 1));
            for (std::uint32_t j = 0; j + 3 < padded_rows; ++j)
                combine(at(l2, j), at(l1, j), at(l1, j + 2));
            for (std::uint32_t j = 0; j + 7 < padded_rows; ++j)
                combine(at(l1, j), at(l2, j), at(l2, j + 4));
            windowed[plane].resize(std::size_t(rows) * span);
            for (std::uint32_t j = 0; j < rows; ++j)
                combine(at(windowed[plane], j), at(l1, j), source + source_row(j + 2 * radius));
        }
        std::array<std::vector<std::uint32_t>, 3> column_sum;
        for (unsigned c = 0; c < 3; ++c)
        {
            column_sum[c].assign(span, 0u);
            const std::uint32_t y0 = first >= radius ? first - radius : 0, y1 = (std::min)(height - 1, first + radius);
            for (std::uint32_t yy = y0; yy <= y1; ++yy)
            {
                std::uint32_t *__restrict total = column_sum[c].data();
                const std::uint32_t *__restrict row_total = row_sum[c].data() + std::size_t(yy) * span;
                for (std::uint32_t x = 0; x < span; ++x)
                    total[x] += row_total[x];
            }
        }
        for (std::uint32_t y = first; y < last; ++y)
        {
            const std::uint32_t y0 = y >= radius ? y - radius : 0, y1 = (std::min)(height - 1, y + radius);
            if (y > first)
                for (unsigned c = 0; c < 3; ++c)
                {
                    std::uint32_t *__restrict total = column_sum[c].data();
                    if (y + radius <= height - 1)
                    {
                        const std::uint32_t *__restrict entering = row_sum[c].data() + std::size_t(y + radius) * span;
                        for (std::uint32_t x = 0; x < span; ++x)
                            total[x] += entering[x];
                    }
                    if (y >= radius + 1)
                    {
                        const std::uint32_t *__restrict leaving =
                            row_sum[c].data() + std::size_t(y - radius - 1) * span;
                        for (std::uint32_t x = 0; x < span; ++x)
                            total[x] -= leaving[x];
                    }
                }
            const std::size_t j = std::size_t(y - first) * span;
            const std::uint32_t *const column_opaque = windowed[0].data() + j;
            const std::array<const std::uint32_t *, 3> column_mask{windowed[1].data() + j, windowed[2].data() + j,
                                                                   windowed[3].data() + j};
            for (std::uint32_t x = 0; x < span; ++x)
            {
                const auto r = column_mask[0][x], g = column_mask[1][x], b = column_mask[2][x];
                if (!column_opaque[x] || !one_step(r) || !one_step(g) || !one_step(b))
                    continue; // cut-out or real edge/detail: exact replication
                if ((r & (r - 1)) == 0 && (g & (g - 1)) == 0 && (b & (b - 1)) == 0)
                    continue; // flat: exact replication
                const std::uint32_t x0 = x >= radius ? x - radius : 0, x1 = (std::min)(span - 1, x + radius);
                if (changes(std::size_t(y) * span + x0, 1, x1 - x0) > 2 ||
                    changes(std::size_t(y0) * span + x, span, y1 - y0) > 2)
                    continue; // 1-LSB dither or detail, not a band
                const float count = static_cast<float>((x1 - x0 + 1) * (y1 - y0 + 1));
                const float threshold = (bayer[(y & 3u) * 4u + (x & 3u)] + 0.5f) / 16.0f;
                std::uint32_t pixel = 0xFF000000u;
                for (unsigned c = 0; c < 3; ++c)
                {
                    const float mean = static_cast<float>(column_sum[c][x]) / count;
                    const auto q = (std::min)(255u, static_cast<unsigned>(mean + threshold));
                    pixel |= q << (16 - 8 * c); // c 0 = R (bits 16), 1 = G (8), 2 = B (0)
                }
                out[std::size_t(y) * span + x] = pixel;
            }
        }
    });
    return out;
}
// 8-bit mip chain: the R23 algorithm (linear-light alpha-weighted box, binary
// coverage-preserving alpha, now 0/255) over 8-bit level 0, stored with the
// exact nearest 8-bit sRGB code per level: no 5-bit re-quantisation.
std::vector<std::vector<std::uint32_t>> bgra8_mip_chain(std::uint32_t width, std::uint32_t height,
                                                        std::span<const std::uint32_t> texels, std::uint32_t levels)
{
    require(levels > 1 && texels.size() == std::size_t(width) * height, "polygon 8-bit mip chain extent");
    struct Tables // R30: built once per process, as in a1r5g5b5_mip_chain
    {
        std::array<float, 256> decode{};
        std::array<float, 255> rounding{}; // rounding[k]: linear value of 8-bit code k+0.5
        std::array<std::uint8_t, 4097> start{};
        Tables()
        {
            for (unsigned i = 0; i < 256; ++i)
                decode[i] = srgb_to_linear(static_cast<float>(i) / 255.0f);
            for (unsigned i = 0; i < 255; ++i)
                rounding[i] = srgb_to_linear((static_cast<float>(i) + 0.5f) / 255.0f);
            // Same exact encode as the 16-bit chain: 4096-bucket start (edges <= the
            // bucket, one merge pass over the sorted edges) plus a forward scan (8-bit
            // edges are at least 0.0003 apart in linear light, > 1/4096).
            unsigned counted = 0;
            for (unsigned i = 0; i < start.size(); ++i)
            {
                const float at = static_cast<float>(i) / 4096.0f;
                while (counted < 255 && rounding[counted] <= at)
                    ++counted;
                start[i] = static_cast<std::uint8_t>(counted);
            }
        }
    };
    static const Tables tables;
    const auto &decode = tables.decode;
    const auto &rounding = tables.rounding;
    const auto &start = tables.start;
    const auto encode = [&](float linear) -> unsigned {
        unsigned value = start[(std::min)(4096u, static_cast<unsigned>(linear * 4096.0f))];
        while (value < 255 && rounding[value] <= linear)
            ++value;
        return value;
    };
    std::vector<MipTexel> current(texels.size());
    std::size_t opaque = 0;
    for (const auto t : texels)
        opaque += (t >> 24) >= 128u ? 1u : 0u;
    host_parallel::for_bands(static_cast<std::uint32_t>(texels.size()), band_texels, band_limit,
                             [&](std::uint32_t first, std::uint32_t last) {
                                 for (std::size_t i = first; i < last; ++i)
                                 {
                                     const auto t = texels[i];
                                     const bool a = (t >> 24) >= 128u;
                                     current[i] = {decode[(t >> 16) & 255u], decode[(t >> 8) & 255u], decode[t & 255u],
                                                   a ? 1.0f : 0.0f};
                                 }
                             });
    const double coverage = static_cast<double>(opaque) / static_cast<double>(texels.size());
    return coverage_mip_chain<std::uint32_t>(
        width, height, std::move(current), coverage, levels, [&](const MipTexel &t, bool opaque_bit) {
            return (opaque_bit ? 0xFF000000u : 0u) | encode(t.r) << 16 | encode(t.g) << 8 | encode(t.b);
        });
}

// ------------------------------------------------------------------------------------------------
// Result cache
// ------------------------------------------------------------------------------------------------
namespace
{
constexpr std::uint64_t algorithm_version = 1;  // R30 layout of the cached levels
constexpr std::uint32_t disk_kind = 0x4C564C32; // "2LVL"
constexpr std::size_t large_texels = 131072;    // upscaled 2D art and larger
constexpr std::size_t large_budget = 128u * 1024u * 1024u;
// NEW HOST (R35): the small results are kept under a byte budget (was the last 4): a menu that cycles
// through literal (not upscaled) swap pictures kept missing with 4 entries.
constexpr std::size_t small_budget = 32u * 1024u * 1024u;
constexpr auto disk_threshold = std::chrono::milliseconds(2);

std::uint64_t request_bits(const Request &r) noexcept
{
    return (r.edge_fix ? 1u : 0u) | (r.wide ? 2u : 0u) | (std::uint64_t(r.levels) << 8);
}
std::size_t level_texels(std::uint32_t width, std::uint32_t height, std::uint32_t level) noexcept
{
    return std::size_t((std::max)(1u, width >> level)) * (std::max)(1u, height >> level);
}
std::size_t levels_bytes(const Levels &l) noexcept
{
    std::size_t bytes = l.edge_fixed.size() * 2;
    for (const auto &m : l.mips)
        bytes += m.size() * 2;
    for (const auto &m : l.wide)
        bytes += m.size() * 4;
    return bytes;
}
struct Entry
{
    std::uint64_t hash{};
    std::uint32_t width{}, height{};
    Request request;
    std::vector<std::uint16_t> input;
    std::shared_ptr<const Levels> result;
    std::size_t bytes{};
};
// NEW HOST (R32 perf-swap): a prewarmed replacement result. `input` is the exact (immutable) word vector
// the prewarm converted, owned here, so a later upload of that same vector is recognised by address (no
// other vector can have it while it lives) and any other upload by exact content.
struct Prewarmed
{
    std::uint64_t hash{};
    std::uint32_t width{}, height{};
    Request request;
    std::shared_ptr<const std::vector<std::uint16_t>> input;
    std::shared_ptr<const Levels> result;
    std::size_t bytes{};
};
constexpr std::size_t prewarm_budget = 256u * 1024u * 1024u;
constexpr std::size_t shape_limit = 4;
struct State
{
    std::mutex mutex;
    std::list<Entry> large, small; // most recently used first
    std::size_t large_bytes{};
    std::size_t small_bytes{};      // NEW HOST (R35)
    std::list<Prewarmed> prewarmed; // oldest first (FIFO within prewarm_budget), never LRU-evicted
    std::size_t prewarmed_bytes{};
    std::vector<Shape> shapes; // noted replacement shapes, most recent first
    Statistics stats;
};
State &state()
{
    // NEW HOST (R35): never destroyed. Background prewarm and cache-write tasks may still run while the
    // process exits; a destroyed state under them was an access violation at exit.
    static State *const s = new State;
    return *s;
}
template <class T> void put(std::vector<std::uint8_t> &out, const T &value)
{
    const auto at = out.size();
    out.resize(at + sizeof value);
    std::memcpy(out.data() + at, &value, sizeof value);
}
template <class T> void put_vector(std::vector<std::uint8_t> &out, const std::vector<T> &values)
{
    put(out, std::uint64_t(values.size()));
    const auto at = out.size();
    out.resize(at + values.size() * sizeof(T));
    if (!values.empty())
        std::memcpy(out.data() + at, values.data(), values.size() * sizeof(T));
}
std::vector<std::uint8_t> serialize(const Levels &l)
{
    std::vector<std::uint8_t> out;
    out.reserve(levels_bytes(l) + 64);
    put_vector(out, l.edge_fixed);
    put(out, std::uint32_t(l.mips.size()));
    for (const auto &m : l.mips)
        put_vector(out, m);
    put(out, std::uint32_t(l.wide.size()));
    for (const auto &m : l.wide)
        put_vector(out, m);
    return out;
}
class Reader
{
    std::span<const std::uint8_t> bytes_;
    std::size_t at_{};

  public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    template <class T> bool get(T &value)
    {
        if (bytes_.size() - at_ < sizeof value)
            return false;
        std::memcpy(&value, bytes_.data() + at_, sizeof value);
        at_ += sizeof value;
        return true;
    }
    template <class T> bool get_vector(std::vector<T> &values, std::size_t expected)
    {
        std::uint64_t count{};
        if (!get(count) || count != expected || (bytes_.size() - at_) / sizeof(T) < count)
            return false;
        values.resize(static_cast<std::size_t>(count));
        if (count)
            std::memcpy(values.data(), bytes_.data() + at_, values.size() * sizeof(T));
        at_ += values.size() * sizeof(T);
        return true;
    }
    bool done() const noexcept
    {
        return at_ == bytes_.size();
    }
};
// Parses a cached payload and checks every level extent the request implies.
std::shared_ptr<Levels> deserialize(std::span<const std::uint8_t> bytes, std::uint32_t width, std::uint32_t height,
                                    const Request &request)
{
    auto l = std::make_shared<Levels>();
    Reader in(bytes);
    const bool narrow_edge = request.edge_fix && !request.wide;
    if (!in.get_vector(l->edge_fixed, narrow_edge ? level_texels(width, height, 0) : 0))
        return nullptr;
    std::uint32_t count{};
    const std::uint32_t mips = !request.wide && request.levels > 1 ? request.levels - 1 : 0;
    if (!in.get(count) || count != mips)
        return nullptr;
    l->mips.resize(count);
    for (std::uint32_t i = 0; i < count; ++i)
        if (!in.get_vector(l->mips[i], level_texels(width, height, i + 1)))
            return nullptr;
    const std::uint32_t wide = request.wide ? request.levels : 0;
    if (!in.get(count) || count != wide)
        return nullptr;
    l->wide.resize(count);
    for (std::uint32_t i = 0; i < count; ++i)
        if (!in.get_vector(l->wide[i], level_texels(width, height, i)))
            return nullptr;
    return in.done() ? l : nullptr;
}
std::shared_ptr<const Levels> compute(std::uint32_t width, std::uint32_t height, std::span<const std::uint16_t> words,
                                      const Request &request)
{
    auto result = std::make_shared<Levels>();
    std::span<const std::uint16_t> pixels = words;
    if (request.edge_fix)
    {
        SGN_HOST_PROFILE_SCOPE("gpu.texture_edge_fix");
        result->edge_fixed = a1r5g5b5_edge_fill(width, height, pixels);
        pixels = result->edge_fixed;
    }
    if (request.wide)
    {
        SGN_HOST_PROFILE_SCOPE("gpu.texture_high_precision");
        result->wide.push_back(a1r5g5b5_to_bgra8(width, height, pixels));
        if (request.levels > 1)
        {
            SGN_HOST_PROFILE_SCOPE("gpu.mip_generation");
            auto mips = bgra8_mip_chain(width, height, result->wide.front(), request.levels);
            for (auto &level : mips)
                result->wide.push_back(std::move(level));
        }
        std::vector<std::uint16_t>().swap(result->edge_fixed); // level 0 is wide[0]
    }
    else if (request.levels > 1)
    {
        SGN_HOST_PROFILE_SCOPE("gpu.mip_generation");
        result->mips = a1r5g5b5_mip_chain(width, height, pixels, request.levels);
    }
    return result;
}
void remember(std::uint64_t hash, std::uint32_t width, std::uint32_t height, std::span<const std::uint16_t> words,
              const Request &request, const std::shared_ptr<const Levels> &result)
{
    Entry entry{hash, width, height, request, std::vector<std::uint16_t>(words.begin(), words.end()), result, 0};
    entry.bytes = entry.input.size() * 2 + levels_bytes(*result);
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    if (words.size() >= large_texels)
    {
        s.large_bytes += entry.bytes;
        s.large.push_front(std::move(entry));
        while (s.large_bytes > large_budget && s.large.size() > 1)
        {
            s.large_bytes -= s.large.back().bytes;
            s.large.pop_back();
        }
    }
    else
    {
        s.small_bytes += entry.bytes;
        s.small.push_front(std::move(entry));
        while (s.small_bytes > small_budget && s.small.size() > 1)
        {
            s.small_bytes -= s.small.back().bytes;
            s.small.pop_back();
        }
    }
}
std::uint64_t content_key(std::uint32_t width, std::uint32_t height, std::span<const std::uint16_t> words,
                          const Request &request) noexcept
{
    return host_texture_disk_cache::hash_words(words, 0x9E3779B97F4A7C15ull ^ request_bits(request) ^
                                                          (std::uint64_t(width) << 40) ^ (std::uint64_t(height) << 20));
}
bool same_input(const Entry &e, std::uint64_t hash, std::uint32_t width, std::uint32_t height,
                std::span<const std::uint16_t> words, const Request &request) noexcept
{
    return e.hash == hash && e.width == width && e.height == height && e.request == request &&
           std::equal(e.input.begin(), e.input.end(), words.begin(), words.end());
}
// Under s.mutex. A prewarmed result for exactly these words: by the identity of the prewarmed vector
// (immutable and owned by the entry, so no other vector can have its address), which needs no hash, or
// by exact content once `hash` is known.
std::shared_ptr<const Levels> find_prewarmed(State &s, std::uint32_t width, std::uint32_t height,
                                             std::span<const std::uint16_t> words, const Request &request,
                                             const std::uint64_t *hash)
{
    for (const auto &p : s.prewarmed)
    {
        if (p.width != width || p.height != height || !(p.request == request))
            continue;
        if (p.input->data() == words.data() && p.input->size() == words.size())
            return p.result;
        if (hash && p.hash == *hash && std::equal(p.input->begin(), p.input->end(), words.begin(), words.end()))
            return p.result;
    }
    return nullptr;
}
void count_prewarm_hit() noexcept
{
    if (const auto sink = host_parallel::profile_sink())
        sink("swap.prewarm_hit", 0);
}
} // namespace

void note_replacement_shape(const Shape &shape) noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    const auto found = std::find(s.shapes.begin(), s.shapes.end(), shape);
    if (found != s.shapes.end())
    {
        std::rotate(s.shapes.begin(), found, found + 1);
        return;
    }
    try
    {
        s.shapes.insert(s.shapes.begin(), shape);
        if (s.shapes.size() > shape_limit)
            s.shapes.pop_back();
    }
    catch (...)
    {
    }
}
void prewarm_replacement(std::shared_ptr<const std::vector<std::uint16_t>> words, std::uint32_t width,
                         std::uint32_t height) noexcept
{
    try
    {
        if (!words || width == 0 || height == 0 || words->size() != std::size_t(width) * height)
            return;
        auto &s = state();
        std::vector<Shape> shapes;
        {
            const std::lock_guard lock(s.mutex);
            shapes = s.shapes;
        }
        for (const auto &shape : shapes)
        {
            const auto request = request_for(shape, width, height);
            if (!request.edge_fix && !request.wide && request.levels <= 1)
                continue; // such an upload converts nothing
            SGN_HOST_PROFILE_SCOPE("swap.prewarm_levels");
            const auto hash = content_key(width, height, *words, request);
            {
                const std::lock_guard lock(s.mutex);
                if (find_prewarmed(s, width, height, *words, request, &hash))
                    continue; // prewarmed at an earlier race load
            }
            // The regular path (memory lists, disk cache, or computation; a result that took at least 2 ms
            // is stored on disk too), then pinned in the prewarm store.
            const auto result = prepare(width, height, *words, request);
            if (!result)
                continue;
            Prewarmed pinned{hash, width, height, request, words, result, words->size() * 2 + levels_bytes(*result)};
            const std::lock_guard lock(s.mutex);
            if (find_prewarmed(s, width, height, *words, request, &hash))
                continue;
            s.prewarmed_bytes += pinned.bytes;
            s.prewarmed.push_back(std::move(pinned));
            while (s.prewarmed_bytes > prewarm_budget && s.prewarmed.size() > 1)
            {
                s.prewarmed_bytes -= s.prewarmed.front().bytes;
                s.prewarmed.pop_front();
            }
        }
    }
    catch (...)
    {
        // A failed prewarm only leaves the swap to convert its image itself, as before.
    }
}
std::shared_ptr<const Levels> prepare(std::uint32_t width, std::uint32_t height, std::span<const std::uint16_t> words,
                                      const Request &request)
{
    if (!request.edge_fix && !request.wide && request.levels <= 1)
        return nullptr;
    require(width > 0 && height > 0 && words.size() == std::size_t(width) * height && request.levels >= 1,
            "polygon texture conversion extent");
    SGN_HOST_PROFILE_SCOPE("host.texture_levels");
    auto &s = state();
    {
        // NEW HOST (R32 perf-swap): the prewarmed replacement of this exact vector, found without hashing.
        const std::lock_guard lock(s.mutex);
        if (auto found = find_prewarmed(s, width, height, words, request, nullptr))
        {
            ++s.stats.requests;
            ++s.stats.memory_hits;
            count_prewarm_hit();
            return found;
        }
    }
    const auto hash = content_key(width, height, words, request);
    {
        SGN_HOST_PROFILE_SCOPE("host.texture_levels_memory_lookup");
        const std::lock_guard lock(s.mutex);
        ++s.stats.requests;
        if (auto found = find_prewarmed(s, width, height, words, request, &hash))
        {
            ++s.stats.memory_hits;
            count_prewarm_hit();
            return found;
        }
        for (auto *list : {&s.small, &s.large})
            for (auto it = list->begin(); it != list->end(); ++it)
                if (same_input(*it, hash, width, height, words, request))
                {
                    list->splice(list->begin(), *list, it);
                    ++s.stats.memory_hits;
                    return list->front().result;
                }
    }
    const host_texture_disk_cache::Key key{
        disk_kind, {request_bits(request), algorithm_version, 0, 0}, width, height, words};
    const bool disk = host_texture_disk_cache::enabled();
    if (disk)
    {
        SGN_HOST_PROFILE_SCOPE("host.texture_levels_disk_lookup");
        if (auto payload = host_texture_disk_cache::load(key))
            if (auto loaded = deserialize(*payload, width, height, request))
            {
                std::shared_ptr<const Levels> result = std::move(loaded);
                remember(hash, width, height, words, request, result);
                const std::lock_guard lock(s.mutex);
                ++s.stats.disk_hits;
                return result;
            }
    }
    const auto start = std::chrono::steady_clock::now();
    auto result = compute(width, height, words, request);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    remember(hash, width, height, words, request, result);
    {
        const std::lock_guard lock(s.mutex);
        ++s.stats.computed;
        s.stats.compute_nanoseconds +=
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    }
    if (disk && elapsed >= disk_threshold)
        host_texture_disk_cache::store(key, serialize(*result));
    return result;
}
Statistics statistics() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    auto result = s.stats;
    result.cache_bytes = s.large_bytes;
    for (const auto &e : s.small)
        result.cache_bytes += e.bytes;
    for (const auto &p : s.prewarmed)
        result.cache_bytes += p.bytes;
    result.cache_entries = s.large.size() + s.small.size() + s.prewarmed.size();
    return result;
}
void clear_cache() noexcept
{
    auto &s = state();
    const std::lock_guard lock(s.mutex);
    s.large.clear();
    s.small.clear();
    s.small_bytes = 0;
    s.large_bytes = 0;
    s.prewarmed.clear();
    s.prewarmed_bytes = 0;
}
} // namespace sgn::host_texture_processing
