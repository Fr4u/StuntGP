#pragma once
#include <cstdint>
#include <filesystem>

// NEW HOST graphics settings (R22). Not recovered game behaviour: the 2001
// original had no such options. They change only how the host presents the
// unchanged source frames (filtering, anti-aliasing, high-refresh presentation,
// aspect), never a value the simulation or the source frontend reads.
//
// One process-wide snapshot, guarded internally; any thread may read it. A
// consumer applies a change when revision() differs from the revision it last
// applied (for example once per frame), so set() never calls into the renderer.
// The file is a plain UTF-8 "key=value" text next to the game (default_path);
// unknown keys and malformed values are ignored (defaults kept), so the file
// never prevents the game from starting.
namespace sgn::host_graphics
{

enum class TextureFilter : std::uint8_t
{
    original = 0,    // exactly the D3D7 stage filters the game requests
    trilinear = 1,   // requested linear min/mag become linear with linear mips
    anisotropic = 2, // requested linear min/mag become anisotropic (anisotropy)
};
enum class PostAntiAliasing : std::uint8_t
{
    off = 0,
    fxaa = 1
};
enum class DisplayMode : std::uint8_t
{
    windowed = 0,   // resizable window with title bar
    borderless = 1, // "fullscreen windowed": borderless window covering the monitor
    exclusive = 2,  // exclusive fullscreen (DXGI fullscreen state, native mode)
};

struct Settings
{
    bool interpolation{true}; // high-refresh presentation between 60 Hz ticks
    bool vsync{true};
    bool original_4x3{false};      // original 4:3 presentation instead of Hor+ widescreen
    std::uint32_t msaa_samples{4}; // 1, 2, 4 or 8 (the renderer clamps to device support)
    TextureFilter texture_filter{TextureFilter::anisotropic};
    std::uint32_t anisotropy{16}; // 2..16, used by TextureFilter::anisotropic
    bool generate_mipmaps{true};  // mip chains for textures the game uploads without mips
    bool alpha_to_coverage{true}; // smooth alpha-tested edges (foliage, fences) under MSAA
    PostAntiAliasing post_anti_aliasing{PostAntiAliasing::off};
    DisplayMode display_mode{DisplayMode::windowed}; // NEW R24
    // NEW R27 (graphics audit quick wins). "Original" presentation: all four off / 0.
    bool smooth_gradients{true};       // dithered (or 10-bit) output instead of 8-bit truncation
    bool texture_edge_fix{true};       // fill transparent texels' colour at upload (no dark cut-out halos)
    std::uint32_t frame_rate_limit{0}; // 0 = off; otherwise presentation cap in frames per second (30..1000)
    bool low_latency{false};           // frame latency 1 instead of 2 (less input lag, less headroom)
    // NEW R28 (graphics audit items 5-10). "Original" presentation: all off / 0.
    bool high_precision_textures{true}; // 8-bit texture path: decode 16-bit sources without 5-bit banding, 8-bit mips
    bool sharp_hud{true};               // upscaled HUD/font textures at load (sharper 2D at high resolution)
    std::int32_t texture_sharpness{0};  // -3..3, higher = sharper: MipLODBias = -0.25 * value
    bool smooth_magnification{false};   // higher-order filtering when textures are magnified (opt-in)
    bool bloom{false};                  // glow on the game's own additive lights: trails, flares (opt-in)
    bool smooth_effects{true};          // interpolation also moves trails, particles and shadows at display rate
    bool operator==(const Settings &) const = default;
};

// Clamps every field into its documented domain.
Settings sanitize(Settings) noexcept;
Settings current() noexcept;
std::uint64_t revision() noexcept;   // increments on every effective change
void set(const Settings &) noexcept; // sanitizes; bumps revision only if the value changed

std::filesystem::path default_path(const std::filesystem::path &game_root);
// Missing file: defaults stay, returns true. Unreadable file: returns false, defaults stay.
bool load(const std::filesystem::path &) noexcept;
// Writes the current snapshot atomically (temporary file + replace). Returns false on failure.
bool save(const std::filesystem::path &) noexcept;
// Writes the given settings (sanitized) instead of the current snapshot, e.g. the
// user's choices without session-only command-line overrides.
bool save(const std::filesystem::path &, const Settings &) noexcept;
} // namespace sgn::host_graphics
