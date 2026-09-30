#include "sgn/retail_vehicle_tuning.hpp"
#include <bit>
namespace sgn::retail
{
void apply_selection_vehicle_tuning(std::span<const SelectionRecord, 6> selection, std::span<VehicleTuningView, 6> cars)
{
    for (const auto &record : selection)
        if (record.kind == 2 && record.configuration.present == 1 && record.configuration.tuning_words[13] > 6)
            throw FormatError("selection forward ratio count outside recovered native capacity");
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto &record = selection[i];
        if (record.kind != 2 || record.configuration.present != 1)
            continue;
        const auto &raw = record.configuration.tuning_words;
        const auto bits = [&](unsigned source_offset) { return raw[(source_offset + 8) / 4]; };
        const auto real = [&](unsigned source_offset) { return std::bit_cast<float>(bits(source_offset)); };
        auto &p = cars[i].parameters;
        auto &s = cars[i].physics;
        auto &sounds = cars[i].sounds;
        auto &audio = cars[i].audio;
        p.mass = static_cast<float>(double(std::bit_cast<float>(raw[1])) + std::bit_cast<float>(raw[0]));
        p.com_a = real(0);
        p.com_b = real(4);
        p.com_height = real(8);
        p.ride_height = real(0x68);
        p.engine_decay = real(0x0c);
        p.rpm_idle = real(0x10);
        p.rpm_limit = real(0x14);
        p.rpm_max = real(0x18);
        p.max_torque = real(0x1c);
        s.torque_curve = std::bit_cast<std::int32_t>(bits(0x20));
        p.fuel_capacity = real(0xa4);
        p.refuel_rate = real(0xac);
        p.fuel_consumption = real(0xa8);
        p.drive_mode = std::bit_cast<std::int32_t>(bits(0x28));
        p.gear_count = static_cast<std::int32_t>(bits(0x2c) + 1);
        p.shift_time = real(0x30);
        p.efficiency = real(0x34);
        p.power_split = real(0x38);
        p.final_drive = real(0x3c);
        p.ratios[0] = std::bit_cast<float>(0xc003d70au);
        p.ratios[1] = 2;
        for (unsigned j = 0; j < bits(0x2c); ++j)
            p.ratios[j + 2] = real(0x40 + j * 4);
        p.brake_bias = real(0x60);
        p.brake_acceleration = real(0x64);
        for (auto &tyre : p.tyres)
        {
            tyre.static_friction = real(0x88);
            tyre.kinetic_friction = real(0x8c);
        }
        for (auto &tyre : p.tyres)
        {
            tyre.travel_in = real(0x6c);
            tyre.damping_in = real(0x70);
            tyre.stiffness_in = real(0x74);
            tyre.travel_out = real(0x78);
            tyre.damping_out = real(0x7c);
            tyre.stiffness_out = real(0x80);
        }
        p.drag_coefficient = real(0x90);
        p.lift_coefficient = real(0x94);
        p.steering_max = real(0x98);
        p.steering_sensitivity = real(0x9c);
        p.steering_acceleration = real(0xa0);
        p.turbo_acceleration = real(0xb0);
        p.turbo_max = real(0xb4);
        p.turbo_decay = real(0xb8);
        p.turbo_scale = real(0xbc);
        p.turbo_consumption = real(0xc0);
        p.turbo_energy_threshold = real(0xc4);
        p.launch_time = real(0xc8);
        p.launch_tolerance = real(0xcc);
        p.launch_speed = real(0xd0);
        p.max_pitch_speed = real(0xd4);
        p.max_yaw_speed = real(0xd8);
        sounds.engine_rev = std::bit_cast<std::int32_t>(bits(0xe0));
        sounds.engine_idle = std::bit_cast<std::int32_t>(bits(0xe4));
        sounds.engine_boost = std::bit_cast<std::int32_t>(bits(0xe8));
        sounds.horn = std::bit_cast<std::int32_t>(bits(0xdc));
        audio.field_7718 = bits(0xec);
        audio.field_771c = bits(0xf0);
        audio.field_7720 = bits(0xf4);
    }
}
} // namespace sgn::retail
