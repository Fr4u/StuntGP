#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle.hpp"
#include <stdexcept>

namespace sgn::retail
{
namespace
{
CameraVector vector(VehicleVector v) noexcept
{
    return {v.x, v.y, v.z, v.w};
}
} // namespace
void camera_observe_vehicle(CameraVehicle &camera, const VehicleParameters &parameters, const VehicleState &state,
                            const CameraVehicleObservation &race) noexcept
{
    camera.active = race.active;
    camera.progress = race.progress;
    camera.contact_height = race.contact_height;
    camera.position = vector(state.position);
    camera.velocity = {state.velocity.x, state.velocity.y, state.velocity.z, state.velocity_w};
    camera.right = vector(state.basis_x);
    camera.up = {state.up.x, state.up.y, state.up.z, state.basis_y_w};
    camera.forward = vector(state.basis_z);
    camera.orientation = state.orientation;
    camera.speed = state.speed;
    camera.height = parameters.height;
    camera.contact_class = static_cast<std::uint32_t>(state.contact_class);
    camera.contact_counter = static_cast<std::uint32_t>(state.contact_counter);
    camera.shift = vector(state.auxiliary_displacement);
    camera.previous_shift = vector(state.previous_auxiliary_displacement);
}
CameraModeReset camera_vehicle_reset_adapter(CameraEffectTargets &targets)
{
    for (std::size_t slot = 0; slot < targets.active.size(); ++slot)
        if (targets.active[slot] == 1 && !targets.vehicles[slot])
            throw std::invalid_argument("active camera reset slot has no vehicle state");
    return {&targets, targets.active, [](void *context, std::uint32_t slot) noexcept {
                auto &binding = *static_cast<CameraEffectTargets *>(context);
                reset_camera_dependent_effects(*binding.vehicles[slot]);
            }};
}
} // namespace sgn::retail
