#pragma once
#include "sgn/vehicle.hpp"
#include <span>

namespace sgn::retail
{
struct VehicleTrailStorage
{
    std::array<VehicleVector, 64> speed{}; // 351c; its cursor/count already live in camera_effects[2/3]
    std::array<std::array<VehicleVector, 32>, 2> pair_a{}, pair_b{}; // 2d14/2f2c,28d0/2aec
    std::array<std::uint32_t, 2> cursor_a{}, cursor_b{};             // 2f14/312c,2ad0/2cec
};
struct VehicleTrailInputs
{
    std::uint32_t field_28a0{};
    std::array<VehicleVector, 2> pair_a{}, pair_b{}; // 2d04/2f1c,28c0/2adc, borrowed owner snapshots
    std::array<std::uint32_t, 2> pair_b_available{}; // 28bc/2ad8, initialized by 004150d0
};
struct VehicleTrailModel
{
    // Original LOD zero, irrespective of the renderer's selected LOD.
    VehicleVector base_forward{}, base_up{}; // part zero's transform+20/+30
    float part23_position_z{};               // part23+8 -> transform+8
};
struct VehicleTrailProjection
{
    VehicleMatrix world_view{};                         // current source scratch at 61c594
    float projection_x{}, projection_y{};               // current projection 61c3e8/61c3fc
    float center_x{}, center_y{}, scale_x{}, scale_y{}; // 576b94..576ba0
    float clip_near{};                                  // current Controller+198
};
struct VehicleTrailParameters
{
    std::uint32_t capacity{};
    bool project_to_screen{};
};
struct VehicleTrailSettings
{
    VehicleTrailParameters speed, pair_a, pair_b;
};
enum class VehicleTrailOpaqueLane
{
    basis_x,
    basis_y,
    latest,
    cyclic
};
// Read-only diagnostics of executed writes. Never selects or changes a trail.
struct VehicleTrailWriteAudit
{
    void *context{};
    void (*retained_word)(void *, const float &, VehicleTrailOpaqueLane) noexcept {};
};
VehicleTrailSettings initial_vehicle_trail_settings() noexcept; // original 474c08/474d18/474e28
struct VehicleTrailBindings
{
    VehicleState &vehicle;
    const VehicleParameters &parameters;
    VehicleTrailStorage &storage;
    VehicleMatrix &model_basis; // single world owner for source 4d5b30, never per-car reset
    const VehicleTrailInputs &inputs;
    const VehicleTrailModel &model;
    const VehicleTrailProjection &projection;
    const VehicleTrailSettings &settings;
    std::uint32_t source_tick; // 61297c; not monitor refresh/frame time
    VehicleTrailWriteAudit *write_audit{};
};
// Existing owners without an eagerly observed model. Used only by414C60.
struct VehicleSpeedTrailBindings
{
    VehicleState &vehicle;
    const VehicleParameters &parameters;
    VehicleTrailStorage &storage;
    VehicleMatrix &model_basis;
    const VehicleTrailProjection &projection;
    const VehicleTrailSettings &settings;
    std::uint32_t source_tick;
    VehicleTrailWriteAudit *write_audit{};
};
class SourcePmdModelView;
class ModelResources;
void update_vehicle_speed_trail(VehicleTrailBindings); // full 00414c60 + 00414c50
// The tick/speed gates precede registry and raw0 PMD reads, including invalid handles.
void update_vehicle_speed_trail(VehicleSpeedTrailBindings, SourcePmdModelView);
void update_vehicle_speed_trail(VehicleSpeedTrailBindings, ModelResources &, std::size_t handle);
// 414F90/4152D0 owners without an eager model or unused part23 observation.
struct VehiclePairTrailBindings
{
    VehicleState &vehicle;
    VehicleTrailStorage &storage;
    VehicleMatrix &model_basis;
    const VehicleTrailInputs &inputs;
    const VehicleTrailProjection &projection;
    const VehicleTrailSettings &settings;
    std::uint32_t source_tick;
    VehicleTrailWriteAudit *write_audit{};
};
void update_vehicle_pair_a_trails(VehiclePairTrailBindings, SourcePmdModelView);
void update_vehicle_pair_b_trails(VehiclePairTrailBindings, SourcePmdModelView);
void update_vehicle_pair_a_trails(VehiclePairTrailBindings, ModelResources &, std::size_t handle);
void update_vehicle_pair_b_trails(VehiclePairTrailBindings, ModelResources &, std::size_t handle);
void update_vehicle_pair_a_trails(VehicleTrailBindings); // full 00414f90
void update_vehicle_pair_b_trails(VehicleTrailBindings); // full 004152d0
} // namespace sgn::retail
