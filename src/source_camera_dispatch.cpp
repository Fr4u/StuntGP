#include "sgn/source_camera_dispatch.hpp"
#include "sgn/source_camera_ground_query.hpp"
#include "sgn/source_camera_motion.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "camera_mounted_provider.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
float f32(double x)
{
    return static_cast<float>(x);
}
using sgn::bit_reinterpret::as_float;
bool c0(double a, double b)
{
    return a < b || std::isnan(a) || std::isnan(b);
}
double square(double x)
{
    return x * x;
}
template <class A, class B> double dot(const A &a, const B &b)
{
    return (double(a.z) * b.z + double(a.y) * b.y) + double(a.x) * b.x;
}
auto &current(RaceSessionState &w, W site)
{
    if (w.controls.current >= w.controls.controllers.size())
        throw SourceAllocationError(site, "405210 current camera outside four source owners");
    return w.controls.controllers[w.controls.current];
}
W slot(RaceSessionState &w, W site)
{
    const auto id = current(w, site).vehicle;
    if (id >= w.vehicles.physics.size())
        throw SourceAllocationError(site, "405210 target outside six source vehicles");
    return id;
}
CameraLifecycle &life(RaceSessionState &w, W site)
{
    (void)current(w, site);
    return w.camera_lifecycle[w.controls.current];
}
const AuthoredCamera &authored(RaceSessionState &w, W index, W site)
{
    if (index >= w.camera_bank.records.size())
        throw SourceAllocationError(site, "camera record outside source32-slot bank");
    return w.camera_bank.records[index];
}
void projection(RaceSessionState &w)
{
    auto &c = current(w, 0x44f4c0).camera;
    camera_rebuild_projection(c, w.shared_model_identity);
    w.frame_projection = c.projection;
    w.inverse_depth_scale = c.inverse_depth_scale;
    w.depth_offset = c.depth_offset;
}
void set_lens(RaceSessionState &w, float lens)
{
    auto &c = current(w, 0x403310).camera;
    c.lens = lens;
    c.half_width = as_float(0x3faa9fbe);
    c.half_height = 1;
    projection(w);
}
void set_mode(RaceSessionState &w, W mode)
{
    auto &c = current(w, 0x4028d0).camera;
    c.mode = mode;
    c.twist = 0;
    c.collision_bypass_ticks = 300;
    // Actual412910 visits physical slots in order, with exact-one active gates.
    for (W i = 0; i < 6; ++i)
        if (w.vehicles.race[i].active == 1)
            reset_camera_dependent_effects(w.vehicles.physics[i]);
}
void apply_record(RaceSessionState &w)
{
    auto &c = current(w, 0x402950).camera;
    auto &l = life(w, 0x402955);
    if (!l.pending_record)
        return;
    c.mode = l.pending_mode; // 40296D, before even the first record read
    const auto &t = w.model_draws.template_pose;
    const auto vec = [](Vec3 v, W opaque) { return CameraVector{v.x, v.y, v.z, std::bit_cast<float>(opaque)}; };
    c.position = vec(t.position, t.position_w);
    l.scale = vec(t.scale, t.scale_w);
    c.target = vec(t.forward, t.forward_w);
    c.up = vec(t.up, t.up_w);
    l.transform_word_48 = t.tail[0];
    c.world_to_camera_reference = t.tail[1];
    c.camera_to_world_reference = t.tail[2];
    l.transform_word = t.tail[3];
    const auto &r = authored(w, *l.pending_record, 0x402985);
    c.position = r.position;
    c.target = r.target;
    c.up = r.up;
    c.lens = r.lens;
    l.base_lens = r.lens;
    c.twist = r.twist;
    camera_clear_velocity(c);
    camera_clear_acceleration(c);
    projection(w);
    if (l.pending_mode == 4)
    {
        const auto i = slot(w, 0x402a28);
        const auto &p = w.vehicles.physics[i].position;
        const auto &offset = w.vehicles.camera_settings[i].target_offset;
        c.target.x = f32(double(offset.x) + p.x);
        c.target.y = f32(double(offset.y) + p.y);
        c.target.z = f32(double(offset.z) + p.z);
    }
    l.pending_record.reset(); // only after completed402A67
}
W select_authored(RaceSessionState &w, W vehicle)
{
    const auto count = w.camera_bank.count;
    if (count == 1)
        return 0;
    if (vehicle >= 6)
        throw SourceAllocationError(0x40380f, "authored camera target outside actual vehicle bank");
    if (w.vehicles.race[vehicle].active != 1)
        return 0;
    const auto selected = life(w, 0x403822).current_record;
    if (!selected)
        return 0;
    const auto index = *selected;
    auto next = index + 1u;
    // Original unsigned pointer bound uses wrapping SHL6/ADD, without an
    // eager count gate. Each actual record read has its own bank bound below.
    if (0x4722d8u + (next << 6) >= 0x4722d8u + (count << 6))
        next = 0;
    const auto &a = authored(w, index, 0x403844);
    const auto &b = authored(w, next, 0x403874);
    const auto &p = w.vehicles.physics[vehicle].position;
    const auto distance = [&](const CameraVector &v) {
        return (square(double(v.z) - p.z) + square(double(v.y) - p.y)) + square(double(v.x) - p.x);
    };
    return c0(distance(b.position), distance(a.position)) ? next : index;
}
void authored_lens(RaceSessionState &w, W vehicle)
{
    auto &c = current(w, 0x4038c0).camera;
    auto &l = life(w, 0x4038c6);
    if (c0(l.clip_far, 1.0))
        return;
    if (!l.current_record)
        throw SourceAllocationError(0x4038e3, "authored lens reached null current record");
    const auto &r = authored(w, *l.current_record, 0x4038e3);
    if (r.tracking == 0)
    {
        camera_request_lens(c, r.lens);
        return;
    }
    if (r.tracking != 1 && r.tracking != 2)
        return;
    if (vehicle >= 6)
        throw SourceAllocationError(0x4038fb, "authored lens target outside actual vehicle bank");
    const auto &p = w.vehicles.physics[vehicle].position;
    double d = std::sqrt((square(double(r.position.z) - p.z) + square(double(r.position.y) - p.y)) +
                         square(double(r.position.x) - p.x));
    d *= as_float(0x37d1b717);
    d *= r.tracking == 1 ? 10.0 : 2.75;
    if (d > 1.0)
        d = 1.0;
    camera_request_lens(c, f32(d * r.lens));
}
void preset(RaceSessionState &w, W vehicle, bool far_preset)
{
    auto &v = w.vehicles.camera_settings[vehicle];
    v.angles.x = far_preset ? 18.0f : 13.0f;
    v.angles.y = 0;
    v.angles.z = 0;
    v.distance = far_preset ? 1000.0f : 600.0f;
    v.approach_scale = far_preset ? 0.125f : 0.5f;
    v.target_offset.x = 0;
    v.target_offset.y = 150;
    v.target_offset.z = 0;
    w.vehicles.race[vehicle].saved_camera_mode = 5;
    v.adaptive_distance = 0;
    set_mode(w, 5);
    set_lens(w, as_float(0x4005c28f));
    camera_request_lens(current(w, 0x402ab0).camera, as_float(0x4005c28f));
    w.selected_camera_preset = far_preset ? 2u : 1u;
}
void prepare_mode(RaceSessionPlatform &platform)
{
    auto &w = platform.state();
    const auto &c = current(w, 0x4039b0).camera;
    const auto mode = c.mode;
    const auto vehicle = current(w, 0x4039ba).vehicle; // pointer capture only
    if (mode < 10 || mode > 16)
        return;
    if (mode == 10)
    {
        const auto chosen = select_authored(w, vehicle);
        auto &l = life(w, 0x4039e2);
        if (!l.current_record || chosen != *l.current_record)
        {
            if (vehicle >= 6)
                throw SourceAllocationError(0x4039f4, "authored camera offset target outside vehicle bank");
            auto &offset = w.vehicles.camera_settings[vehicle].target_offset;
            offset.x = 0;
            offset.y = 0;
            offset.z = 0;
            l.pending_record = chosen;
            l.pending_mode = 10;
            l.current_record = chosen;
        }
        if (authored(w, *l.current_record, 0x403a1c).tracking != 3)
            track_source_camera_target_403770(platform, vehicle);
        authored_lens(w, vehicle);
        return;
    }
    auto &actual = current(w, 0x403a32).camera;
    if (actual.pending_follow_setup != 1)
        return;
    actual.pending_follow_setup = 0;
    if (mode != 16)
    {
        (void)slot(w, 0x403a4b);
        preset(w, vehicle, mode == 11);
    }
    if (mode == 12)
    {
        auto &v = w.vehicles.camera_settings[vehicle];
        v.angles.x = 6;
        v.angles.y = 200;
        v.angles.z = 0;
        v.distance = 1350;
        camera_request_lens(actual, 1.25f);
    }
    else if (mode == 13 || mode == 14)
    {
        auto &v = w.vehicles.camera_settings[vehicle];
        v.angles.x = mode == 13 ? 20.0f : 30.0f;
        v.angles.y = mode == 13 ? 260.0f : 45.0f;
        v.angles.z = 0;
        v.distance = 2000;
    }
    set_mode(w, mode);
}
void consume_lens(RaceSessionState &w)
{
    auto &c = current(w, 0x402a80).camera;
    if (c.pending_lens == 0)
        return;
    c.lens = c.requested_lens;
    c.pending_lens = 0;
    projection(w);
}
void adaptive_distance(RaceSessionState &w)
{
    const auto i = w.current_slot;
    if (i >= 6)
        throw SourceAllocationError(0x404366, "404360 current vehicle outside source bank");
    auto &v = w.vehicles.camera_settings[i];
    if (v.adaptive_distance == 0)
        return;
    const auto next = std::bit_cast<std::int32_t>(w.vehicles.race[i].place + 1u);
    auto selected = i;
    if (next < std::bit_cast<std::int32_t>(w.ranking.count))
    {
        if (next < 0 || next >= 6 || !w.ranking.slots[static_cast<unsigned>(next)])
            throw SourceAllocationError(0x404385, "404360 reached invalid ranking pointer");
        selected = *w.ranking.slots[static_cast<unsigned>(next)];
        if (selected >= 6)
            throw SourceAllocationError(0x404396, "404360 ranked vehicle outside source bank");
    }
    const double gap =
        ((double(w.vehicles.race[i].progress) - w.vehicles.race[selected].progress) * w.clock.course_length) *
        as_float(0x3c23d70a);
    const bool close =
        gap > 0 && c0(gap, 5000.0) &&
        double(w.vehicles.physics[selected].speed) + as_float(0x41055555) > w.vehicles.physics[i].speed &&
        w.vehicles.physics[i].speed > as_float(0x3ed55555);
    v.distance = f32(double(v.distance) + (close ? 10.0 : -5.0));
    if (c0(v.distance, 600.0))
        v.distance = 600;
    else if (v.distance > 2500)
        v.distance = 2500;
}
void finish_orbit(RaceSessionState &w)
{
    auto &settings = w.vehicles.camera_settings[slot(w, 0x4031eb)];
    const double next = double(settings.angles.y) + 0.5;
    settings.angles.y = f32(next);
    if (c0(next, 0.0))
        settings.angles.y = f32(next + 360.0);
    else if (!c0(next, 360.0))
        settings.angles.y = f32(next - 360.0);
    auto &c = current(w, 0x403e8e).camera;
    if (std::abs(double(c.lens) - 1.25) > as_float(0x3ca3d70a))
        camera_request_lens(
            c, f32(double(c.lens) + (c.lens > 1.25 ? -double(as_float(0x3c23d70a)) : double(as_float(0x3c23d70a)))));
    auto &v = w.vehicles.camera_settings[slot(w, 0x403ee2)];
    if (v.angles.x > 6)
        v.angles.x = f32(double(v.angles.x) - as_float(0x3dcccccd));
    if (v.distance > 500)
        v.distance = f32(double(v.distance) - 5.0);
}
void speed_lens(RaceSessionState &w)
{
    auto &c = current(w, 0x403bc0).camera;
    if (c.mode >= 10)
        return;
    c.speed_lift = 0;
    const auto &v = w.vehicles.physics[slot(w, 0x403bda)].velocity;
    double delta = std::sqrt((square(v.x) + square(v.y)) + square(v.z)) - as_float(0x42a6aaab);
    if (c0(delta, 0.0))
    {
        set_lens(w, as_float(0x4005c28f));
        return;
    }
    delta *= as_float(0x3cc49ba5);
    if (delta > 1.0)
        delta = 1.0;
    c.speed_lift = f32(delta * 75.0);
    set_lens(w, f32(double(as_float(0x4005c28f)) - delta * 0.5));
}
W low_word(double value)
{
    // 462860 masked FISTP signed64 then EAX, including integer indefinite.
    constexpr double limit = 9223372036854775808.0;
    if (!std::isfinite(value) || value < -limit || value >= limit)
        return 0;
    return static_cast<W>(static_cast<std::int64_t>(value));
}
CameraVector rotate_degrees(RaceSessionState &w, const CameraVector &angle, CameraVector value)
{
    const auto convert = [](float degrees) {
        return std::bit_cast<std::int32_t>(low_word(double(degrees) * as_float(0x41b60b61)));
    };
    const auto z = convert(angle.z), y = convert(angle.y), x = convert(angle.x);
    auto &matrix = w.source_light_rotation_61c3a4; // sole shared math scratch, also used by lights
    matrix = camera_rotation(x, y, z);             // 44EC38 ->44FB00 before vector stores
    return {f32((double(matrix[8]) * value.z + double(matrix[4]) * value.y) + double(matrix[0]) * value.x),
            f32((double(matrix[9]) * value.z + double(matrix[5]) * value.y) + double(matrix[1]) * value.x),
            f32((double(matrix[10]) * value.z + double(matrix[6]) * value.y) + double(matrix[2]) * value.x), value.w};
}
void normalize(CameraVector &v)
{
    const double sum = (square(v.x) + square(v.y)) + square(v.z);
    // 44ED66 tests C3 alone; zero and unordered both clear Z/Y/X.
    if (sum == 0 || std::isnan(sum))
    {
        v.z = 0;
        v.y = 0;
        v.x = 0;
        return;
    }
    const double reciprocal = 1.0 / std::sqrt(sum);
    v.x = f32(reciprocal * v.x);
    v.y = f32(reciprocal * v.y);
    v.z = f32(reciprocal * v.z);
}
void integrate_velocity(RaceSessionState &w)
{
    auto &c = current(w, 0x403080).camera;
    c.velocity.x = f32(double(c.drift.x) + c.velocity.x);
    c.velocity.y = f32(double(c.drift.y) + c.velocity.y);
    c.velocity.z = f32(double(c.drift.z) + c.velocity.z);
    c.velocity.x = f32(double(c.velocity.x) * as_float(0x3f733333));
    c.velocity.y = f32(double(c.velocity.y) * as_float(0x3f733333));
    const double z = double(c.velocity.z) * as_float(0x3f733333);
    c.velocity.z = f32(z);
    c.linear_speed = f32(std::sqrt((z * z + square(c.velocity.y)) + square(c.velocity.x)));
    if (c0(std::abs(double(c.velocity.x)), as_float(0x3dcccccd)))
        c.velocity.x = 0;
    if (c0(std::abs(double(c.velocity.y)), as_float(0x3dcccccd)))
        c.velocity.y = 0;
    if (c0(std::abs(double(c.velocity.z)), as_float(0x3dcccccd)))
        c.velocity.z = 0;
}
void integrate_angles(RaceSessionState &w)
{
    auto &c = current(w, 0x402f61).camera;
    c.angular_velocity.x = f32(double(c.angular_acceleration.x) + c.angular_velocity.x);
    c.angular_velocity.y = f32(double(c.angular_acceleration.y) + c.angular_velocity.y);
    c.angular_velocity.z = f32(double(c.angular_acceleration.z) + c.angular_velocity.z);
    const double xy = std::bit_cast<double>(std::uint64_t{0xc0f6c16c16c16c17});
    const double z = std::bit_cast<double>(std::uint64_t{0x40f6c16c16c16c17});
    c.control_angles[0] = (c.control_angles[0] - low_word(double(c.angular_velocity.x) * xy)) & 0x1ffffffu;
    c.control_angles[1] = (c.control_angles[1] - low_word(double(c.angular_velocity.y) * xy)) & 0x1ffffffu;
    c.control_angles[2] = low_word(double(c.angular_velocity.z) * z) & 0x1ffffffu;
    c.angular_velocity.x = f32(double(c.angular_velocity.x) * as_float(0x3f733333));
    c.angular_velocity.y = f32(double(c.angular_velocity.y) * as_float(0x3f733333));
    if (c0(std::abs(double(c.angular_velocity.x)), as_float(0x3dcccccd)))
        c.angular_velocity.x = 0;
    if (c0(std::abs(double(c.angular_velocity.y)), as_float(0x3dcccccd)))
        c.angular_velocity.y = 0;
    if (c0(std::abs(double(c.angular_velocity.z)), as_float(0x3dcccccd)))
        c.angular_velocity.z = 0;
}
void orbit_offset(RaceSessionState &w)
{
    auto &v = w.vehicles.camera_settings[slot(w, 0x4032b9)];
    // 44EC00 reads XYZ only; its copied uninitialized local W is outside the
    // defined numeric result and is not invented as zero in the source bank.
    const auto out = rotate_degrees(w, v.angles, {0, 0, v.distance, 0});
    v.orbit_offset.x = out.x;
    v.orbit_offset.y = out.y;
    v.orbit_offset.z = out.z;
}
void mounted(RaceSessionPlatform &platform)
{
    auto &w = platform.state();
    auto &c = current(w, 0x404d01).camera;
    const auto i = slot(w, 0x404d01);
    auto &v = w.vehicles.camera_settings[i];
    auto &p = w.vehicles.physics[i];
    v.orbit_offset.x = 0;
    v.orbit_offset.y = 100;
    v.orbit_offset.z = 0;
    const float rx = f32(double(v.orbit_offset.x) * p.basis_x.x), ry = f32(double(v.orbit_offset.x) * p.basis_x.y),
                rz = f32(double(v.orbit_offset.x) * p.basis_x.z);
    const double ux = double(v.orbit_offset.y) * p.up.x;
    const float uy = f32(double(v.orbit_offset.y) * p.up.y), uz = f32(double(v.orbit_offset.y) * p.up.z);
    const float fx = f32(double(v.orbit_offset.z) * p.basis_z.x), fy = f32(double(v.orbit_offset.z) * p.basis_z.y),
                fz = f32(double(v.orbit_offset.z) * p.basis_z.z);
    c.position.x = f32(double(f32(double(fx) + (ux + rx))) + p.position.x);
    c.position.y = f32(double(f32(double(fy) + f32(double(uy) + ry))) + p.position.y);
    c.position.z = f32(double(f32(double(fz) + f32(double(uz) + rz))) + p.position.z);
    auto axis = [&](W at) {
        const auto handle = w.vehicles.models[slot(w, at)].model;
        if (!handle)
            throw SourceAllocationError(at, "mounted camera reached missing source model identity");
        return detail::mounted_axis(platform.source_scene_resources().models().source_at(*handle), at);
    };
    const auto rotate = [&](CameraVector a, unsigned component) {
        const auto &m = w.vehicles.physics[slot(w, 0x404e53)].orientation;
        return (double(a.z) * m[8 + component] + double(a.y) * m[4 + component]) + double(a.x) * m[component];
    };
    auto up = axis(0x404e1c);
    up.x = -up.x;
    c.up.x = f32(rotate(up, 0));
    c.up.y = f32(rotate(up, 1));
    c.up.z = f32(rotate(up, 2));
    auto forward = axis(0x404ec8);
    forward.y = -forward.y;
    const double x = rotate(forward, 0);
    const float y = f32(rotate(forward, 1)), z = f32(rotate(forward, 2));
    c.target.x = f32(x * 256.0 + c.position.x);
    c.target.y = f32(double(f32(double(y) * 256.0)) + c.position.y);
    c.target.z = f32(double(f32(double(z) * 256.0)) + c.position.z);
    c.twist = 0; // 403460 mode6 is the literal early return
    translate_source_camera_403340(platform);
}

