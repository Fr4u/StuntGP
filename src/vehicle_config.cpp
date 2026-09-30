#include "sgn/vehicle.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>

namespace sgn::retail
{
namespace
{
// New bounds-checked reader for the original named text fields. The original
// loaders 0040b490..0040bb60 are the field/schema authority, not editor labels.
struct Section
{
    std::string_view text;
    std::string_view value(std::string_view key) const
    {
        std::size_t line = 0;
        std::string_view found;
        unsigned matches = 0;
        while (line < text.size())
        {
            auto end = text.find('\n', line);
            if (end == text.npos)
                end = text.size();
            auto row = text.substr(line, end - line);
            auto start = row.find_first_not_of(" \t\r");
            if (start != row.npos)
            {
                row.remove_prefix(start);
                if (row.starts_with(key))
                {
                    auto rest = row.substr(key.size());
                    auto equal = rest.find_first_not_of(" \t");
                    if (equal != rest.npos && rest[equal] == '=')
                    {
                        found = text.substr(line + start + key.size() + equal + 1);
                        ++matches;
                    }
                }
            }
            line = end + 1;
        }
        if (matches != 1)
            throw FormatError("missing/duplicate vehicle field: " + std::string(key));
        return found;
    }
    static double number(std::string_view &text)
    {
        auto start = text.find_first_not_of(" \r\n\t,");
        if (start == text.npos)
            throw FormatError("missing vehicle number");
        text.remove_prefix(start);
        double value{};
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
        if (error != std::errc{} || !std::isfinite(value))
            throw FormatError("invalid vehicle number");
        if (end != text.data() + text.size() && *end != ' ' && *end != '\r' && *end != '\n' && *end != '\t' &&
            *end != ',')
            throw FormatError("invalid vehicle number suffix");
        text.remove_prefix(static_cast<std::size_t>(end - text.data()));
        return value;
    }
    float scalar(std::string_view key) const
    {
        auto source = value(key);
        auto x = static_cast<float>(number(source));
        if (!std::isfinite(x))
            throw FormatError("vehicle number outside float range");
        return x;
    }
    std::int32_t integer(std::string_view key) const
    {
        auto source = value(key);
        auto x = number(source);
        if (x < -2147483648.0 || x > 2147483647.0 || std::trunc(x) != x)
            throw FormatError("invalid vehicle integer");
        return static_cast<std::int32_t>(x);
    }
    template <std::size_t N> std::array<float, N> array(std::string_view key) const
    {
        std::array<float, N> out{};
        auto source = value(key);
        for (auto &x : out)
        {
            x = static_cast<float>(number(source));
            if (!std::isfinite(x))
                throw FormatError("vehicle curve outside float range");
        }
        auto next = source.find_first_not_of(" \r\n\t,");
        if (next != source.npos)
        {
            // A section marker starts with '-', so inspect the following byte
            // before treating a sign as the start of an additional sample.
            if (source[next] == '+' || source[next] == '-')
                ++next;
            if (next < source.size() && ((source[next] >= '0' && source[next] <= '9') || source[next] == '.'))
                throw FormatError("too many vehicle curve samples");
        }
        return out;
    }
};
Section section(std::string_view text, std::string_view name, std::string_view end_name = {})
{
    std::string marker = "-" + std::string(name);
    auto start = text.find(marker);
    if (start == text.npos)
        throw FormatError("missing vehicle section: " + std::string(name));
    if (text.find(marker, start + marker.size()) != text.npos)
        throw FormatError("duplicate vehicle section");
    if (end_name.empty())
    {
        auto end = text.find("-" + std::string(name.substr(0, name.find('['))) + "_END", start + marker.size());
        if (end == text.npos)
            throw FormatError("unterminated vehicle section");
        return {text.substr(start, end - start)};
    }
    auto end = text.find(end_name, start + marker.size());
    if (end == text.npos)
        throw FormatError("unterminated tyre section");
    return {text.substr(start, end - start)};
}
void positive(float x, const char *name)
{
    if (!(x > 0))
        throw FormatError(std::string("nonpositive vehicle parameter: ") + name);
}
} // namespace
VehicleParameters decode_vehicle_parameters(std::string_view text)
{
    if (text.empty() || text.size() > 65536 || text.find('\0') != text.npos)
        throw FormatError("invalid vehicle config size/text");
    VehicleParameters p;
    const auto d = section(text, "DIMENSIONS[]"), e = section(text, "ENGINE[]"), g = section(text, "GEARBOX[]"),
               b = section(text, "BRAKE[]"), t = section(text, "TYRES[]"), a = section(text, "AERODYNAMICS[]"),
               s = section(text, "STEERING[]"), c = section(text, "CONTROL[]"), f = section(text, "FUEL[]"),
               u = section(text, "TURBO[]");
    p.mass = d.scalar("Mass");
    p.com_a = d.scalar("Com_a");
    p.com_b = d.scalar("Com_b");
    p.com_height = d.scalar("Com_h");
    p.wheelbase = d.scalar("WheelBase");
    p.track_front = d.scalar("TrackFront");
    p.track_rear = d.scalar("TrackRear");
    p.length = d.scalar("Length");
    p.width = d.scalar("Width");
    p.height = d.scalar("Height");
    p.ride_height = d.scalar("RideHeight");
    p.rpm_idle = e.scalar("RpmIdle");
    p.rpm_limit = e.scalar("RpmLimit");
    p.rpm_max = e.scalar("RpmMax");
    p.engine_decay = e.scalar("Cm");
    p.max_torque = e.scalar("MaxTorque");
    for (unsigned j = 0; j < 4; ++j)
        p.torque_curves[j] = e.array<128>("TorqueEnv[" + std::to_string(j) + "]");
    p.drive_mode = g.integer("DriveMode");
    p.gear_count = g.integer("NrOfGears");
    p.final_drive = g.scalar("FinalDrive");
    p.shift_time = g.scalar("ShiftTime");
    p.efficiency = g.scalar("Efficiency");
    p.power_split = g.scalar("4WDPowerSplit");
    p.ratios = g.array<8>("Ratios[]");
    p.brake_bias = b.scalar("Bias");
    p.brake_acceleration = b.scalar("Acceleration");
    p.friction_curve = t.array<128>("TyreFrictionEnv");
    for (unsigned j = 0; j < 4; ++j)
    {
        auto tyre =
            section(text, "TYRE " + std::to_string(j), j == 3 ? "-TYRES_END" : "-TYRE " + std::to_string(j + 1));
        auto &w = p.tyres[j];
        w.radius = tyre.scalar("Radius");
        w.pressure = tyre.scalar("Pressure");
        w.static_friction = tyre.scalar("Cs");
        w.kinetic_friction = tyre.scalar("Ck");
        w.travel_in = tyre.scalar("MaxTravelIn");
        w.damping_in = tyre.scalar("MaxDampingIn");
        w.stiffness_in = tyre.scalar("MaxStiffnessIn");
        w.travel_out = tyre.scalar("MaxTravelOut");
        w.damping_out = tyre.scalar("MaxDampingOut");
        w.stiffness_out = tyre.scalar("MaxStiffnessOut");
    }
    p.drag_coefficient = a.scalar("Cd");
    p.lift_coefficient = a.scalar("Cl");
    p.frontal_area = a.scalar("FA");
    p.steering_max = s.scalar("MaxAngle");
    p.steering_sensitivity = s.scalar("Sensitivity");
    p.steering_acceleration = s.scalar("Acceleration");
    p.steering_curve = s.array<128>("AlignmentTorque");
    p.digital_steering = c.array<128>("DigitalSteeringInputEnv");
    p.digital_brake = c.array<128>("DigitalBrakeInputEnv");
    p.analog_steering = c.array<128>("AnalogSteeringInputEnv");
    p.analog_brake = c.array<128>("AnalogBrakeInputEnv");
    p.velocity_steering = c.array<128>("AnalogVelocitySteeringInputEnv");
    p.fuel_capacity = f.scalar("MaxFuel");
    p.fuel_unit_mass = f.scalar("UnitMass");
    p.fuel_consumption = f.scalar("MaxConsumption");
    p.refuel_rate = f.scalar("RefuelRate");
    p.consumption_curve = f.array<128>("ConsumptionEnv");
    p.turbo_acceleration = u.scalar("RpmAcc");
    p.turbo_max = u.scalar("RpmMax");
    p.turbo_decay = u.scalar("Cm");
    p.turbo_scale = u.scalar("PowerMultiplier");
    p.turbo_consumption = u.scalar("MaxConsumption");
    p.turbo_energy_threshold = u.scalar("FuelCutoffPoint");
    p.turbo_curve = u.array<128>("PowerEnv");
    const auto stunt = section(text, "STUNT[]");
    p.launch_time = stunt.scalar("LaunchTime");
    p.launch_tolerance = stunt.scalar("LaunchTolerance");
    p.launch_speed = stunt.scalar("LaunchSpd");
    p.max_pitch_speed = stunt.scalar("MaxRotSpd_X");
    p.max_yaw_speed = stunt.scalar("MaxRotSpd_Y");
    positive(p.mass, "mass");
    positive(p.wheelbase, "wheelbase");
    positive(p.rpm_limit, "rpm limit");
    positive(p.rpm_max, "rpm max");
    positive(p.turbo_max, "turbo max");
    positive(p.shift_time, "shift time");
    positive(p.fuel_capacity, "fuel capacity");
    positive(p.track_front, "front track");
    positive(p.track_rear, "rear track");
    for (const auto &w : p.tyres)
    {
        positive(w.radius, "tyre radius");
        positive(w.pressure, "tyre pressure");
    }
    if (p.drive_mode < 0 || p.drive_mode > 2 || p.gear_count < 2 || p.gear_count > 7)
        throw FormatError("unsupported vehicle gearing");
    for (auto ratio : p.ratios)
        if (ratio == 0)
            throw FormatError("zero gear ratio");
    return p;
}
} // namespace sgn::retail
