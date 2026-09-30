#pragma once
#include "sgn/retail_world_polygons.hpp"
#include "sgn/retail_world_view_prefix.hpp"

namespace sgn::retail
{
// Exact FVF2C4 object representation: XYZRHW, diffuse, specular, two UV pairs.
struct PolygonSubmissionVertex
{
    std::array<std::uint32_t, 10> words{};
};
static_assert(sizeof(PolygonSubmissionVertex) == 40);
// Tokens represent source texture-interface identity, never host pointers or
// local PMD indices. Alternate is record0+4 when used by433C50, not table[1].
struct PolygonSubmissionMaterial
{
    std::uint32_t primary_texture{}, alternate_texture{}, motion_index{};
};
struct PolygonSubmissionInputs
{
    std::span<const PolygonSubmissionMaterial> materials;
    std::span<const TextureMotion> motions;
    std::span<const SkyTextureMotion, 2> sky;
    const std::uint32_t &sky_material_base;
    const std::uint32_t &depth_capability;          // 5D91B4->64
    const std::uint32_t &quality;                   // 624984, live at each source read
    const std::uint32_t &legacy_driver_restriction; // 589E08
    const std::uint32_t &multitexture_capability;   // 592074
    const ViewportState &viewport;                  // metadata only; vertices already projected
};
// Single persistent owner, configured by the platform before use. Capacity is
// a host safety bound, not an inferred retail allocation size. No per-call
// clearing: untouched words/indices survive material changes and later frames.
struct PolygonSubmissionState
{
    std::vector<PolygonSubmissionVertex> indexed_vertices, alpha_vertices;
    std::vector<std::uint16_t> indices;
    std::uint32_t depth_scale_bits{0x43000000u};                 // 477CCC: pinned PE binary32 128, retained by42F300
    std::uint32_t specular_enabled{};                            // 592050
    std::uint32_t depth_enabled{}, depth_write{}, depth_other{}; // 59205C/60/64
};
// Required external API boundary. Draw spans borrow scratch only during the
// call; a deferred GPU/presentation consumer must own its copies. HRESULT bits
// are returned literally. Only433290 checks its initial ZFUNC return; that
// legacy error/termination path is rejected explicitly by this bounded module.
struct PolygonSubmissionSink
{
    virtual ~PolygonSubmissionSink() = default;
    virtual std::uint32_t set_render_state(std::uint32_t state, std::uint32_t value) = 0;
    virtual std::uint32_t set_texture(std::uint32_t stage, std::uint32_t token) = 0;
    virtual std::uint32_t set_texture_stage_state(std::uint32_t stage, std::uint32_t state, std::uint32_t value) = 0;
    virtual std::uint32_t draw_indexed(std::uint32_t type, std::uint32_t fvf, std::span<const PolygonSubmissionVertex>,
                                       std::span<const std::uint16_t>, std::uint32_t flags, const ViewportState &) = 0;
    virtual std::uint32_t draw(std::uint32_t type, std::uint32_t fvf, std::span<const PolygonSubmissionVertex>,
                               std::uint32_t flags, const ViewportState &) = 0;
};
enum class PolygonSubmissionRoute : std::uint32_t
{
    sky,
    opaque,
    specular,
    multitexture,
    alpha
};
// Full original named route, with its source helper state calls. Valid input
// domain: nonnull headers have at least3 corners; alpha has at most15; all
// referenced native owners and scratch spans must exist. Invalid host inputs
// throw FormatError instead of reproducing out-of-bounds guest memory access.
void submit_polygon_route(PolygonSubmissionRoute, std::span<const WorldPolygonPacket>, const PolygonSubmissionInputs &,
                          PolygonSubmissionState &, PolygonSubmissionSink &);
struct PolygonSubmissionLists
{
    // 6243E0,624400,624410,624420,624430,624440,624450
    std::array<std::span<const WorldPolygonPacket>, 7> categories;
    // Whole lists61C600/6243CC and61C5F8/6243DC, in that order.
    std::array<std::span<const WorldPolygonPacket>, 2> alpha;
};
void submit_world_polygons(const PolygonSubmissionLists &, const PolygonSubmissionInputs &, PolygonSubmissionState &,
                           PolygonSubmissionSink &); // full433F40
} // namespace sgn::retail
