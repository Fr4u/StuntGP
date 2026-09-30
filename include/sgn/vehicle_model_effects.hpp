#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_vehicle_anchors.hpp"
#include "sgn/contact_effects.hpp"
namespace sgn::retail
{
// Additional single per-car owner. Existing physical flags/counters and authored
// anchors stay in their current owners; clear/clone copy this whole value.
struct VehicleModelEffects
{
    std::uint32_t phase{}, enabled{}, cached_brake{}, route_envelope{}; // 225C,226C,28A4,7684
    std::array<std::uint32_t, 2> glow_phases{};                         // sole28B4/28B8, advanced by4154C0
    std::array<VehicleVector, 2> current_scale{};                       // 32B0,33F0; W retained
    std::array<std::array<VehicleVector, 16>, 2> trails{};              // 32C0,3400
    std::array<std::uint32_t, 2> cursor{}; // 33C0,3500; counts stay physics.camera_effects[0/1]
};
struct VehicleRoutePresentation
{
    std::span<const float, 128> lights;           // 60E1F0
    std::span<const std::uint32_t, 128> ambience; // 60E3F0, opaque bits copied to7684
    std::uint32_t ambience_adjacent_word{};       // 60E5F0 exact pinned/owned boundary word
    std::uint32_t point_count{};                  // first source spline descriptor+8
};
struct VehicleModelEffectPart
{
    std::uint16_t &flags;
    ModelTransform &transform;
};
struct VehicleModelEffectResource
{
    virtual ~VehicleModelEffectResource() = default;
    virtual std::uint32_t part_count() const = 0;
    virtual unsigned selected_lod() const = 0;
    virtual VehicleModelEffectPart part(unsigned lod, unsigned index) = 0;
    virtual std::span<std::array<std::uint8_t, 8>> attributes(unsigned lod, unsigned index) = 0;
    virtual std::span<std::array<std::uint32_t, 4>> coordinates(unsigned lod, unsigned index) = 0;
};
// Direct borrowing adapter. Neither this object nor its returned references
// owns a duplicate transform, flag, coordinate or attribute table.
class VehicleModelEffectResourceView final : public VehicleModelEffectResource
{
    ModelResource &model_;
    unsigned lod_;

  public:
    VehicleModelEffectResourceView(ModelResource &model, unsigned lod) : model_(model), lod_(lod) {}
    std::uint32_t part_count() const override
    {
        return static_cast<std::uint32_t>(model_.part_count(model_.source_lod_index(lod_)));
    }
    unsigned selected_lod() const override
    {
        return lod_;
    }
    VehicleModelEffectPart part(unsigned lod, unsigned index) override
    {
        auto p = model_.source_part(model_.source_lod_index(lod), index);
        return {p.flags, p.transform};
    }
    std::span<std::array<std::uint8_t, 8>> attributes(unsigned lod, unsigned index) override
    {
        return model_.source_attributes(model_.source_lod_index(lod), index);
    }
    std::span<std::array<std::uint32_t, 4>> coordinates(unsigned lod, unsigned index) override
    {
        return model_.source_coordinates(model_.source_lod_index(lod), index);
    }
};
void update_vehicle_brake_lights(VehicleState &, const VehicleTrailInputs &, VehicleModelEffects &,
                                 VehicleModelEffectResource &); // 415640
// Checked canonical owner; descriptor5C and raw slot48 are read at this call.
// The legacy ref/span interface remains for stages without a canonical route.
void update_vehicle_brake_lights(VehicleState &, const VehicleTrailInputs &, VehicleModelEffects &,
                                 SourcePmdModelView); // 415640..4156D5
void update_vehicle_brake_lights(VehicleState &, const VehicleTrailInputs &, VehicleModelEffects &, ModelResources &,
                                 std::size_t handle);
void update_vehicle_route_lights(VehicleState &, const VehicleRaceState &, VehicleTrailInputs &, VehicleModelEffects &,
                                 VehicleModelEffectResource &, VehicleRoutePresentation); // 415770+4156E0+415730
// Full415770 with4156E0/415730/414EF0/4152C0; source never reads descriptor5C.
void update_vehicle_route_lights(VehicleState &, const VehicleRaceState &, VehicleTrailInputs &, VehicleModelEffects &,
                                 SourcePmdModelView, VehicleRoutePresentation);
void update_vehicle_route_lights(VehicleState &, const VehicleRaceState &, VehicleTrailInputs &, VehicleModelEffects &,
                                 ModelResources &, std::size_t handle, VehicleRoutePresentation);
void update_vehicle_brake_effects(VehicleState &, VehicleModelEffectResource &); // 414B30
// Full414B30..414C46: three raw slot clears precede current5C; exact aliases/order.
void update_vehicle_brake_effects(VehicleState &, SourcePmdModelView);
void update_vehicle_brake_effects(VehicleState &, ModelResources &, std::size_t handle);
void animate_vehicle_effect_coordinates(VehicleModelEffectResource &, unsigned part); // 4167A0, LODzero
// Optional host diagnostics after completed source-phase operations. No game state.
struct VehicleLaunchAudit
{
    void *context{};
    void (*after)(void *, std::uint32_t instruction, std::uint32_t offset, std::uint32_t value) noexcept {};
};
struct VehicleLaunchBindings
{
    VehicleState &vehicle;
    VehicleMatrix &model_basis;
    const VehicleTrailProjection &projection;
    std::uint32_t source_tick;
    VehicleTrailWriteAudit *write_audit{};
    VehicleLaunchAudit *launch_audit{};
};
void update_vehicle_launch_effect(VehicleLaunchBindings, VehicleModelEffects &, const VehicleAnchorState &,
                                  SourcePmdModelView, contact::effects::ImpactParticles &, VehicleRandom &);
void update_vehicle_launch_effect(VehicleLaunchBindings, VehicleModelEffects &, const VehicleAnchorState &,
                                  ModelResources &, std::size_t handle, contact::effects::ImpactParticles &,
                                  VehicleRandom &);
// Required existing owners. The passed trail binding must refer to the same
// physical vehicle and shared model basis as the model effects call.
void update_vehicle_launch_effect(VehicleTrailBindings, VehicleModelEffects &, const VehicleAnchorState &,
                                  VehicleModelEffectResource &, contact::effects::ImpactParticles &,
                                  VehicleRandom &); // 416800, all callees
VehiclePresentationEffects vehicle_presentation_effects(const VehicleModelEffects &, const VehicleAnchorState &);
} // namespace sgn::retail
