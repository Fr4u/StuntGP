#pragma once
#include "sgn/vehicle_math.hpp"
#include "sgn/source_world_span.hpp"
#include <optional>
namespace sgn::retail
{
// The three words reset by41D170. Their later update roles are not yet named.
// These are headers, not representations of the entire old D10-byte records.
struct VehicleStripHeader
{
    std::uint32_t field_00{}, field_04{}, field_08{};
};
struct VehicleStripHeaders
{
    std::array<VehicleStripHeader, 4> items{};
}; // 3964+i*d10
void clear_vehicle_strip_headers(VehicleStripHeaders &) noexcept; // 41D180's four41D170 calls
struct VehicleStripCorner
{
    float u{}, v{};
    std::uint16_t vertex{}, field_0a{};
    std::uint32_t color{};
};
struct VehicleStripPolygon
{
    std::uint8_t vertex_count{}, texture{}, field_02{}, flags{};
    std::uint16_t normal{}, first_corner{};
};
struct ModelTransform;
struct VehicleStripTransform
{
    VehicleVector position, scale, forward, up;
    std::array<std::uint32_t, 4> tail;
};
static_assert(sizeof(VehicleStripTransform) == 80);
// Initial PE value only. The mutable474820 owner is geometry.part_transform.
const VehicleStripTransform &vehicle_strip_transform() noexcept;
// One world owner for dynamically generated geometry, distinct from PMD
// resources and all per-car trail buffers. No source addresses are retained.
struct VehicleStripGeometry
{
    // Actual six allocations571300..571314, never mirrored into native arrays.
    // Bindings own no payload and retain null/expired identity until reached.
    struct SourceBuffers
    {
        SourceWorldSpan<std::uint8_t> polygons, indices, vertices, coordinates, part, descriptor;
    } source;
    std::array<VehicleStripPolygon, 128> polygons{};
    std::array<std::uint16_t, 512> corner_indices{};
    std::array<VehicleVector, 512> vertices{};
    std::array<VehicleStripCorner, 512> corners{};
    std::array<VehicleVector, 128> normals{};     // 570ad8; first W survives initialization
    std::array<std::uint32_t, 3> texture_names{}; // 5712e0; source null bindings
    std::uint32_t retained_bound_tail{};          // 5712EC, retained fourth bound-record word
    std::optional<std::uint32_t> bound_index;     // 5712D8, PE-null;41C651 binds index0 to5712E0
    std::uint32_t texture_id{};                   // 5712fc, preserved
    std::uint32_t field_5712f0{}, field_5712f4{}, field_5712f8{};
    std::uint16_t part_flags{}, model_field20{}, model_field22{}, part_field20{}, part_field22{};
    std::uint16_t polygon_count{}, corner_count{}, vertex_count{};
    VehicleStripTransform part_transform{vehicle_strip_transform()}; // sole mutable474820
    const ModelTransform *outer_transform{};                         // descriptor0C, borrowed by42AF30
    std::uint16_t model_flags_02{1}, part_flags_02{};
};
// Mandatory resource publication. The owner binds this geometry to resource
// type1 (636428). Implementations must retain a live reference, not a snapshot.
struct VehicleStripBinding
{
    virtual ~VehicleStripBinding() = default;
    virtual void bind(VehicleStripGeometry &) = 0;
};
void initialize_strip_geometry(VehicleStripGeometry &,
                               VehicleStripBinding &);                     // full41C530, portable allocation ownership
void expand_strip_geometry(VehicleStripGeometry &, VehicleStripBinding &); // full41C9B0 incl41C530
void initialize_vehicle_strip(VehicleStripGeometry &, VehicleStripHeaders &, VehicleStripBinding &); // full41D180
} // namespace sgn::retail
