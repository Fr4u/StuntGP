#include "sgn/contact_response.hpp"
#include "sgn/vehicle_math.hpp"
#include "sgn/vehicle.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::response
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
bool supports_stabilization(std::span<const ContactProbe, 4> probes)
{
    for (const auto &probe : probes)
    {
        if (probe.contact_iterations < 4)
            return false;
    }
    return true;
}
} // namespace

void center_probes(std::span<ContactProbe> probes, float &origin_y_offset)
{
    // 00422235..0042224E keeps all sums in x87 until the loop ends, then
    // stores y/z. The inverse is still extended for x, but stored for y/z.
    double x = 0, y = 0, z = 0;
    for (const auto &probe : probes)
    {
        x += probe.local.x;
        y += probe.local.y;
        z += probe.local.z;
    }
    const double inverse = -1.0 / static_cast<double>(probes.size());
    const float stored_inverse = f32(inverse);
    const float shift_x = f32(inverse * x);
    const double shift_y = double(stored_inverse) * f32(y);
    const float shift_z = f32(double(stored_inverse) * f32(z));
    for (auto &probe : probes)
    {
        probe.local.x = f32(double(shift_x) + probe.local.x);
        probe.local.y = f32(shift_y + probe.local.y);
        probe.local.z = f32(double(shift_z) + probe.local.z);
    }
    origin_y_offset = f32(shift_y);
}

void initialize_rig(std::uint32_t selector, const Vector4 &position, ContactRig &rig)
{
    const float sixty = as_float(0x42700001);
    const auto set = [&](unsigned index, float x, float y, float z, float radius) {
        auto &probe = rig.probes[index];
        probe.local.x = x;
        probe.local.y = y;
        probe.local.z = z;
        probe.mass_fraction = 0.25f;
        probe.radius = radius;
    };
    set(0, -sixty, 0, -110, 70);
    set(1, sixty, 0, -110, 70);
    set(2, -sixty, 0, 110, 70);
    set(3, sixty, 0, 110, 70);
    rig.count = 4;
    switch (selector)
    {
    case 1:
    case 3:
    case 4:
    case 5:
    case 8:
    case 9:
    case 10:
    case 12:
    case 14:
    case 15:
    case 16:
    case 18:
    case 19:
    case 20:
        rig.count = 5;
        set(4, 0, as_float(0x41f00001), 0, sixty);
        break;
    case 2:
    case 7:
    case 11:
    case 13:
        rig.count = 5;
        set(4, 0, 50, 0, 80);
        break;
    case 6:
        rig.count = 6;
        set(4, 0, sixty, 70, 110);
        set(5, 0, sixty, -70, 110);
        break;
    case 17:
        rig.count = 6;
        set(4, 0, 50, 0, 80);
        set(5, 0, 130, 0, 80);
        break;
    default:
        break;
    }
    auto active = std::span(rig.probes).first(rig.count);
    center_probes(active, rig.origin_y_offset);
    for (auto &probe : active)
    {
        probe.current.x = f32(double(probe.local.x) + position.x);
        probe.current.y = f32(double(probe.local.y) + position.y);
        probe.current.z = f32(double(probe.local.z) + position.z);
        probe.previous = probe.current;
    }
    rig.bounding_radius = 0;
    for (const auto &probe : active)
    {
        const double x = probe.local.x, y = probe.local.y, z = probe.local.z;
        // 00408548..00408568: x*x + z*z, then y*y; keep the sum extended.
        const double radius = std::sqrt((x * x + z * z) + y * y) + probe.radius;
        if (radius > rig.bounding_radius)
            rig.bounding_radius = f32(radius);
    }
}

void damp_incremental(Matrix4 &matrix, std::span<const ContactProbe, 4> probes)
{
    if (!supports_stabilization(probes))
        return;
    auto x = vehicle_column(matrix, 0);
    auto y = vehicle_column(matrix, 1);
    auto z = vehicle_column(matrix, 2);
    // The original rejects ordered greater-than; retain its unordered branch.
    if (std::abs(double(z.x) + 0.0) > double(0.05f) || std::abs(double(z.y) + 0.0) > double(0.05f))
        return;
    x.x = f32(double(x.x) * 1.5);
    y.y = f32(double(y.y) * 1.5);
    z.z = f32(double(z.z) * 1.5);
    matrix = vehicle_basis(vehicle_normalize(x), vehicle_normalize(y), vehicle_normalize(z));
}

