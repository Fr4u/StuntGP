#pragma once
#include "sgn/retail_trail_render.hpp"
#include "sgn/vehicle_model_effects.hpp"
#include "sgn/vehicle_model_basis.hpp"
#include <functional>
#include <optional>
#include <variant>

namespace sgn::retail
{
class ModelResources;
// Invocation-only access to the real model. This stores borrowed identities,
// not PMD data or a second LOD owner. Registry lookup occurs only when4155E2
// reaches413B60, after the glow's original early returns and phase writes.
class VehicleTrailBasisSource
{
    struct Registry
    {
        ModelResources *models;
        const std::optional<std::size_t> *handle;
    };
    std::variant<std::reference_wrapper<const VehicleTrailModel>, Registry> value_;

  public:
    VehicleTrailBasisSource(const VehicleTrailModel &value) : value_(std::cref(value)) {}
    VehicleTrailBasisSource(ModelResources &models, const std::optional<std::size_t> &handle)
        : value_(Registry{&models, &handle})
    {
    }
    VehicleTrailModel read() const;
};
// Test-only incoming provenance, supplied by the actual ring writers.
struct VehicleTrailRingAudit
{
    std::array<bool, 64> speed{};
    std::array<std::array<bool, 32>, 2> pair_a{}, pair_b{};
    std::array<std::array<bool, 16>, 2> effect{};
};
struct VehicleTrailRenderBindings
{
    TrailRenderBindings render;
    const VehicleState &vehicle;
    const VehicleTrailStorage &trails;
    const VehicleTrailInputs &inputs;
    const VehicleModelEffects &effects;
    const VehicleAnchorState &anchors;
    const VehiclePresentationState &presentation;
    VehicleTrailBasisSource model;
    VehicleMatrix &model_basis;           // sole4D5B30
    std::array<std::uint32_t, 2> &phases; // borrowed28B4/28B8; same per-car owner as other effects
    std::uint32_t source_tick, current_vehicle_slot, camera_vehicle_slot;
    const VehicleTrailRingAudit *ring_audit{};
    VehicleTrailWriteAudit *basis_audit{};
};
void render_vehicle_speed_trail(VehicleTrailRenderBindings);   // 414CF0
void render_vehicle_effect_trails(VehicleTrailRenderBindings); // 414E50
void render_vehicle_pair_a_trails(VehicleTrailRenderBindings); // 415040
void render_vehicle_pair_b_trails(VehicleTrailRenderBindings); // 4153A0
void render_vehicle_pair_b_glows(VehicleTrailRenderBindings);  // 4154C0+413B60+415190+434590
// Complete415190, retaining destinationW. It uses the supplied shared model
// basis and current physical vehicle, not a cached projected trail point.
void transform_vehicle_pair_b_anchor(const VehicleState &, const VehicleMatrix &, VehicleVector, CameraVector &);
} // namespace sgn::retail
