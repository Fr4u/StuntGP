#include "sgn/vehicle_tick_input.hpp"
#include <bit>
#include <string>

namespace sgn::retail
{
void analog_steering_input(const VehicleParameters &p, VehicleState &s, const VehicleRaceState &race)
{
    // 0040C500..0040C5EC. Source converts with truncation, not nearest rounding.
    steering_boost(s);
    const float boost = vehicle_f32(double(s.steering_boost) + 1.0f);
    const auto input = s.input.left != 0 ? s.input.left : s.input.right;
    const auto negative_index = static_cast<int>((double(input) * 127.0f) * -0.003921568859368563f);
    double amount = double(p.analog_steering.at(static_cast<std::size_t>(-negative_index))) * p.steering_sensitivity;
    if (amount > 1)
        amount = 1;
    if (race.control_kind == 1 || race.control_kind == 3)
    {
        const double scaled = (std::abs(double(s.speed)) * 128.0f) * 0.004999999888241291f;
        if (!std::isfinite(scaled) || scaled >= 2147483648.0)
            throw std::domain_error("analog steering speed index outside finite int32 domain");
        auto index = static_cast<std::uint32_t>(static_cast<std::int32_t>(scaled));
        if (index >= 128)
            index = 127;
        amount = (amount * p.velocity_steering[index]) * boost;
        if (amount > 1)
            amount = 1;
    }
    amount *= p.steering_max;
    if (s.input.right != 0)
        amount = -amount;
    s.steering = vehicle_f32(amount);
}
void analog_vehicle_input(const VehicleParameters &p, VehicleState &s, const VehicleRaceState &race,
                          const VehicleTickInputContext &c)
{
    analog_throttle(s);
    analog_brake(p, s);
    analog_steering_input(p, s, race);
    analog_stunt_input(p, s, c.tick, c.unit_gravity);
    analog_manual_shift(p, s);
}
void mixed_vehicle_input(const VehicleParameters &p, VehicleState &s, const VehicleTickInputContext &c)
{
    analog_throttle(s);
    analog_brake(p, s);
    reversed_digital_steering(p, s);
    analog_stunt_input(p, s, c.tick, c.unit_gravity);
    analog_manual_shift(p, s);
}
void digital_vehicle_input(const VehicleParameters &p, VehicleState &s, const VehicleTickInputContext &c)
{
    digital_manual_shift(p, s);
    digital_throttle(s);
    digital_brake(p, s);
    digital_steering(p, s);
    digital_stunt_input(p, s, c.tick, c.unit_gravity);
}
void update_low_energy_notice(VehicleState &s, VehicleTickState &state, const VehicleTickInputContext &c)
{
    // 0040D6B0..0040D781: special mode skips all writes; timer wraps before DIV60.
    if (s.special_mode == 1)
        return;
    if (s.energy > 10.0f)
    {
        state.low_energy_notice = 0;
        return;
    }
    state.low_energy_phase = (state.low_energy_phase + 1u) % 60u;
    if (state.low_energy_notice != 0)
        return;
    if (c.current >= 6)
        throw std::runtime_error("low energy vehicle identity");
    const auto name = c.vehicle_names[c.current].substr(0, c.vehicle_names[c.current].find('\0'));
    const auto message = c.low_energy_text.substr(0, c.low_energy_text.find('\0'));
    // Literal473A1C is "%s %s\n". A real format overflow is outside the host domain.
    const std::string text = std::string(name) + " " + std::string(message) + "\n";
    if (text.size() >= 64)
        throw FormatError("low energy source notice exceeds 63 bytes");
    contact::player::post_notice(c.controls, c.phase, c.tick, 180, text, c.current, 0, 0);
    ++state.low_energy_notice;
    state.low_energy_phase = 0;
}
void dispatch_vehicle_input(const VehicleParameters &p, VehicleState &s, VehicleRaceState &race,
                            VehicleTickState &state, const VehicleTickInputContext &c)
{
    // 0040D790..0040D84B, all five switch cases and the unsigned default.
    if (race.active != 1)
        return;
    if (race.input_hold != 0)
    {
        --race.input_hold;
        s.rpm = 0;
        digital_throttle(s);
    }
    if (race.control_enabled == 0 || race.input_hold != 0)
    {
        s.input.throttle = 0;
        s.input.brake = 0;
        s.input.shift = 0;
        s.input.stunt = 0;
        s.input.turbo = 0;
        s.input.auxiliary = 0;
        if (s.special_mode == 1)
        {
            s.input.left = 0;
            s.input.right = 0;
        }
    }
    else
        switch (race.control_kind)
        {
        case 0:
            analog_vehicle_input(p, s, race, c);
            break;
        case 1:
        case 3:
            mixed_vehicle_input(p, s, c);
            break;
        case 2:
        case 4:
            digital_vehicle_input(p, s, c);
            break;
        default:
            break;
        }
    update_low_energy_notice(s, state, c);
}
} // namespace sgn::retail
