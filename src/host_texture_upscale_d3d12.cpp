#include "sgn/host_texture_upscale_d3d12.hpp"
#include "sgn/host_parallel.hpp"
#include "sgn/host_profiler.hpp"

// NEW HOST (R28) sharp HUD, renderer side. See host_texture_upscale.hpp.
namespace sgn::host_texture_upscale
{
namespace
{
// R30: the platform-independent texture code (host_texture_processing, host_texture_upscale) reports its
// profiler scopes through host_parallel's sink; the renderer's upload thread is the game thread.
void forward_profile(const char *stage, std::int64_t nanoseconds)
{
    host_profile::add(stage, nanoseconds);
}
void install_profile_sink() noexcept
{
    static const bool installed = [] {
        if (host_profile::enabled())
            host_parallel::set_profile_sink(&forward_profile);
        return true;
    }();
    (void)installed;
}
} // namespace
PolygonUploadImage prepare_polygon_upload(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                          std::span<const std::uint16_t> words, std::uint32_t factor_limit) noexcept
{
    install_profile_sink();
    PolygonUploadImage literal{width, height, 1, words, {}, {}};
    const auto decision = decide(token, width, height, words, factor_limit);
    if (!decision)
        return literal;
    SGN_PROFILE_SCOPE("host.hud_upscale");
    try
    {
        auto source = std::make_shared<PolygonTexturePixels>();
        source->width = width;
        source->height = height;
        source->words.assign(words.begin(), words.end());
        // R30: where the words came from, as its own stage (memory hit / disk hit / computed).
        const auto start = std::chrono::steady_clock::now();
        UpscaleSource origin{};
        auto storage = upscaled_words(words, width, height, *decision, &origin);
        if (host_profile::enabled())
            host_profile::add(
                origin == UpscaleSource::memory ? "host.hud_upscale_memory_hit"
                : origin == UpscaleSource::disk ? "host.hud_upscale_disk_hit"
                                                : "host.hud_upscale_computed",
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
        const std::span<const std::uint16_t> upscaled(*storage);
        return {width * decision->factor, height * decision->factor, decision->factor, upscaled,
                std::move(storage),       std::move(source)};
    }
    catch (...)
    {
        return literal; // cosmetic feature: never fail an upload over it
    }
}
} // namespace sgn::host_texture_upscale
