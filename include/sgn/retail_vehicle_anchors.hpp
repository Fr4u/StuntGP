#pragma once
#include "sgn/retail_race.hpp"
#include "sgn/vehicle_presentation.hpp"
#include "sgn/vehicle_trails.hpp"

namespace sgn::retail
{
// Additional persistent owners. The enable word3134 stays in presentation,
// while trail positions/enable words stay in VehicleTrailInputs.
struct VehicleAnchorState
{
    VehicleVector position{}, forward{};                                 // 3138,3148
    float extent{};                                                      // 3158
    std::array<std::uint32_t, 2> effect_available{};                     // 328c,33cc
    std::array<VehicleVector, 2> effect_position{}, effect_base_scale{}; // 3290/33d0,32a0/33e0
};
struct VehicleAnchorPart
{
    std::uint16_t flags{};
    VehicleVector position{}, scale{}, forward{};
};
// Required read interface. A live model-resource adapter must borrow its single
// transform/flag owners. Geometry-Z must include all source part vertices,
// including vertices omitted from the renderer's emitted triangles.
struct VehicleAnchorModel
{
    virtual ~VehicleAnchorModel() = default;
    virtual std::uint32_t part_count() const = 0;
    virtual unsigned selected_lod() const = 0;
    virtual std::uint16_t part_flags(unsigned index, unsigned lod) const = 0;
    virtual VehicleAnchorPart part(unsigned index, unsigned lod) const = 0;
    virtual float maximum_vertex_z(unsigned index, unsigned lod) const = 0;
};
// Borrows an unrelocated PMD V1.83. The owner keeps its bytes alive. This
// initializer reads authored transforms before animation changes them.
// Root supplies the source-selected LOD explicitly; pair trails always read0.
class VehicleAnchorModelView final : public VehicleAnchorModel
{
    std::span<const std::uint8_t> bytes_;
    std::size_t descriptor_{}, parts_{}, part_bytes_{}, transforms_{}, transform_bytes_{}, vertices_{}, vertex_bytes_{};
    std::uint32_t part_count_{}, lod_count_{}, selected_lod_{};
    std::array<std::uint32_t, 4> first_parts_{};
    std::size_t part_offset(unsigned index, unsigned lod) const;
    VehicleVector vector(std::size_t offset) const;

  public:
    explicit VehicleAnchorModelView(std::span<const std::uint8_t>, unsigned selected_lod = 0);
    std::uint32_t part_count() const noexcept override
    {
        return part_count_;
    }
    unsigned selected_lod() const noexcept override
    {
        return selected_lod_;
    }
    std::uint16_t part_flags(unsigned index, unsigned lod) const override;
    VehicleAnchorPart part(unsigned index, unsigned lod) const override;
    float maximum_vertex_z(unsigned index, unsigned lod) const override;
};
struct VehicleAnchorVehicle
{
    const VehicleRaceState &race;
    VehiclePresentationState &presentation;
    VehicleTrailInputs &trails;
    VehicleAnchorState &anchors;
    const VehicleAnchorModel *model; // null is valid only on an inactive path
};
struct VehicleAnchorWorld
{
    std::span<VehicleAnchorVehicle, 6> vehicles;
    unsigned &current_slot;
};
void initialize_vehicle_extent_anchors(VehicleAnchorWorld); // full417e10
void initialize_vehicle_effect_anchors(VehicleAnchorWorld); // full4162c0
void initialize_vehicle_pair_a_anchors(VehicleAnchorWorld); // full414f00
void initialize_vehicle_pair_b_anchors(VehicleAnchorWorld); // full4150d0
} // namespace sgn::retail
