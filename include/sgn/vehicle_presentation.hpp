#pragma once
#include "sgn/vehicle.hpp"
#include "sgn/vehicle_visual_state.hpp"
#include <functional>
#include <optional>
#include <span>

namespace sgn::retail
{
struct ModelTransform;
struct VehiclePresentationState
{
    // Source roles of these flags are not yet fully established.
    std::uint32_t field_3134{}, field_33c8{}, field_3508{}, field_3924{}, field_3928{};
    std::int32_t idle_hold{}, idle_interval{};    // 7614/7618
    std::array<VehicleVector, 4> wheel_centers{}; // 201c+i*98, shared consumer state
};
struct VehiclePresentationEffects
{
    std::uint32_t field_226c{}, field_328c{}, field_33cc{};
    Vec3 first_scale{}, second_scale{}; // 32b0/33f0, read-only owner snapshot
};
struct VehicleWheelPositionWriteAudit
{
    void *context{};
    void (*retained_word)(void *, unsigned wheel, const float &) noexcept {};
};
struct VehicleEffectPart
{
    std::uint16_t &flags;
    Vec3 &scale; // PMD part+8 -> transform+10, not its position
};
// Borrowed selected-LOD fields; references intentionally preserve transform
// aliasing. The model/resource owner resolves these references before the call.
struct VehiclePresentationParts
{
    std::array<std::reference_wrapper<std::uint16_t>, 6> flags_11_to_16;
    VehicleEffectPart part19, part20;
};
struct VehicleModelStorage
{
    // Root resource-store handles replace original pointers 26b0/26b4.
    std::optional<std::size_t> model, shadow;
    VehicleVector position{}, scale{}, target{}, up{}; // template 26b8..26f7
    std::array<std::uint32_t, 4> template_tail{};      // 26f8..2707; roles not established
    std::uint32_t field_2708{};
    VehicleMatrix body_rotation{}; // 270c; wheel rotations have their own single owner
};
struct ColdPresentationStorage
{
    // Eight original allocation slots, including two without physical vehicles.
    std::span<VehiclePresentationState> presentation;
    std::span<VehicleModelStorage> models;
    std::span<std::array<VehicleWheelAnimation, 4>> wheels;
};
void initialize_cold_presentation(ColdPresentationStorage, std::optional<std::size_t> model,
                                  std::optional<std::size_t> shadow,
                                  const ModelTransform &template_pose); // owned 0040a2f0 writes + full 0040a2c0
void update_vehicle_effect_parts(const VehiclePresentationEffects &, VehicleEffectPart, VehicleEffectPart); // 004166f0
void prepare_vehicle_presentation(VehiclePresentationState &, const VehiclePresentationEffects &,
                                  VehiclePresentationParts); // 00413da0 and full 004166f0
void animate_vehicle_wheels(const VehicleParameters &, const VehicleState &,
                            std::span<VehicleWheelAnimation, 4>); // 00414370
void locate_vehicle_wheels(const VehicleParameters &, const VehicleState &, VehiclePresentationState &,
                           VehicleWheelPositionWriteAudit *audit = nullptr);               // 00414460
void idle_vehicle_suspension(VehicleState &, VehiclePresentationState &, VehicleRandom &); // 004137b0
} // namespace sgn::retail
