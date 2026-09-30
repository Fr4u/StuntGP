#include "sgn/retail_replay_controls.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
int signed_byte(std::uint8_t value)
{
    return std::bit_cast<std::int8_t>(value);
}
std::int16_t signed_word(std::span<const std::uint8_t, 44> bytes, unsigned at)
{
    std::int16_t value;
    std::memcpy(&value, bytes.data() + at, 2);
    return value;
}
std::uint16_t unsigned_word(std::span<const std::uint8_t, 44> bytes, unsigned at)
{
    std::uint16_t value;
    std::memcpy(&value, bytes.data() + at, 2);
    return value;
}
} // namespace
void decode_replay_steering(const ReplayFramePair &frames, VehicleState &s)
{
    const int previous = signed_byte(frames.current[0x22]), next = signed_byte(frames.next[0x22]);
    s.steering = vehicle_f32(double(next - previous) * frames.phase + previous);
}
void decode_replay_suspension(const ReplayFramePair &frames, VehicleState &s)
{
    for (unsigned j = 0; j < 4; ++j)
    {
        const double previous = double(signed_byte(frames.current[0x1c + j])) * 0.5;
        const double next = double(signed_byte(frames.next[0x1c + j])) * 0.5;
        s.suspension[j] = vehicle_f32((next - previous) * frames.phase + previous);
    }
}
void decode_replay_loads(const ReplayFramePair &frames, VehicleState &s, contact::solver::State &contact, float gravity)
{
    for (unsigned j = 0; j < 4; ++j)
    {
        const double previous = double(signed_word(frames.current, 0x14 + j * 2)) * 0.5;
        const double next = double(signed_word(frames.next, 0x14 + j * 2)) * 0.5;
        const float value = vehicle_f32((next - previous) * frames.phase + previous);
        s.wheels[j].load = value;
        contact.wheels[j].current_load = value;
        if ((frames.next[0x24] & 0x20) != 0)
            contact.wheels[j].current_load = vehicle_f32((double(gravity) * s.effective_mass) * 3.5f);
    }
}
void decode_replay_turbo(const ReplayFramePair &frames, VehicleState &s)
{
    s.turbo_active = (frames.next[0x24] & 2) ? 1 : 0;
}
void decode_replay_gear(const ReplayFramePair &frames, VehicleState &s)
{
    const int gear = signed_byte(frames.next[0x23]);
    if (gear != s.gear)
    {
        s.gear_changed = 1;
        s.clutch = 0;
    } // complete 0040C160
    s.gear = gear;
}
void decode_replay_auxiliary(const ReplayFramePair &frames, VehicleState &s)
{
    s.input.auxiliary = (frames.next[0x24] & 0x10) ? 1 : 0;
}
void decode_replay_brake(const ReplayFramePair &frames, VehicleState &s)
{
    s.control_flags[1] = s.control_flags[3];
    s.control_flags[3] = (frames.next[0x24] >> 3) & 1;
}
void decode_replay_rpm(const ReplayFramePair &frames, VehicleState &s)
{
    const int previous = unsigned_word(frames.current, 0x20), next = unsigned_word(frames.next, 0x20);
    s.rpm = vehicle_f32(double(next - previous) * frames.phase + previous);
}
void decode_replay_throttle(const ReplayFramePair &frames, VehicleState &s)
{
    s.control_flags[0] = s.control_flags[2];
    s.control_flags[2] = (frames.next[0x24] >> 2) & 1;
    if (s.control_flags[2] == 1)
    {
        const double value = double(s.throttle) + 0.15f;
        s.throttle = vehicle_f32(value);
        if (value > 1)
            s.throttle = 1;
    }
    else if (s.contact_counter != 0)
        s.throttle = vehicle_f32(double(s.throttle) * 0.5);
}
void decode_replay_shock(const ReplayFramePair &frames, contact::solver::State &contact)
{
    const auto value = frames.next[0x25];
    if (value == 0)
        return;
    if ((value & 0x80) != 0)
    {
        contact.pair_shock = static_cast<float>(value & 0x7f);
        contact.effect_2368 = 16;
        return;
    }
    contact.shock = static_cast<float>(value);
    contact.shock_hold = 16;
}
void decode_replay_surface(const ReplayFramePair &frames, contact::solver::State &contact)
{
    const auto kind = frames.next[0x24] & 0xc0;
    const int type = kind == 0x40 ? 2 : (kind == 0x80 ? 3 : (kind == 0xc0 ? 4 : 1));
    for (auto &wheel : contact.wheels)
    {
        wheel.previous_type = wheel.type;
        wheel.type = type;
    }
}
} // namespace sgn::retail