void place_rig(const Vector4 &position, Pose &pose, ContactBody &body, ContactRig &rig)
{
    if (rig.count > rig.probes.size())
        throw FormatError("contact placement probe count");
    pose.previous_position = position;
    pose.position = position;
    body.previous_position = position;
    body.position = position;
    for (auto &probe : std::span(rig.probes).first(rig.count))
    {
        probe.current.x = f32(double(probe.local.x) + body.position.x);
        probe.current.y = f32(double(probe.local.y) + body.position.y);
        probe.current.z = f32(double(probe.local.z) + body.position.z);
        probe.previous = probe.current;
    }
}

void settle_incremental(Matrix4 &matrix, std::span<const ContactProbe, 4> probes, bool *audit_payload_undefined)
{
    if (!supports_stabilization(probes))
        return;
    const auto z = vehicle_column(matrix, 2);
    if (std::abs(double(z.x) + 0.0) > double(0.002f) || std::abs(double(z.y) + 0.0) > double(0.002f))
        return;
    matrix = vehicle_identity();
    if (audit_payload_undefined)
        *audit_payload_undefined = false;
}

void linear_friction(const Vector4 &velocity, Vec3 normal, float strength, Vec3 &delta)
{
    const float normal_speed =
        f32((double(normal.z) * velocity.z + double(normal.y) * velocity.y) + double(normal.x) * velocity.x);
    VehicleVector tangent{f32(double(velocity.x) - double(normal.x) * normal_speed),
                          f32(double(velocity.y) - f32(double(normal.y) * normal_speed)),
                          f32(double(velocity.z) - f32(double(normal.z) * normal_speed)), 0};
    float length{};
    tangent = vehicle_normalize(tangent, &length);
    if (double(strength) * 0.2f < length)
        length = f32(double(strength) * 0.1f);
    delta.x = f32(double(delta.x) - double(tangent.x) * length);
    delta.y = f32(double(delta.y) - f32(double(tangent.y) * length));
    delta.z = f32(double(delta.z) - f32(double(tangent.z) * length));
}

void angular_friction(const Vector4 &position, const ContactProbe &probe, const Matrix4 &incremental, Vec3 snapshot_up,
                      Vec3 normal, float strength, Matrix4 &delta)
{
    const double x = probe.local.x, y = probe.local.y, z = probe.local.z;
    const float radius = f32(std::sqrt((z * z + y * y) + x * x));
    const double dx = double(f32((z * incremental[8] + y * incremental[4]) + x * incremental[0])) - x;
    const float dy = f32(double(f32((z * incremental[9] + y * incremental[5]) + x * incremental[1])) - y);
    const float dz = f32(double(f32((z * incremental[10] + y * incremental[6]) + x * incremental[2])) - z);
    const float projection = f32((double(normal.z) * dz + double(normal.y) * dy) + double(normal.x) * dx);
    VehicleVector tangent{f32(dx - double(normal.x) * projection), f32(double(dy) - f32(double(normal.y) * projection)),
                          f32(double(dz) - f32(double(normal.z) * projection)), 0};
    float length{};
    tangent = vehicle_normalize(tangent, &length);
    if (double(strength) * 0.2f < length)
        length = f32(double(strength) * 0.1f);
    float angle = f32(double(length) / radius);
    const auto axis = vehicle_normalize(vehicle_cross({snapshot_up.x, snapshot_up.y, snapshot_up.z, 0},
                                                      {probe.local.x, probe.local.y, probe.local.z, 0}));
    if ((double(tangent.z) * axis.z + double(tangent.y) * axis.y) + double(tangent.x) * axis.x < 0)
        angle = -angle;
    support_rotation({position.x, position.y, position.z, 0}, {probe.current.x, probe.current.y, probe.current.z, 0},
                     angle, delta);
}

void limit_incremental(Matrix4 &matrix)
{
    const double threshold = as_float(0x3f3504f4);
    const auto rotate = [&](unsigned axis, double value) {
        if (std::abs(value) <= threshold)
            return;
        const float argument = f32(value < 0 ? value + threshold : value - threshold);
        // 00462950/0046296D computes atan2(x,sqrt((1+x)*(1-x))).
        const double angle = std::atan2(double(argument), std::sqrt((1.0 + argument) * (1.0 - argument)));
        vehicle_rotate(matrix, axis, f32(angle * as_float(0x42652ee0)));
    };
    rotate(0, matrix[9]);
    rotate(1, -double(matrix[8]));
    rotate(2, -double(matrix[1]));
}

