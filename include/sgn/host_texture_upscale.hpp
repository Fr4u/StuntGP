#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// NEW HOST (R28) sharp HUD (graphics audit item 7). Not recovered behaviour: the 2001 game uploads its
// 640x480-era 2D textures as they are. When host_graphics::Settings::sharp_hud is on, the renderer uploads
// the 2D UI textures (fonts, HUD overlays, menu art) upscaled 2x or 4x at load. Texture coordinates are
// normalised and every scaler here is centre-aligned (output texel j of source texel i covers exactly the
// same normalised span), so no UV, glyph metric or half-texel offset changes. 3D world, car and effect
// textures are never touched. This file is platform independent (no D3D); the renderer side is
// host_texture_upscale_d3d12.hpp.
namespace sgn::host_texture_upscale
{
// What a texture is, decided from its source file name (or from its upload site). Only the 2D UI classes
// are upscaled; `world` (every name outside the 2D directories, and every upload without a class) is
// uploaded literally.
enum class TextureClass : std::uint8_t
{
    world = 0,     // tracks, cars, sky, misc effects (flares, smoke, skid marks): 3D, never upscaled
    glyph = 1,     // Fonts\ strips packed into the 256x256 font atlases (1-bit alpha glyphs)
    hud = 2,       // Overlays\ race HUD sheets (dial, gear, digits, start lights, banners)
    menu_art = 3,  // FrontEnd\ pages, dialog, icons, logos, pads; Boot\ and Loading\ pictures
    menu_tile = 4, // FrontEnd\Tiles\ repeating background tiles (sampled with wrap addressing)
};
enum class Scaler : std::uint8_t
{
    none = 0,
    // Edge-directed pixel-art scaler (xBRZ 1.8 rules, alpha-aware) for strong edges, blended with the
    // smooth scaler where the source is locally flat (gradients), so shading does not posterise.
    pixel_art = 1,
    // Lanczos-3 on premultiplied colour with an anti-ringing clamp, clamp-to-edge borders.
    smooth = 2,
    smooth_wrap = 3, // the same with periodic borders (tiling textures)
};
TextureClass classify_source_name(std::string_view source_name) noexcept;
Scaler scaler_for(TextureClass) noexcept;
const char *class_name(TextureClass) noexcept;

// Names the class of the uploads made on THIS thread while the object lives (nestable; the previous
// value is restored). The loader, the font upload and the image replacements set it; the renderer reads
// it through pending_class(). Uploads without a class (device-loss rebind) use the registry below.
class ScopedTextureSource
{
    std::optional<TextureClass> previous_;