void chase(RaceSessionPlatform &platform, SourceCameraGroundQuery &ground)
{
    auto &w = platform.state();
    adaptive_distance(w);
    camera_twist(current(w, 0x403420).camera);
    if (current(w, 0x4044be).camera.mode == 16)
        finish_orbit(w);
    speed_lens(w);
    orbit_offset(w);
    auto &c = current(w, 0x4044d9).camera;
    const auto i = slot(w, 0x4044de);
    auto &v = w.vehicles.camera_settings[i];
    auto &p = w.vehicles.physics[i];
    const auto &gravity = w.gravity.unit;
    const float tx = f32(double(v.target_offset.x) + p.position.x), ty = f32(double(v.target_offset.y) + p.position.y),
                tz = f32(double(v.target_offset.z) + p.position.z);
    const double dx = double(tx) - c.target.x;
    const float dy = f32(double(ty) - c.target.y), dz = f32(double(tz) - c.target.z);
    const double third = as_float(0x3eaaaaab);
    c.target.x = f32(dx * third + c.target.x);
    c.target.y = f32(double(f32(double(dy) * third)) + c.target.y);
    c.target.z = f32(double(f32(double(dz) * third)) + c.target.z);
    const float ux_delta = f32(double(c.stabilized_up.x) - p.up.x), uy_delta = f32(double(c.stabilized_up.y) - p.up.y);
    const double uz_delta = double(c.stabilized_up.z) - p.up.z;
    c.stabilized_up.x = f32(double(c.stabilized_up.x) - f32(double(ux_delta) * as_float(0x3dcccccd)));
    c.stabilized_up.y = f32(double(c.stabilized_up.y) - f32(double(uy_delta) * as_float(0x3dcccccd)));
    c.stabilized_up.z = f32(double(c.stabilized_up.z) - f32(uz_delta * as_float(0x3dcccccd)));
    const auto up = c.stabilized_up; // source actual16-byte local at4045FD
    const double projection_dot = dot(up, p.basis_z);
    CameraVector forward{f32(double(p.basis_z.x) - f32(projection_dot * up.x)),
                         f32(double(p.basis_z.y) - f32(projection_dot * up.y)),
                         f32(double(p.basis_z.z) - f32(projection_dot * up.z)), 0};
    normalize(forward);
    CameraVector right{f32(double(forward.y) * up.z - double(forward.z) * up.y),
                       f32(double(forward.z) * up.x - double(forward.x) * up.z),
                       f32(double(forward.x) * up.y - double(forward.y) * up.x), 0};
    normalize(right);
    const float rx = f32(double(v.orbit_offset.x) * right.x), ry = f32(double(v.orbit_offset.x) * right.y),
                rz = f32(double(v.orbit_offset.x) * right.z);
    double ux = double(v.orbit_offset.y) * up.x;
    float uy = f32(double(v.orbit_offset.y) * up.y), uz = f32(double(v.orbit_offset.y) * up.z);
    const float fx = f32(double(v.orbit_offset.z) * forward.x), fy = f32(double(v.orbit_offset.z) * forward.y),
                fz = f32(double(v.orbit_offset.z) * forward.z);
    const double gravity_dot = dot(gravity, up);
    const float stored_gravity_dot = f32(gravity_dot);
    if (!c0(gravity_dot, 0.0))
    {
        ux = -ux;
        uy = -uy;
        uz = -uz;
    }
    float px = f32(((ux + rx) + fx) + p.position.x);
    float py = f32(double(f32(double(fy) + f32(double(uy) + ry))) + p.position.y);
    double pz = f32(double(f32(double(fz) + f32(double(uz) + rz))) + p.position.z); // 404803 FSTP before relative reads
    const CameraVector relative{f32(double(px) - p.position.x), f32(double(py) - p.position.y), f32(pz - p.position.z),
                                0};
    const double height = dot(gravity, relative);
    if (c0(height, 100.0))
    {
        const double amount = -(height - 100.0);
        px = f32(double(px) + f32(amount * gravity.x));
        py = f32(double(py) + f32(amount * gravity.y));
        pz += f32(amount * gravity.z);
    }
    const auto drift_gravity = [&] {
        const float x = f32(double(gravity.x) + gravity.x), y = f32(double(gravity.y) + gravity.y),
                    z = f32(double(gravity.z) + gravity.z);
        c.drift.x = f32(double(c.drift.x) - x);
        c.drift.y = f32(double(c.drift.y) - y);
        c.drift.z = f32(double(c.drift.z) - z);
    };
    const auto decay = [&] {
        c.drift.x = f32(double(c.drift.x) * as_float(0x3f666666));
        c.drift.y = f32(double(c.drift.y) * as_float(0x3f666666));
        c.drift.z = f32(double(c.drift.z) * as_float(0x3f666666));
    };
    if (!c0(stored_gravity_dot, 0.0))
        drift_gravity();
    double body_height = double(w.vehicles.parameters[i].height) - 115.0;
    if (c0(body_height, 0.0))
        body_height = 0;
    px = f32(double(px) - f32(body_height * p.up.x));
    py = f32(double(py) - f32(body_height * p.up.y));
    pz = f32(pz - body_height * p.up.z);
    py = f32((double(py) - c.height_offset) + c.speed_lift);
    const float range_x = f32(double(c.position.x) - p.position.x), range_y = f32(double(c.position.y) - p.position.y);
    const double range_z = double(c.position.z) - p.position.z;
    const double range = (range_z * f32(range_z) + square(range_y)) + square(range_x);
    const auto finish = [&] {
        move_source_camera_403460(platform, ground);
        translate_source_camera_403340(platform);
    };
    if (c0(range, 9000000.0))
    {
        if (p.contact_counter == 0 && std::abs(double(p.position.y) - p.stunt.launch_height) > 50.0)
        {
            decay();
            drift_gravity();
            camera_settle(c, 2);
            finish();
            return;
        }
        if (p.contact_class == 2)
        {
            camera_settle(c, 1);
            if (static_cast<W>(p.contact_counter) < 4)
            {
                decay();
                finish();
                return;
            }
        }
        else if (p.contact_class == 1)
        {
            camera_settle(c, 1);
            if (static_cast<W>(p.contact_counter) >= 2)
            {
                decay();
                finish();
                return;
            }
        }
    }
    c.drift.x = 0;
    c.drift.y = 0;
    c.drift.z = 0; // 404B3D..404B49, including retained writes on later failure
    // 404B4F..404B64's first local difference is overwritten without reads.
    CameraVector offset{f32(double(px) - p.position.x), f32(double(py) - p.position.y), f32(pz - p.position.z), 0};
    c.drift.x = 0;
    c.drift.y = 0;
    c.drift.z = 0; // 404B6A..404B76
    normalize(offset);
    offset.x = f32(-double(v.distance) * offset.x);
    offset.y = f32(-double(v.distance) * offset.y);
    offset.z = f32(-double(v.distance) * offset.z);
    offset.x = f32(double(offset.x) + p.position.x);
    offset.y = f32(double(offset.y) + p.position.y);
    offset.z = f32(double(offset.z) + p.position.z);
    const float motion_x = f32(double(offset.x) - c.position.x), motion_y = f32(double(offset.y) - c.position.y);
    const double motion_z = double(offset.z) - c.position.z;
    offset.x = motion_x;
    offset.y = motion_y;
    offset.z = f32(motion_z);
    float speed = f32(std::sqrt((motion_z * offset.z + square(motion_y)) + square(motion_x)) * v.approach_scale);
    double limit = 256.0;
    if (c.settle_ticks != 0)
    {
        const auto old = c.settle_ticks;
        --c.settle_ticks;
        limit = (30.0 - double(old)) * as_float(0x41088889);
    }
    if (speed > limit)
        speed = f32(limit);
    normalize(offset);
    c.velocity.x = f32(double(speed) * offset.x);
    c.velocity.y = f32(double(speed) * offset.y);
    c.velocity.z = f32(double(speed) * offset.z);
    c.drift.x = 0;
    c.drift.y = 0;
    c.drift.z = 0; // 404CDF..404CEB precedes403460
    finish();
}
void update_mode(RaceSessionPlatform &platform, SourceCameraGroundQuery &ground)
{
    auto &w = platform.state();
    auto &c = current(w, 0x404456).camera;
    if (c.mode == 7)
    {
        advance_source_camera_mode7_404fad(platform, &ground);
        return;
    }
    c.previous_target.x = c.target.x;
    c.previous_target.y = c.target.y;
    c.previous_target.z = c.target.z;
    c.previous_target.w = c.target.w;
    switch (c.mode)
    {
    case 4:
        track_source_camera_target_403770(platform, slot(w, 0x404499));
        move_source_camera_403460(platform, ground);
        translate_source_camera_403340(platform);
        return;
    case 5:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
        chase(platform, ground);
        return;
    case 6:
        mounted(platform);
        return;
    default:
        move_source_camera_403460(platform, ground);
        translate_source_camera_403340(platform);
        return;
    }
}
} // namespace
void advance_source_camera_405210(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(0x405210, "camera dispatcher requires actual source platform");
    auto &w = platform.state();
    SourceCameraGroundQuery ground;
    for (W i = 0; i < 4; ++i)
    {
        const auto active = w.controls.controllers[i].active; // 405216 before selector publication
        w.controls.current = i;                               // 405218, also for inactive controllers
        if (active != 1)
            continue;
        apply_record(w);
        prepare_mode(platform);
        consume_lens(w);
        update_mode(platform, ground);
        integrate_velocity(w);
        integrate_angles(w);
    }
    w.controls.current = 0; // 40524F only after every reached child returned
}
} // namespace sgn::retail
