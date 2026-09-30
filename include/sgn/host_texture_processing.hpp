#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

// NEW HOST (R30) CPU conversion chain of the polygon texture upload (not recovered behaviour): the R27 edge
// fix, the R28 8-bit expansion (with gradient deband) and the R23/R28 mip chains. The four conversions were
// moved here from polygon_d3d12.cpp with unchanged arithmetic; R30 runs their independent rows/texels in
// bands on host_parallel's pool (every output value is computed by the same expression from the same
// inputs, so the result does not depend on the band split) and caches whole results:
//   - in memory: exact input words + extent + request -> result (LRU; large inputs, i.e. upscaled 2D art,
//     up to 128 MB; small ones up to 32 MB (R35; was the last 4), which also covers a material uploading
//     the same words to both of its stage destinations, the case the former single-entry caches served);
//   - on disk (host_texture_disk_cache), when a result took at least 2 ms to compute.
namespace sgn::host_texture_processing
{
struct Request
{
    bool edge_fix{};
    bool wide{};             // B8G8R8A8 path (high-precision profile)
    std::uint32_t levels{1}; // mip levels including level 0
    bool operator==(const Request &) const = default;
};
struct Levels
{
    // 16-bit path: level 0 after the edge fix (empty without edge fix: level 0 is the input itself) and
    // levels 1..levels-1.
    std::vector<std::uint16_t> edge_fixed;
    std::vector<std::vector<std::uint16_t>> mips;
    // 8-bit path: levels 0..levels-1 (B | G << 8 | R << 16 | A << 24).
    std::vector<std::vector<std::uint32_t>> wide;
};
// NEW HOST (R32 perf-swap) the conversion shape of one texture identity (its R28b recorded profile
// with the device's MIP support applied): the Request of an upload is a pure function of the shape and
// the uploaded extent (request_for), which PolygonD3D12::upload_a1r5g5b5 uses to build its request.
struct Shape
{
    bool edge_fix{};
    bool wide{};    // B8G8R8A8 (high-precision profile)
    bool mipmaps{}; // the profile asks for mips AND the upload format supports them
    bool operator==(const Shape &) const = default;
};
constexpr Request request_for(const Shape &shape, std::uint32_t width, std::uint32_t height) noexcept
{
    const bool mipmapped = shape.mipmaps && (width > 1 || height > 1);
    return {shape.edge_fix, shape.wide,
            mipmapped ? static_cast<std::uint32_t>(std::bit_width((std::max)(width, height))) : 1u};
}
// NEW HOST (R32 perf-swap) replacement prewarm. The renderer notes the shape of every identity that a
// 430D40 replacement can write (generated identities, and every identity actually replaced); the race
// load then calls prewarm_replacement for each image a race can swap in (the already upscaled words the
// swap will upload), which computes (or loads from the disk cache) the levels of every noted shape into a
// separate store that the LRU lists never evict (FIFO, 256 MB). The in-race swap then finds them there:
// by identity when it uploads the same immutable word vector (the upscale cache's result), otherwise by
// exact content. Counter swap.prewarm_hit. The levels are the same values prepare() would compute.
void note_replacement_shape(const Shape &) noexcept;
void prewarm_replacement(std::shared_ptr<const std::vector<std::uint16_t>> words, std::uint32_t width,
                         std::uint32_t height) noexcept;
// The converted levels of one upload, or null when the request converts nothing (no edge fix, 16-bit, one
// level: the input is uploaded as it is). Throws FormatError / std::bad_alloc like the former inline code.
// Profiler scopes (game thread): host.texture_levels (whole call), host.texture_levels_memory_hit,
// host.texture_levels_disk_hit, and on a computed result the former gpu.texture_edge_fix,
// gpu.texture_high_precision and gpu.mip_generation.
std::shared_ptr<const Levels> prepare(std::uint32_t width, std::uint32_t height, std::span<const std::uint16_t> words,
                                      const Request &);

// The pure conversions (deterministic; any thread).
std::vector<std::uint16_t> a1r5g5b5_edge_fill(std::uint32_t width, std::uint32_t height,
                                              std::span<const std::uint16_t> words);
std::vector<std::vector<std::uint16_t>> a1r5g5b5_mip_chain(std::uint32_t width, std::uint32_t height,
                                                           std::span<const std::uint16_t> words, std::uint32_t levels);
std::vector<std::uint32_t> a1r5g5b5_to_bgra8(std::uint32_t width, std::uint32_t height,
                                             std::span<const std::uint16_t> words);
std::vector<std::vector<std::uint32_t>> bgra8_mip_chain(std::uint32_t width, std::uint32_t height,
                                                        std::span<const std::uint32_t> texels, std::uint32_t levels);

struct Statistics
{
    std::uint64_t requests{}, memory_hits{}, disk_hits{}, computed{};
    std::uint64_t compute_nanoseconds{};
    std::size_t cache_bytes{}, cache_entries{};
};
Statistics statistics() noexcept;
void clear_cache() noexcept;
} // namespace sgn::host_texture_processing
