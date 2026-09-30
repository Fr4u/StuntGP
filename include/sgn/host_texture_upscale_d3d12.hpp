#pragma once
#include "sgn/host_texture_upscale.hpp"
#include "sgn/polygon_d3d12.hpp"

// NEW HOST (R28) sharp HUD, renderer side. See host_texture_upscale.hpp.
namespace sgn::host_texture_upscale
{
// The image the renderer uploads for one polygon texture: the literal source (setting off, 3D class,
// failure) or the upscaled 2D texture. `source` is non-null only for an upscaled image: it is the
// recovery payload the uploaded texture must retain instead of the upscaled words, so material
// recovery, extent checks and a device-loss rebind keep working on the source extent and words.
struct PolygonUploadImage
{
    std::uint32_t width{}, height{};
    std::uint32_t factor{1}; // R28b: applied upscale (1 = literal), recorded as PolygonGpuTexture::scale
    std::span<const std::uint16_t> words;
    std::shared_ptr<const std::vector<std::uint16_t>> storage; // owns `words` when upscaled
    std::shared_ptr<const PolygonTexturePixels> source;
};
// Never throws: any failure uploads the literal source. Profiler scope "host.hud_upscale".
// R28b: `factor_limit` is the texture identity's recorded PolygonTextureProfile::upscale.
PolygonUploadImage prepare_polygon_upload(std::uint32_t token, std::uint32_t width, std::uint32_t height,
                                          std::span<const std::uint16_t> words, std::uint32_t factor_limit) noexcept;
// (R28b: the extent checks use sgn::polygon_texture_consistent(), polygon_d3d12.hpp, which knows the
// applied factor of each texture version.)
} // namespace sgn::host_texture_upscale
