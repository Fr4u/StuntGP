#pragma once
#include "sgn/assets.hpp"
#include "sgn/retail_camera.hpp"
#include <optional>

namespace sgn::retail
{
struct TextureMotion
{
    std::uint32_t kind{};
    float u{}, v{}, step_u{}, step_v{};
};
static_assert(sizeof(TextureMotion) == 20);
// A loaded PMD animation section, not an optional empty runtime substitute.
// Geometry/texture indices address these records in their original order.
class TextureAnimationBank
{
    std::vector<TextureMotion> records_;
    std::size_t source_offset_{};

  public:
    explicit TextureAnimationBank(std::span<const std::uint8_t> track_pmd); // 42EF50 header5C, bounded sentinel parser
    std::span<TextureMotion> records() noexcept
    {
        return records_;
    }
    std::span<const TextureMotion> records() const noexcept
    {
        return records_;
    }
    std::size_t source_offset() const noexcept
    {
        return source_offset_;
    }
};
struct SkyTextureMotion
{
    std::uint32_t marker{};
    float u{}, v{}, step_u{}, step_v{};
};
static_assert(sizeof(SkyTextureMotion) == 20);
struct TextureAnimationState
{
    float oscillation{};                   // 577650
    std::uint32_t phase{};                 // 577654
    std::array<SkyTextureMotion, 2> sky{}; // 577660..577687;577688 belongs to LevelCurveTables
};
struct WorldViewQuality
{
    std::uint32_t first{}, second{};           // 624980/984
    std::uint32_t legacy_driver_restriction{}; // 589E08;42E6D0 initializes0
};
struct ViewMatrixArena
{
    static constexpr std::uint32_t capacity = 384;
    std::array<CameraMatrix, capacity> matrices{}; // 61C7CC..6227CB, retained by44FD60
    std::optional<std::uint32_t> cursor;           // 6227CC; byte offset, null represents source null
    std::uint32_t count{};                         // 6227D0; no derived count/cache
};
struct WorldViewPrefix
{
    WorldViewQuality &quality;
    const std::uint32_t &rate_selector; // borrow existing ImpactParticles.rate_selector61297C
    ViewMatrixArena &matrices;
    TextureAnimationBank &textures;
    TextureAnimationState &animation;
    const std::uint32_t &frame_pause; // 61294C, distinct from audio pause4D5B20
};
void reset_view_matrix_cursor(ViewMatrixArena &) noexcept; // full44FD60, no clearing/allocating matrices
void update_world_view_quality(WorldViewQuality &, std::uint32_t rate_selector) noexcept; // full42E820
void initialize_legacy_driver_restriction(WorldViewQuality &) noexcept;    // full42E6D0, device detection separate
void clear_sky_texture_motion(std::array<SkyTextureMotion, 2> &) noexcept; // full425880
// The animation side effects of42C740 after its real L_SKY name was parsed.
// An absent sky key preserves all ten words; matching is source case-sensitive strstr.
void initialize_sky_texture_motion(std::array<SkyTextureMotion, 2> &, std::optional<std::string_view> loaded_sky_name);
void advance_texture_animation(TextureAnimationBank &, TextureAnimationState &,
                               std::uint32_t frame_pause); // full4258A0/425AD0
// Actual ordered42E870 prefix after the caller's viewport selector gate.
void prepare_world_view_prefix(WorldViewPrefix);
} // namespace sgn::retail