  public:
    explicit ScopedTextureSource(TextureClass) noexcept;
    explicit ScopedTextureSource(std::string_view source_name) noexcept;
    ~ScopedTextureSource();
    ScopedTextureSource(const ScopedTextureSource &) = delete;
    ScopedTextureSource &operator=(const ScopedTextureSource &) = delete;
};
std::optional<TextureClass> pending_class() noexcept;

// 2x or 4x from the output size: the 2D canvas is 640x480 scaled uniformly by m = min(w/640, h/480).
// m < 1.25: 1 (no upscale), m <= 3 (up to 1440p): 2, above (4K: 4.5): 4. The residual GPU magnification
// then stays within about 0.6..1.5, where bilinear (or point) sampling adds no visible blur.
std::uint32_t factor_for_output(std::uint32_t output_width, std::uint32_t output_height) noexcept;

// The decision for one upload. The class comes from pending_class() or, without one, from the registry
// of earlier classified uploads (token + exact source content), so a device-loss rebind of the retained
// source words repeats the class. NEW HOST (R28b): the factor is NOT chosen here: `factor_limit` is the
// upscale factor recorded in the texture identity's PolygonTextureProfile (1 = the identity never
// upscales: setting off when it was created, or small output), so a replacement or rebind of an identity
// always uses its original's factor. The applied factor is the largest of 2/4 not above it that keeps the
// result within 16384 texels. Classified 2D uploads are registered even when the limit is 1. Returns
// nullopt for a literal upload: limit 1, world class, texture smaller than 32 texels, or no usable factor.
struct Decision
{
    TextureClass texture_class{};
    Scaler scaler{};
    std::uint32_t factor{};
    std::uint64_t content_hash{}; // of the source words and extent (cache and registry key)
};
std::optional<Decision> decide(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                               std::span<const std::uint16_t> words, std::uint32_t factor_limit) noexcept;

// Upscaled A1R5G5B5 words for a decision, from the in-memory cache (per exact source content, scaler and
// factor; LRU, about 192 MB), from the persistent cache of this build (R30, host_texture_disk_cache: a
// result that took at least 2 ms is stored there), or computed. The three give identical words. Throws
// only std::bad_alloc / std::invalid_argument. `source` (optional) receives where the words came from.
enum class UpscaleSource : std::uint8_t
{
    memory,
    disk,
    computed,
};
std::shared_ptr<const std::vector<std::uint16_t>> upscaled_words(std::span<const std::uint16_t> words,
                                                                 std::uint32_t width, std::uint32_t height,
                                                                 const Decision &, UpscaleSource *source = nullptr);

// Fills the cache ahead of a later upload of this image (RGBA bytes, alpha 0 = transparent, converted to
// A1R5G5B5 exactly as the material replacement path does), with the factor limit of the renderer's
// latest upload (R28b). The race overlays use it at race load, so a mid-race overlay swap (countdown
// lights, lap record, results) does not compute its upscale during play. No-op while the setting is off
// or before the first upload; never throws. NEW HOST (R32 perf-swap): it also prewarms the converted levels
// of the words that replacement uploads (upscaled or, without upscale, literal) for every replacement
// shape (host_texture_processing::prewarm_replacement), so the swap does no CPU conversion at all.
void prewarm_rgba8(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                   TextureClass) noexcept;
// R30: the same for A1R5G5B5 words (the words a later 430D40 replacement uploads). Any thread; the loader
// runs it on host_parallel's background thread for FrontEnd/Boot/Loading images, so the first display of
// such an image in the menu finds its upscale ready. prewarm_wanted: the class is upscaled and the extent
// is eligible (the factor limit is checked when the prewarm runs).
bool prewarm_wanted(TextureClass, std::uint32_t width, std::uint32_t height) noexcept;
void prewarm_words(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                   TextureClass) noexcept;

// Pure scalers (deterministic; the same input always gives the same output). A1R5G5B5 in: bit 15 alpha,
// 5-bit R, G, B. Out: width*factor x height*factor texels. Alpha stays 1-bit: it is thresholded at 0.5
// after scaling, so alpha-tested draws keep their exact semantics and A2C antialiases the new edge.
// Colour is quantised back to 5 bits with a 4x4 ordered dither for in-between values only; a texel whose
// scaled value is an exact source code keeps that code. factor: 2 or 4.
std::vector<std::uint16_t> upscale_a1r5g5b5(std::span<const std::uint16_t> words, std::uint32_t width,
                                            std::uint32_t height, std::uint32_t factor, Scaler);
// The same scalers with 8-bit output (R, G, B, A bytes per texel, alpha 0 or 255, no dither), for an
// 8-bit texture path.
std::vector<std::uint8_t> upscale_rgba8(std::span<const std::uint16_t> words, std::uint32_t width, std::uint32_t height,
                                        std::uint32_t factor, Scaler);

struct Statistics
{
    std::uint64_t upscaled{}, cache_hits{}, registry_hits{}, computed_texels{};
    std::uint64_t disk_hits{};           // R30
    std::uint64_t compute_nanoseconds{}; // wall time spent in the scalers (cache misses)
    std::size_t cache_bytes{}, registry_entries{};
};
Statistics statistics() noexcept;
void clear_cache() noexcept; // drops cached results; the registry (class decisions) is kept
// Threads a scaler may use (1..8, default 8, never more than the hardware has); 1 = the calling thread
// only. The results do not depend on it. R30: the threads are host_parallel's persistent pool.
void set_worker_threads(unsigned) noexcept;
} // namespace sgn::host_texture_upscale
