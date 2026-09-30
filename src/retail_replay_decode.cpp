#include "sgn/retail_replay_decode.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
std::int32_t position(std::span<const std::uint8_t, 44> frame, std::size_t at)
{
    const auto bits = std::uint32_t(frame[at]) | (std::uint32_t(frame[at + 1]) << 8) |
                      (std::uint32_t(frame[at + 2]) << 16) | (std::uint32_t(frame[at + 3]) << 24);
    return std::bit_cast<std::int32_t>(bits);
}
std::int32_t angle(std::span<const std::uint8_t, 44> frame, std::size_t at)
{
    const auto bits = static_cast<std::uint16_t>(std::uint16_t(frame[at]) | (std::uint16_t(frame[at + 1]) << 8));
    return std::bit_cast<std::int16_t>(bits);
}
double direction(std::span<const std::uint8_t, 44> frame, std::size_t at)
{
    // Original float [0046e5dc], including its exact stored approximation.
    return double(std::bit_cast<std::int8_t>(frame[at])) * std::bit_cast<float>(std::uint32_t{0x3c010204});
}
float stored_interpolation(float a, float b, float phase)
{
    const float difference = vehicle_f32(double(b) - a);
    const float displacement = vehicle_f32(double(difference) * phase);
    return vehicle_f32(double(displacement) + a);
}
void valid_phase(float phase)
{
    if (!std::isfinite(phase))
        throw std::domain_error("nonfinite replay phase outside recovered domain");
}
} // namespace
void decode_replay_position(const ReplayFramePair &frames, VehicleState &state)
{
    valid_phase(frames.phase);
    // 00419eee..00419f89: X stays on the x87 stack; Y/Z pass through float stores.
    const double ax = position(frames.current, 0), bx = position(frames.next, 0);
    const float x = vehicle_f32((bx - ax) * frames.phase + ax);
    const float y = stored_interpolation(vehicle_f32(position(frames.current, 4)),
                                         vehicle_f32(position(frames.next, 4)), frames.phase);
    const float z = stored_interpolation(vehicle_f32(position(frames.current, 8)),
                                         vehicle_f32(position(frames.next, 8)), frames.phase);
    state.previous_velocity = state.velocity;
    state.previous_velocity_w = state.velocity_w;
    state.velocity.x = vehicle_f32(double(x) - state.position.x);
    state.velocity.y = vehicle_f32(double(y) - state.position.y);
    const double vz = double(z) - state.position.z;
    state.velocity.z = vehicle_f32(vz);
    // 00419fa5 is FST, not FSTP: speed consumes the unrounded Z difference.
    state.speed = vehicle_f32((vz * state.basis_z.z + double(state.basis_z.y) * state.velocity.y) +
                              double(state.basis_z.x) * state.velocity.x);
    state.position = {x, y, z, state.position.w};
    state.previous_position = {x, y, z, state.previous_position.w};
}
void decode_replay_basis(const ReplayFramePair &frames, VehicleState &state)
{
    valid_phase(frames.phase);
    const double ax = direction(frames.current, 41), bx = direction(frames.next, 41);
    VehicleVector up{vehicle_f32((bx - ax) * frames.phase + ax),
                     stored_interpolation(vehicle_f32(direction(frames.current, 42)),
                                          vehicle_f32(direction(frames.next, 42)), frames.phase),
                     stored_interpolation(vehicle_f32(direction(frames.current, 43)),
                                          vehicle_f32(direction(frames.next, 43)), frames.phase),
                     0};
    const float fx = vehicle_f32(direction(frames.current, 38)), nx = vehicle_f32(direction(frames.next, 38));
    VehicleVector forward{vehicle_f32((double(nx) - fx) * frames.phase + fx),
                          stored_interpolation(vehicle_f32(direction(frames.current, 39)),
                                               vehicle_f32(direction(frames.next, 39)), frames.phase),
                          stored_interpolation(vehicle_f32(direction(frames.current, 40)),
                                               vehicle_f32(direction(frames.next, 40)), frames.phase),
                          0};
    up = vehicle_normalize(up);
    forward = vehicle_normalize(forward);
    const auto right = vehicle_normalize(vehicle_cross(forward, up)); // 0041a18a..0041a1c5
    state.basis_x = {right.x, right.y, right.z, state.basis_x.w};
    state.up = {up.x, up.y, up.z};
    state.basis_z = {forward.x, forward.y, forward.z, state.basis_z.w};
}
void decode_replay_wheels(const ReplayFramePair &frames, VehicleState &state, const VehicleParameters &parameters,
                          std::span<VehicleWheelAnimation, 4> visuals)
{
    valid_phase(frames.phase);
    for (std::size_t i = 0; i < 4; ++i)
    {
        const auto a = angle(frames.current, 12 + i * 2), b = angle(frames.next, 12 + i * 2);
        const double interpolated = double(b - a) * frames.phase + a;
        const double difference = interpolated - visuals[i].current_angle;
        visuals[i].current_angle = vehicle_f32(interpolated);
        // 0041a285 stores delta but tests the still-unrounded x87 value.
        const float delta = vehicle_f32(difference);
        const double wrapped = difference < 0 ? -std::fmod(-double(delta), 360.0) : std::fmod(double(delta), 360.0);
        const double speed = (wrapped * parameters.tyres[i].radius) * std::bit_cast<float>(std::uint32_t{0xbc8efa36});
        state.wheels[i].acceleration = vehicle_f32((speed - state.wheels[i].speed) * 1.5);
        state.wheels[i].speed = vehicle_f32(speed);
        visuals[i].rotation = vehicle_rotation(0, visuals[i].current_angle);
    }
}
} // namespace sgn::retail
