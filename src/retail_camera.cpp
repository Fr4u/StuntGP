#include "sgn/retail_camera.hpp"
#include "sgn/retail_camera_tables.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
double square(float value)
{
    return double(value) * value;
}
// Dot-product order is the actual x87 instruction order, not pseudocode order.
double dot_zyx(CameraVector a, CameraVector b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
CameraVector add(CameraVector a, CameraVector b)
{
    return {f32(double(a.x) + b.x), f32(double(a.y) + b.y), f32(double(a.z) + b.z), a.w};
}
CameraVector subtract(CameraVector a, CameraVector b)
{
    return {f32(double(a.x) - b.x), f32(double(a.y) - b.y), f32(double(a.z) - b.z), a.w};
}
std::int32_t angle_index(float degrees)
{
    const double value = double(degrees) * as_float(0x41b60b61); // literal at 0046eba0
    if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max())
        throw std::domain_error("retail camera angle outside finite int32 domain");
    return static_cast<std::int32_t>(value); // original __ftol truncation
}
} // namespace

CameraMatrix camera_rotation(std::int32_t x, std::int32_t y, std::int32_t z)
{
    // 0044fb00..0044fbc0. Trig values are the literal retail table, not libm.
    auto sine = [](std::int32_t index) {
        return double(as_float(camera_detail::trig_bits[static_cast<std::uint32_t>(index) & 8191]));
    };
    auto cosine = [](std::int32_t index) {
        return double(as_float(camera_detail::trig_bits[(static_cast<std::uint32_t>(index) & 8191) + 2048]));
    };
    const double sx = sine(x), sy = sine(y), sz = sine(z), cx = cosine(x), cy = cosine(y), cz = cosine(z);
    CameraMatrix m{};
    m[0] = f32(cz * cy);
    m[1] = f32(sz * cy);
    m[2] = f32(-sy);
    m[4] = f32((sy * sx) * cz - sz * cx);
    m[5] = f32((sy * sx) * sz + cz * cx);
    m[6] = f32(sx * cy);
    m[8] = f32(sz * sx + (sy * cx) * cz);
    m[9] = f32((sy * cx) * sz - sx * cz);
    m[10] = f32(cy * cx);
    m[15] = 1;
    return m;
}
CameraVector camera_rotate_degrees(CameraVector angles, CameraVector value, CameraMatrix *audit_rotation)
{
    const auto m = camera_rotation(angle_index(angles.x), angle_index(angles.y), angle_index(angles.z));
    if (audit_rotation)
        *audit_rotation = m;
    return {f32((double(m[8]) * value.z + double(m[4]) * value.y) + double(m[0]) * value.x),
            f32((double(m[9]) * value.z + double(m[5]) * value.y) + double(m[1]) * value.x),
            f32((double(m[10]) * value.z + double(m[6]) * value.y) + double(m[2]) * value.x), value.w};
}
void camera_normalize(CameraVector &v)
{
    // 0044ed40 retains length and reciprocal in x87 registers (binary53).
    const double length_squared = (square(v.x) + square(v.y)) + square(v.z);
    if (length_squared == 0)
    {
        v.x = 0;
        v.y = 0;
        v.z = 0;
        return;
    }
    const double reciprocal = 1.0 / std::sqrt(length_squared);
    v.x = f32(double(reciprocal) * v.x);
    v.y = f32(double(reciprocal) * v.y);
    v.z = f32(double(reciprocal) * v.z);
}
void camera_orbit_offset(CameraVehicle &v)
{
    const auto offset = camera_rotate_degrees(v.angles, {0, 0, v.distance, 0});
    v.orbit_offset.x = offset.x;
    v.orbit_offset.y = offset.y;
    v.orbit_offset.z = offset.z;
}
void camera_projection(CameraState &s)
{
    // 0044f4e0: three matrix multiplies collapse for the symmetric frustum.
    // Preserve the argument stores performed before each multiply.
    const float far_plane = f32(double(s.far_factor) * 4096);
    const float lens2 = f32(double(s.lens) + s.lens);
    const float x = f32(double(lens2) / (double(s.half_width) - (-double(s.half_width))));
    const float y = f32(double(lens2) / (double(s.half_height) - (-double(s.half_height))));
    const double range = double(far_plane) - s.near_clip;
    const float depth = f32(double(far_plane) / range);
    const float translation = f32(-(double(far_plane) * s.near_clip) / range);
    s.projection = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, depth, 1, 0, 0, translation, 0};
    s.inverse_depth_scale = f32(1.0 / double(depth));
    s.depth_offset = translation;
}
void camera_rebuild_projection(CameraState &s, CameraMatrix &shared_model_identity)
{
    camera_projection(s);
    initialize_model_matrix_identity(shared_model_identity); // 44F4D9, before returning to40333A
}
void camera_set_lens(CameraState &s, float value, CameraMatrix &shared_model_identity)
{
    s.lens = value;
    s.half_width = as_float(0x3faa9fbe);
    s.half_height = 1;
    camera_rebuild_projection(s, shared_model_identity);
}
void camera_consume_lens(CameraState &s, CameraMatrix &shared_model_identity)
{
    if (s.pending_lens != 0)
    {
        s.lens = s.requested_lens;
        s.pending_lens = 0;
        camera_rebuild_projection(s, shared_model_identity);
    }
}
void camera_integrate_velocity(CameraState &s)
{
    // 00403080..00403171: Z remains extended after its float32 store, whereas
    // the length calculation reloads X and Y. Speed precedes the dead zone.
    const float damping = as_float(0x3f733333), dead_zone = as_float(0x3dcccccd);
    s.velocity = add(s.velocity, s.drift);
    s.velocity.x = f32(double(s.velocity.x) * damping);
    s.velocity.y = f32(double(s.velocity.y) * damping);
    const double z = double(s.velocity.z) * damping;
    s.velocity.z = f32(z);
    s.linear_speed = f32(std::sqrt((z * z + square(s.velocity.y)) + square(s.velocity.x)));
    if (std::abs(s.velocity.x) < dead_zone)
        s.velocity.x = 0;
    if (std::abs(s.velocity.y) < dead_zone)
        s.velocity.y = 0;
    if (std::abs(s.velocity.z) < dead_zone)
        s.velocity.z = 0;
}
void camera_integrate_angles(CameraState &s)
{
    // 00402f60..0040307b: __ftol yields a signed 64-bit integer, but callers
    // retain only EAX. Validate before converting to avoid native C++ UB.
    const auto velocity = add(s.angular_velocity, s.angular_acceleration);
    constexpr double xy = std::bit_cast<double>(std::uint64_t{0xc0f6c16c16c16c17});
    constexpr double z = std::bit_cast<double>(std::uint64_t{0x40f6c16c16c16c17});
    auto low_word = [](double value) {
        constexpr double limit = 9223372036854775808.0;
        if (!std::isfinite(value) || value < -limit || value >= limit)
            throw std::domain_error("retail camera angular update outside finite int64 domain");
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
    };
    const auto dx = low_word(double(velocity.x) * xy), dy = low_word(double(velocity.y) * xy),
               dz = low_word(double(velocity.z) * z);
    s.angular_velocity = velocity;
    s.control_angles[0] = (s.control_angles[0] - dx) & 0x1ffffffu;
    s.control_angles[1] = (s.control_angles[1] - dy) & 0x1ffffffu;
    s.control_angles[2] = dz & 0x1ffffffu;
    const float damping = as_float(0x3f733333), dead_zone = as_float(0x3dcccccd);
    s.angular_velocity.x = f32(double(s.angular_velocity.x) * damping);
    s.angular_velocity.y = f32(double(s.angular_velocity.y) * damping);
    if (std::abs(s.angular_velocity.x) < dead_zone)
        s.angular_velocity.x = 0;
    if (std::abs(s.angular_velocity.y) < dead_zone)
        s.angular_velocity.y = 0;
    if (std::abs(s.angular_velocity.z) < dead_zone)
        s.angular_velocity.z = 0;
}
namespace
{
void require_camera_reset(const CameraModeReset &reset)
{
    if (reset.reset == nullptr)
        for (const auto active : reset.active)
            if (active == 1)
                throw std::logic_error("camera mode requires active vehicle effect reset");
}
} // namespace
void camera_select_mode(CameraState &s, std::uint32_t mode, CameraModeReset reset)
{
    require_camera_reset(reset);
    s.mode = mode;
    s.twist = 0;
    s.collision_bypass_ticks = 300;
    for (std::uint32_t slot = 0; slot < reset.active.size(); ++slot)
        if (reset.active[slot] == 1)
            reset.reset(reset.context, slot);
}
void camera_apply_preset(CameraState &s, CameraVehicle &v, std::uint32_t preset, std::uint32_t &selected,
                         CameraModeReset reset, CameraMatrix &shared_model_identity)
{
    if (preset < 1 || preset > 4)
        throw std::invalid_argument("unknown retail camera preset");
    require_camera_reset(reset);
    const std::uint32_t base = preset == 4 ? 1 : preset;
    v.angles.x = base == 1 ? 13.0f : (base == 2 ? 18.0f : 20.0f);
    v.angles.y = 0;
    v.angles.z = 0;
    v.distance = base == 1 ? 600.0f : (base == 2 ? 1000.0f : 1500.0f);
    v.approach_scale = base == 1 ? 0.5f : 0.125f;
    v.target_offset.x = 0;
    v.target_offset.y = 150;
    v.target_offset.z = 0;
    v.camera_mode = 5;
    v.adaptive_distance = 0;
    camera_select_mode(s, 5, reset);
    camera_set_lens(s, as_float(0x4005c28f), shared_model_identity);
    camera_request_lens(s, as_float(0x4005c28f));
    selected = base;
    if (preset == 4)
    {
        v.adaptive_distance = 1;
        selected = 4;
    }
}
void camera_restore_preset(CameraState &s, CameraVehicle &v, std::uint32_t &selected, std::uint32_t force_close,
                           CameraModeReset reset, CameraMatrix &shared_model_identity)
{
    const auto preset = force_close != 0 ? 1 : selected;
    if (preset >= 1 && preset <= 4)
        camera_apply_preset(s, v, preset, selected, reset, shared_model_identity);
}
void camera_prepare_follow_mode(CameraState &s, CameraVehicle &v, std::uint32_t &selected, CameraModeReset reset,
                                CameraMatrix &shared_model_identity)
{
    if (s.mode == 10)
        throw std::logic_error("authored track camera input is required for mode 10");
    if (s.mode < 11 || s.mode > 16 || s.pending_follow_setup != 1)
        return;
    require_camera_reset(reset);
    const auto mode = s.mode;
    s.pending_follow_setup = 0;
    if (mode != 16)
        camera_apply_preset(s, v, mode == 11 ? 2 : 1, selected, reset, shared_model_identity);
    if (mode == 12)
    {
        v.angles.x = 6;
        v.angles.y = 200;
        v.angles.z = 0;
        v.distance = 1350;
        camera_request_lens(s, 1.25f);
    }
    else if (mode == 13 || mode == 14)
    {
        v.angles.x = mode == 13 ? 20.0f : 30.0f;
        v.angles.y = mode == 13 ? 260.0f : 45.0f;
        v.angles.z = 0;
        v.distance = 2000;
    }
    camera_select_mode(s, mode, reset);
}
void camera_translate(CameraState &s, const CameraVehicle &v)
{
    // Two separately rounded operations; combining the shifts changes results.
    s.position = subtract(s.position, v.previous_shift);
    s.target = subtract(s.target, v.previous_shift);
    s.position = add(s.position, v.shift);
    s.target = add(s.target, v.shift);
}
void camera_twist(CameraState &s)
{
    s.twist = f32((double(s.twist) - dot_zyx(s.velocity, s.side) * 0.25) * 0.5);
}
void camera_settle(CameraState &s, std::uint32_t increment)
{
    if (double(s.settle_ticks) < 30)
        s.settle_ticks += increment;
}
void camera_track_target(CameraState &s, const CameraVehicle &v)
{
    if (v.active == 0)
        return;
    // 00403770 keeps x on x87 but spills y/z after the sum and difference.
    const double x = (double(v.target_offset.x) + v.position.x) - s.target.x;
    const float y = f32(double(f32(double(v.target_offset.y) + v.position.y)) - s.target.y);
    const float z = f32(double(f32(double(v.target_offset.z) + v.position.z)) - s.target.z);
    s.target.x = f32(x * 0.125 + s.target.x);
    s.target.y = f32(double(f32(double(y) * 0.125)) + s.target.y);
    s.target.z = f32(double(f32(double(z) * 0.125)) + s.target.z);
}
void camera_speed_lens(CameraState &s, const CameraVehicle &v, CameraMatrix &shared_model_identity)
{
    if (s.mode >= 10)
        return;
    s.speed_lift = 0;
    const double delta =
        std::sqrt((square(v.velocity.x) + square(v.velocity.y)) + square(v.velocity.z)) - double(as_float(0x42a6aaab));
    if (delta < 0)
    {
        camera_set_lens(s, as_float(0x4005c28f), shared_model_identity);
        return;
    }
    double fraction = delta * double(as_float(0x3cc49ba5));
    if (fraction > 1)
        fraction = 1;
    s.speed_lift = f32(fraction * 75);
    camera_set_lens(s, f32(double(as_float(0x4005c28f)) - fraction * 0.5), shared_model_identity);
}
void camera_orbit_angle(CameraVehicle &v, float increment)
{
    const double next = double(increment) + v.angles.y;
    v.angles.y = f32(next);
    if (next < 0)
        v.angles.y = f32(next + 360);
    else if (next >= 360)
        v.angles.y = f32(next - 360);
}
void camera_request_lens(CameraState &s, float value)
{
    s.pending_lens = 1;
    s.requested_lens = value;
}
void camera_finish_orbit(CameraState &s, CameraVehicle &v)
{
    camera_orbit_angle(v, 0.5f);
    if (std::abs(double(s.lens) - 1.25) > double(0.02f))
        camera_request_lens(s, f32(double(s.lens) + (s.lens <= 1.25f ? double(0.01f) : -double(0.01f))));
    if (v.angles.x > 6)
        v.angles.x = f32(double(v.angles.x) - double(0.1f));
    if (v.distance > 500)
        v.distance = f32(double(v.distance) - 5);
}
void camera_adaptive_distance(CameraVehicle &v, const CameraRaceContext &race)
{
    if (v.adaptive_distance == 0)
        return;
    const double gap = ((double(v.progress) - race.next_progress) * race.track_length) * double(0.01f);
    const bool approach = gap > 0 && gap < 5000 && double(race.next_speed) + double(as_float(0x41055555)) > v.speed &&
                          v.speed > as_float(0x3ed55555);
    v.distance = f32(double(v.distance) + (approach ? 10 : -5));
    if (v.distance < 600)
        v.distance = 600;
    else if (v.distance > 2500)
        v.distance = 2500;
}
void camera_move(CameraState &s, const CameraVehicle &v, CameraCollision collision)
{
    if (s.mode == 6)
        return;
    const auto delta = subtract(s.position, v.position);
    if (dot_zyx(delta, delta) > 6250000)
        s.collision_bypass_ticks = 600;
    if (s.collision_bypass_ticks != 0)
    {
        s.position = add(s.position, s.velocity);
        --s.collision_bypass_ticks;
        return;
    }
    if (!collision.solve)
        throw std::logic_error("retail camera requires reconstructed track collision solver");
    CameraSweep sweep;
    sweep.position = s.position;
    sweep.velocity = s.velocity;
    sweep.speed = f32(std::sqrt(dot_zyx(s.velocity, s.velocity)));
    s.position = collision.solve(collision.context, sweep);
}
void camera_view_basis(CameraState &s)
{
    // 00450190 is called with ECX=camera+8; output pointers are camera+50/+4c.
    auto forward = subtract(s.target, s.position);
    camera_normalize(forward);
    CameraVector up{-s.up.x, -s.up.y, -s.up.z, s.up.w};
    const double parallel =
        (std::abs(double(forward.z) + up.z) + std::abs(double(forward.y) + up.y)) + std::abs(double(forward.x) + up.x);
    if (parallel < double(0.00001f))
    {
        up.z = f32(double(up.z) + 1);
        camera_normalize(up);
    }
    const double projection = dot_zyx(forward, up);
    up.x = f32(double(up.x) - projection * forward.x);
    up.y = f32(double(up.y) - projection * forward.y);
    up.z = f32(double(up.z) - projection * forward.z);
    camera_normalize(up);
    CameraVector right{f32(double(forward.y) * up.z - double(forward.z) * up.y),
                       f32(double(forward.z) * up.x - double(forward.x) * up.z),
                       f32(double(forward.x) * up.y - double(forward.y) * up.x), 0};
    camera_normalize(right);
    const std::uint32_t index = static_cast<std::uint32_t>(angle_index(s.twist)) & 8191;
    const double sine = as_float(camera_detail::trig_bits[index]),
                 cosine = as_float(camera_detail::trig_bits[index + 2048]);
    // 0044f870 -> 0044ef40 multiplies the twist matrix by the recovered basis.
    const CameraVector columns[]{right, up, forward};
    CameraMatrix basis{};
    for (unsigned j = 0; j < 3; ++j)
    {
        const double a = j == 0 ? cosine : j == 1 ? -sine : 0;
        const double b = j == 0 ? sine : j == 1 ? cosine : 0;
        const double c = j == 2 ? 1 : 0;
        basis[j * 4] = f32((double(columns[2].x) * c + double(columns[1].x) * b) + double(columns[0].x) * a);
        basis[j * 4 + 1] = f32((double(columns[2].y) * c + double(columns[1].y) * b) + double(columns[0].y) * a);
        basis[j * 4 + 2] = f32((double(columns[2].z) * c + double(columns[1].z) * b) + double(columns[0].z) * a);
    }
    basis[15] = 1;
    s.camera_to_world_rotation = basis;
    for (unsigned col = 0; col < 4; ++col)
        for (unsigned row = 0; row < 4; ++row)
            s.world_to_camera_rotation[col * 4 + row] = basis[row * 4 + col];
}
void camera_chase_tick(CameraState &s, CameraVehicle &v, const CameraRaceContext &race, CameraVector gravity,
                       CameraCollision collision, CameraMatrix &shared_model_identity, CameraVehicle *adaptive_vehicle)
{
    if (s.mode != 4 && s.mode != 5 && (s.mode < 11 || s.mode > 16))
        throw std::invalid_argument("camera_chase_tick requires original camera mode 4, 5 or 11..16");
    s.previous_target = s.target;
    auto finish = [&] {
        camera_move(s, v, collision);
        camera_translate(s, v);
    };
    if (s.mode == 4)
    {
        camera_track_target(s, v);
        finish();
        return;
    }
    camera_adaptive_distance(adaptive_vehicle ? *adaptive_vehicle : v, race);
    camera_twist(s);
    if (s.mode == 16)
        camera_finish_orbit(s, v);
    camera_speed_lens(s, v, shared_model_identity);
    camera_orbit_offset(v);
    // 004044d9..00404568: every target sum is stored, x's delta remains on x87.
    const auto target = add(v.target_offset, v.position);
    const double dx = double(target.x) - s.target.x;
    const float dy = f32(double(target.y) - s.target.y), dz = f32(double(target.z) - s.target.z);
    const double third = double(as_float(0x3eaaaaab));
    s.target.x = f32(dx * third + s.target.x);
    s.target.y = f32(double(f32(double(dy) * third)) + s.target.y);
    s.target.z = f32(double(f32(double(dz) * third)) + s.target.z);
    const auto up_delta = subtract(s.stabilized_up, v.up);
    const CameraVector up_step{f32(double(up_delta.x) * double(0.1f)), f32(double(up_delta.y) * double(0.1f)),
                               f32((double(s.stabilized_up.z) - v.up.z) * double(0.1f)), 0};
    s.stabilized_up = subtract(s.stabilized_up, up_step);
    const auto up = s.stabilized_up;
    const double projection = dot_zyx(up, v.forward);
    const CameraVector projected{f32(projection * up.x), f32(projection * up.y), f32(projection * up.z), 0};
    auto forward = subtract(v.forward, projected);
    camera_normalize(forward);
    CameraVector right{f32(double(forward.y) * up.z - double(forward.z) * up.y),
                       f32(double(forward.z) * up.x - double(forward.x) * up.z),
                       f32(double(forward.x) * up.y - double(forward.y) * up.x), 0};
    camera_normalize(right);
    const CameraVector r{f32(double(v.orbit_offset.x) * right.x), f32(double(v.orbit_offset.x) * right.y),
                         f32(double(v.orbit_offset.x) * right.z), 0};
    double ux = double(v.orbit_offset.y) * up.x;
    float uy = f32(double(v.orbit_offset.y) * up.y), uz = f32(double(v.orbit_offset.y) * up.z);
    const CameraVector f{f32(double(v.orbit_offset.z) * forward.x), f32(double(v.orbit_offset.z) * forward.y),
                         f32(double(v.orbit_offset.z) * forward.z), 0};
    const double gravity_dot = dot_zyx(gravity, up);
    const float stored_gravity_dot = f32(gravity_dot);
    if (gravity_dot >= 0)
    {
        ux = -ux;
        uy = -uy;
        uz = -uz;
    }
    float px = f32(((ux + r.x) + f.x) + v.position.x);
    float py = f32(double(f32(double(f.y) + f32(double(uy) + r.y))) + v.position.y);
    double pz = f32(double(f32(double(f.z) + f32(double(uz) + r.z))) + v.position.z);
    const CameraVector relative{f32(double(px) - v.position.x), f32(double(py) - v.position.y), f32(pz - v.position.z),
                                0};
    const double height = dot_zyx(gravity, relative);
    if (height < 100)
    {
        const double amount = -(height - 100);
        px = f32(double(px) + f32(amount * gravity.x));
        py = f32(double(py) + f32(amount * gravity.y));
        pz = pz + f32(amount * gravity.z);
    }
    auto drift_gravity = [&] {
        const CameraVector twice{f32(double(gravity.x) + gravity.x), f32(double(gravity.y) + gravity.y),
                                 f32(double(gravity.z) + gravity.z), 0};
        s.drift = subtract(s.drift, twice);
    };
    auto decay_drift = [&] {
        s.drift.x = f32(double(s.drift.x) * double(0.9f));
        s.drift.y = f32(double(s.drift.y) * double(0.9f));
        s.drift.z = f32(double(s.drift.z) * double(0.9f));
    };
    if (stored_gravity_dot >= 0)
        drift_gravity();
    const double body_height = double(v.height) - 115 < 0 ? 0 : double(v.height) - 115;
    px = f32(double(px) - f32(body_height * v.up.x));
    py = f32(double(py) - f32(body_height * v.up.y));
    pz = f32(pz - body_height * v.up.z);
    py = f32((double(py) - s.height_offset) + s.speed_lift);
    const auto camera_delta = subtract(s.position, v.position);
    const double unrounded_z = double(s.position.z) - v.position.z;
    const double squared_range = (unrounded_z * camera_delta.z + square(camera_delta.y)) + square(camera_delta.x);
    if (squared_range < 9000000)
    {
        if (v.contact_counter == 0 && std::abs(double(v.position.y) - v.contact_height) > 50)
        {
            decay_drift();
            drift_gravity();
            camera_settle(s, 2);
            finish();
            return;
        }
        if (v.contact_class == 2)
        {
            camera_settle(s, 1);
            if (v.contact_counter < 4)
            {
                decay_drift();
                finish();
                return;
            }
        }
        else if (v.contact_class == 1)
        {
            camera_settle(s, 1);
            if (v.contact_counter >= 2)
            {
                decay_drift();
                finish();
                return;
            }
        }
    }
    s.drift.x = 0;
    s.drift.y = 0;
    s.drift.z = 0;
    CameraVector offset{f32(double(px) - v.position.x), f32(double(py) - v.position.y), f32(pz - v.position.z), 0};
    camera_normalize(offset);
    offset.x = f32(-double(v.distance) * offset.x);
    offset.y = f32(-double(v.distance) * offset.y);
    offset.z = f32(-double(v.distance) * offset.z);
    offset = add(offset, v.position);
    const double motion_z = double(offset.z) - s.position.z;
    offset = subtract(offset, s.position);
    float speed = f32(std::sqrt((motion_z * offset.z + square(offset.y)) + square(offset.x)) * v.approach_scale);
    double limit = 256;
    if (s.settle_ticks != 0)
    {
        limit = (30 - double(s.settle_ticks)) * double(as_float(0x41088889));
        --s.settle_ticks;
    }
    if (double(speed) > limit)
        speed = f32(limit);
    camera_normalize(offset);
    s.velocity.x = f32(double(speed) * offset.x);
    s.velocity.y = f32(double(speed) * offset.y);
    s.velocity.z = f32(double(speed) * offset.z);
    finish();
}
} // namespace sgn::retail
