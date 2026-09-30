#include "sgn/retail_replay_dynamics.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
void replay_tyre_response(const VehicleParameters &p, VehicleState &s, const VehicleMatrix &m)
{
    const auto forward = s.basis_z, right = s.basis_x;
    const auto velocity = s.velocity;
    // Forward projections remain on the x87 stack until their final dot product.
    const double fx = (double(forward.y) * m[4] + double(forward.z) * m[8]) + double(m[0]) * forward.x;
    const double fy = (double(forward.y) * m[5] + double(forward.z) * m[9]) + double(m[1]) * forward.x;
    const double fz = (double(forward.y) * m[6] + double(forward.z) * m[10]) + double(m[2]) * forward.x;
    const VehicleVector turned{vehicle_f32((double(right.z) * m[8] + double(right.y) * m[4]) + double(m[0]) * right.x),
                               vehicle_f32((double(right.z) * m[9] + double(right.y) * m[5]) + double(m[1]) * right.x),
                               vehicle_f32((double(right.z) * m[10] + double(right.y) * m[6]) + double(m[2]) * right.x),
                               0};
    const float longitudinal =
        vehicle_f32((double(forward.x) * velocity.x + double(velocity.z) * forward.z) + double(velocity.y) * forward.y);
    const float turned_longitudinal = vehicle_f32((fy * velocity.y + fz * velocity.z) + fx * velocity.x);
    const double transfer =
        -((double(longitudinal) * forward.z * turned.z + double(longitudinal) * forward.y * turned.y) +
          double(longitudinal) * forward.x * turned.x);
    const float transfer_x = vehicle_f32(transfer * turned.x), transfer_y = vehicle_f32(transfer * turned.y);
    const float lateral =
        vehicle_f32(-((double(transfer_x) * right.x + double(transfer_y) * right.y) + transfer * turned.z * right.z));
    const float side =
        vehicle_f32((double(right.z) * velocity.z + double(velocity.y) * right.y) + double(velocity.x) * right.x);
    const float front = vehicle_f32((1.0 - p.com_a) * lateral + side),
                rear = vehicle_f32((1.0 - p.com_b) * lateral + side);
    s.wheels[0].lateral_speed = s.wheels[1].lateral_speed = rear;
    s.wheels[2].lateral_speed = s.wheels[3].lateral_speed = front;
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &wheel = s.wheels[i];
        wheel.grip_usage = 0;
        if (!(wheel.load > 8.515625f))
            continue;
        float slip = vehicle_f32(double(wheel.speed) - (i < 2 ? longitudinal : turned_longitudinal));
        tyre_friction(p, s, i, wheel.lateral_speed, slip);
    }
}
void replay_contact_state(VehicleState &s)
{
    for (auto &wheel : s.wheels)
    {
        if (wheel.load > 8.515625f)
        {
            wheel.contact_countdown = 8;
            const auto count = std::bit_cast<std::uint32_t>(s.contact_counter);
            if (count < 8)
                s.contact_counter = std::bit_cast<std::int32_t>(count + 1);
        }
        else if (wheel.contact_countdown != 0)
        {
            wheel.contact_countdown =
                std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(wheel.contact_countdown) - 1);
        }
    }
    classify_contacts(s);
}
void replay_braking_signals(VehicleState &s)
{
    if (s.control_flags[3] != 1)
        return;
    brake_signal(s);
    for (unsigned i = 0; i < 4; ++i)
        wheel_braking_signal(s, i);
}
void align_airborne_vehicle(VehicleState &s, Vec3 gravity)
{
    if (s.contact_counter != 0 || s.stunt.active == 1 || !(s.speed >= 8.333333015441895f))
        return;
    const auto direction = vehicle_normalize({s.velocity.x, s.velocity.y, s.velocity.z, s.velocity_w});
    const VehicleVector g{gravity.x, gravity.y, gravity.z, 0};
    const auto side = vehicle_normalize(vehicle_cross(direction, g));
    const auto up = vehicle_normalize(vehicle_cross(direction, side));
    const auto original = s.orientation;
    const double pitch = -((double(original[10]) * up.z + double(original[9]) * up.y) + double(original[8]) * up.x);
    const float stored_pitch = vehicle_f32(pitch);
    if (std::abs(pitch) > 0.7070000171661377)
        return;
    const double roll = -((double(original[1]) * g.y + double(original[2]) * g.z) + double(g.x) * original[0]);
    const float stored_roll = vehicle_f32(roll);
    if (std::abs(roll) > 0.34202009439468384)
        return;
    auto correction = vehicle_transpose(original);
    const auto angle = [](float value) {
        // Actual CRT 00462950: atan2(value,sqrt((1+value)*(1-value))).
        const double radians = std::atan2(double(value), std::sqrt((1.0 + value) * (1.0 - value)));
        return vehicle_f32((radians * 57.2957763671875f) * -0.10000000149011612f);
    };
    vehicle_rotate(correction, 0, angle(stored_pitch));
    vehicle_rotate(correction, 2, angle(stored_roll));
    vehicle_premultiply(correction, original);
    vehicle_premultiply(s.orientation, correction);
}
} // namespace sgn::retail
