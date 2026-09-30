#include "sgn/vehicle_air_tick.hpp"
#include "sgn/contact_takeoff_particles.hpp"
#include <bit>

namespace sgn::retail
{
VehicleAirTickObservation vehicle_air_tick(const VehicleParameters &p, VehicleState &s, VehicleAirTickState &air,
                                           const VehicleAirTickContext &context)
{
    VehicleAirTickObservation observed;
    steer_air_velocity(s, context.unit_gravity);
    if (s.contact_counter != 0)
        s.contact_counter = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(s.contact_counter) - 1);
    accumulate_air_score(s, context.stunt.game_mode);
    update_stunt_landing(s, context.controls, context.vehicle_id, context.score, context.stunt, context.rewards,
                         context.random, context.chain_audio);
    update_stunt_chain_decay(s);
    auto &stunt = s.stunt;
    if (s.contact_counter != 0)
    {
        if (stunt.previous_contact_counter == 0 && s.airborne_ticks > 30)
        {
            air.completed_airtime = s.airborne_ticks;
            stunt.launch_tick = 0;
            begin_stunt_landing(s);
            save_phase_history(s);
            stunt.landing_hold = 60;
            stunt.previous_active = stunt.active;
        }
        stunt.previous_contact_counter = s.contact_counter;
        reset_air_rotation(s);
        return observed;
    }
    if (stunt.previous_contact_counter != 0)
    {
        begin_stunt_height(s);
        reset_phase_history(s);
        reset_launch_state(s);
    }
    else
    {
        update_stunt_height(s, context.controls, context.vehicle_id, context.score, context.stunt,
                            context.rewards.points);
        update_phase_history(s, context.unit_gravity);
    }
    launch_stunt(p, s, context.stunt.tick);
    ++s.airborne_ticks;
    if (s.airborne_ticks < 16)
    {
        vehicle_level_matrix(s.incremental, 0.9f);
        observed.leveled_incremental = true;
    }
    stunt.accumulated_angles.x = vehicle_f32(double(stunt.angular_velocity.x) + stunt.accumulated_angles.x);
    stunt.accumulated_angles.y = vehicle_f32(double(stunt.angular_velocity.y) + stunt.accumulated_angles.y);
    auto rotation = vehicle_transpose(s.orientation);
    vehicle_rotate(rotation, 0, stunt.angular_velocity.x);
    vehicle_premultiply(rotation, s.orientation);
    vehicle_premultiply(s.orientation, rotation);
    rotation = vehicle_transpose(s.orientation);
    vehicle_rotate(rotation, 1, stunt.angular_velocity.y);
    vehicle_premultiply(rotation, s.orientation);
    vehicle_premultiply(s.orientation, rotation);
    vehicle_orthogonalize(s.orientation);
    observed.orthogonalized = true;
    if (stunt.previous_contact_counter != 0)
    {
        contact::effects::emit_takeoff_particles(s, context.random, context.particles, context.template_pose);
        observed.emitted_takeoff = true;
    }
    stunt.previous_contact_counter = s.contact_counter;
    return observed;
}
} // namespace sgn::retail
