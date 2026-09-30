#include "sgn/vehicle_dynamics_tick.hpp"
#include "sgn/retail_replay_dynamics.hpp"

namespace sgn::retail
{
VehicleAirTickObservation vehicle_dynamics_tick(VehicleDynamicsVehicle vehicle,
                                                const VehicleDynamicsTickContext &context)
{
    if (context.current >= context.world.size() || &context.world[context.current].physics != &vehicle.physics)
        throw std::domain_error("dynamics current vehicle binding differs from world slot");
    if (context.dynamics.tick != context.air.stunt.tick ||
        context.dynamics.engine.game_mode != context.air.stunt.game_mode)
        throw std::domain_error("dynamics and airborne globals must share the same retail tick/stage");
    if (context.dynamics.special_drag != 0)
        throw std::domain_error("full dynamics requires the source upgrade drag multiplier");
    std::optional<double> drag;
    if (vehicle.physics.upgrade_active == 1)
        drag = (1.0 - double(vehicle.physics.upgrade_condition)) * context.upgrade_drag_multiplier;
    // The engine prefix does not write 2640 or 2660. This side-effect-free
    // retained product is consumed at 0040F4E9 after the base force store.
    vehicle_dynamics_core(vehicle.parameters, vehicle.physics, vehicle.support, context.dynamics, context.air.random,
                          context.workspace, drag);
    const auto observed = vehicle_air_tick(vehicle.parameters, vehicle.physics, vehicle.air, context.air);
    align_airborne_vehicle(vehicle.physics, context.air.unit_gravity);
    update_vehicle_position_history(context.world, context.current, vehicle.history, context.air.stunt.race_mode);
    average_grip(vehicle.physics);
    return observed;
}
} // namespace sgn::retail