void add_impact_count(ImpactCount &state, std::int32_t game_mode, float strength)
{
    if (game_mode == 3 || std::abs(double(strength)) < 1.0)
        return;
    double amount = std::abs(double(strength)) * double(0.1666666716337204f);
    if (amount > 10.0)
        amount = 10.0;
    const auto increment = static_cast<std::uint32_t>(amount);
    state.value = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(state.value) + increment);
    if (state.value > state.capacity)
        state.value = state.capacity;
}

void consume_impact_energy(EnergyImpactState &state, const EnergyImpactParameters &p, float strength)
{
    if (state.exempt == 1 || state.accumulated_loss > p.accumulated_limit)
        return;
    const double extended = std::abs(double(strength)) * p.scale;
    float amount = f32(extended);
    if (extended < p.minimum)
        return;
    if (amount > p.maximum)
        amount = p.maximum;
    if (state.upgrade_active == 1)
    {
        amount = f32(((1.0 - state.upgrade_condition) * p.upgrade_loss_modifier + 1.0) * amount);
    }
    state.energy = f32(double(state.energy) - amount);
    state.accumulated_loss = f32(double(state.accumulated_loss) + amount);
    state.hold = static_cast<std::int32_t>(p.hold_ticks);
    if (state.energy < 0)
        state.energy = 0;
}

void directional_impact(DirectionalImpact &state, std::int32_t game_mode, Vec3 normal, float strength)
{
    if (game_mode != 2)
        return;
    const double scale = double(strength) * double(0.025f);
    double squared = scale * scale;
    if (squared > 5.0)
        squared = 5.0;
    const float x = f32(double(normal.x) * squared), y = f32(double(normal.y) * squared),
                z = f32(double(normal.z) * squared);
    const double length = std::sqrt((double(x) * x + double(y) * y) + double(z) * z);
    state.magnitude = f32(length + state.magnitude);
    state.accumulated.x = f32(double(x) * 4.0 + state.accumulated.x);
    state.accumulated.y = f32(double(f32(double(y) * 4.0)) + state.accumulated.y);
    state.accumulated.z = f32(double(f32(double(z) * 4.0)) + state.accumulated.z);
}

void finish_impact_physical(ContactBody &body, const ContactProbe &probe, unsigned index, Vector4 normal, Vec3 up,
                            float probe_mass, Material material, std::int32_t track_mode, ImpactAccumulator &impact,
                            WheelContact &wheel, ImpactResponseState &state)
{
    const float threshold = as_float(0x3f3504f4);
    state.alignment =
        index < 4 ? f32((double(normal.z) * up.z + double(normal.y) * up.y) + double(normal.x) * up.x) : threshold;
    if (index < 4 && state.alignment > threshold)
    {
        apply_material(material, track_mode, wheel);
        wheel.normal = normal;
        wheel.countdown = 8;
        state.wheel_touched = 1;
        state.contact_load = f32(-((double(normal.z) * probe.velocity.z + double(normal.y) * probe.velocity.y) +
                                   double(normal.x) * probe.velocity.x) *
                                 probe_mass);
        state.shock = f32((double(state.shock) - impact.normal_speed) * 0.5);
        state.effect_strength = f32(double(impact.normal_speed) * 0.25);
        return;
    }
    if (std::abs(double(normal.y)) < 0.5)
    {
        body.velocity.x = f32(double(normal.x) + body.velocity.x);
        body.velocity.y = f32(double(normal.y) + body.velocity.y);
        body.velocity.z = f32(double(normal.z) + body.velocity.z);
    }
    state.effective_com_height = state.base_com_height;
    state.reset_2628 = 0;
    state.reset_262c = 0;
    state.reset_ticks = -12;
    float strength = std::abs(impact.normal_speed);
    if (strength > 50)
        strength = 50;
    const Vec3 n{normal.x, normal.y, normal.z};
    linear_friction(body.velocity, n, strength, impact.velocity_delta);
    strength = impact.normal_speed;
    if (std::abs(double(strength)) > 5)
        strength = strength < 0 ? -5.0f : 5.0f;
    angular_friction(body.position, probe, body.incremental, up, n, strength, impact.rotation_delta);
    state.body_touched = 1;
    state.effect_strength = impact.normal_speed;
}
} // namespace sgn::retail::contact::response
