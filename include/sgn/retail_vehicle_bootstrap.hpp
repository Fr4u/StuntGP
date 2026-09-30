#pragma once
#include "sgn/retail_vehicle_storage.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/source_camera_spline.hpp"

namespace sgn::retail
{
// Sole00571328 identity, native view OR generation-checked normalized PMD pointer.
// The source producer is4130D0; consumers never fabricate a lookup on a null field.
struct VehicleGridState
{
    CameraSplineBinding start_spline;
};
// Qualification diagnostics only: the original grid copies unwritten stack W
// into three basis columns, and (except selector3) into its position vector.
// Native code retains opaque W; this record never controls physical results.
struct VehicleGridPayloadAudit
{
    std::array<bool, 6> orientation{}, position{};
};
// Full 4130D0 with its fixed bootstrap argument1, including full412BF0 and
// 413490/413530/408640. False means the original missing-spline return ran.
bool initialize_vehicle_starting_grid(VehicleStorage &, VehicleResetWorld &, const SplineSet &, VehicleGridState &,
                                      VehicleGridPayloadAudit *audit = nullptr);
// 413785..413798: four complete anchor initializers in source order. Model
// descriptor5C and raw slot0 are read through their existing owners, not copied.
void initialize_vehicle_model_anchors(VehicleStorage &, ModelResources &, unsigned &current_slot);

struct VehicleBootstrapWorld
{
    VehicleStorage &vehicles;
    const AssetStore &assets;
    ModelResources &models;
    VehicleResetWorld &reset;
    const SplineSet &splines;
    VehicleGridState &grid;
    VehicleStripGeometry &strip;
    VehicleStripBinding &strip_binding;
};
struct VehicleBootstrapParameters
{
    std::span<const LevelCarSlot, 6> cars;
    std::int32_t config_id{}, control_kind{};         // 472C74,60F018
    CameraVector camera_position{}, camera_forward{}; // selected controller +08,+9C
};
// Full413670 caller composition. Prerequisite: real40A2F0 cold initialization,
// model/shadow type4/5 bindings in slot0, and real401DE0 world frame globals.
// VehicleResetWorld must borrow exactly this storage's six physical slots.
// Returns the grid-placement diagnostic; it does not claim a complete race.
bool initialize_vehicle_bootstrap(VehicleBootstrapWorld, const VehicleBootstrapParameters &,
                                  VehicleGridPayloadAudit *audit = nullptr);
} // namespace sgn::retail
