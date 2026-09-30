#include "sgn/vehicle_restart.hpp"

namespace sgn::retail
{
void restart_vehicle(VehicleResetWorld &world, unsigned target, VehicleRestartState &restart)
{
    if (target >= world.vehicles.size() || world.current >= world.vehicles.size())
        throw std::out_of_range("restart vehicle slot");
    auto &car = world.vehicles[target];
    auto &s = car.physics;
    stop_all_wheels(s); // 00413470
    s.total_speed = 0;
    s.speed = 0;
    s.previous_velocity = {};
    s.velocity = {};
    s.acceleration = {};
    s.incremental = vehicle_identity();
    s.secondary_rotation = vehicle_identity();
    s.throttle = 0;
    s.rpm = 0;
    s.turbo_active = 0;
    s.turbo_rpm = 0;
    s.turbo_force = 0;
    s.input = {};
    restart.previous_input = {};
    s.previous_shift = 0;
    s.previous_stunt = 0;
    s.steering_ramp = 0;
    s.brake_ramp = 0;
    s.control_flags[0] = 0;
    s.control_flags[2] = 0;
    s.control_flags[3] = 0;
    s.upgrade_active = 0;
    car.race.route_indices[0] = 0;
    car.race.route_fractions[0] = 0;
    car.race.route_indices[1] = 0;
    car.race.route_fractions[1] = 0;
    restart.mode_2630 = 1;
    s.performance_multiplier = 1;
    restart.mode_2620 = 1;
    car.contact.com_reset_ticks = 0;
    car.contact.com_2628 = 0;
    car.contact.com_262c = 0;
    reset_camera_dependent_effects(s); // 00414D80,00414C50,00414EF0,004152C0 in order
    end_vehicle_special_mode(world, target);
}
} // namespace sgn::retail
