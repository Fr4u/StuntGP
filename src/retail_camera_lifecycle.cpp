#include "sgn/retail_camera_lifecycle.hpp"
#include "sgn/assets.hpp"
#include "sgn/contact_player.hpp"
#include "sgn/model_resources.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
void xyz(CameraVector &v, float x, float y, float z) noexcept
{
    v.x = x;
    v.y = y;
    v.z = z;
}
const AuthoredCamera &record(const CameraBank &bank, std::uint32_t index)
{
    if (bank.count > bank.records.size() || index >= bank.records.size())
        throw std::invalid_argument("authored camera index outside original 32-slot storage");
    return bank.records[index];
}
double squared_distance(CameraVector a, CameraVector b)
{
    const double x = double(a.x) - b.x, y = double(a.y) - b.y, z = double(a.z) - b.z;
    return (z * z + y * y) + x * x;
}
struct TextReader
{
    std::string_view rest;
    void marker(std::string_view name)
    {
        auto at = rest.find(name);
        if (at == rest.npos)
            throw FormatError("missing authored camera section: " + std::string(name));
        rest.remove_prefix(at + name.size());
    }
    double number(std::string_view key)
    {
        marker(key);
        auto at = rest.find('=');
        if (at == rest.npos)
            throw FormatError("missing camera field assignment");
        rest.remove_prefix(at + 1);
        at = rest.find_first_not_of(" \t\r\n");
        if (at == rest.npos)
            throw FormatError("missing camera field value");
        rest.remove_prefix(at);
        double value{};
        auto [end, error] = std::from_chars(rest.data(), rest.data() + rest.size(), value);
        if (error != std::errc{} || !std::isfinite(value))
            throw FormatError("invalid authored camera number");
        if (end != rest.data() + rest.size() && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n')
            throw FormatError("invalid authored camera number suffix");
        rest.remove_prefix(static_cast<std::size_t>(end - rest.data()));
        return value;
    }
    float scalar(std::string_view key)
    {
        const float value = f32(number(key));
        if (!std::isfinite(value))
            throw FormatError("authored camera scalar overflow");
        return value;
    }
    std::uint32_t integer(std::string_view key)
    {
        const double value = number(key);
        if (value < 0 || value > 4294967295.0 || std::trunc(value) != value)
            throw FormatError("invalid authored camera integer");
        return static_cast<std::uint32_t>(value);
    }
    void vector(CameraVector &v, std::string_view name)
    {
        marker(name);
        v.x = scalar("x");
        v.y = scalar("y");
        v.z = scalar("z");
    }
};
} // namespace
void camera_load_authored_text(CameraBank &bank, std::string_view text)
{
    // Stage the replacement so malformed external files cannot partially alter
    // a live bank. Original valid-file numerical conversion remains binary64->32.
    auto staged = bank;
    TextReader reader{text};
    reader.marker("-CAMERA[]");
    staged.count = reader.integer("TotalCameraCnt");
    if (staged.count > staged.records.size())
        throw FormatError("authored camera count exceeds 32");
    for (std::uint32_t index = 0; index < staged.count; ++index)
    {
        auto &camera = staged.records[index];
        reader.vector(camera.position, "-Position[]");
        reader.vector(camera.target, "-OFP[]");
        reader.vector(camera.up, "-UP[]");
        camera.lens = reader.scalar("Zoom");
        camera.tracking = reader.integer("Tracking");
        camera.twist = reader.scalar("Twist");
    }
    if (reader.rest.find("-Position[]") != reader.rest.npos)
        throw FormatError("more authored cameras than declared");
    bank = staged;
}
void camera_clear_velocity(CameraState &s) noexcept
{
    xyz(s.velocity, 0, 0, 0);
    s.linear_speed = 0;
}
void camera_clear_acceleration(CameraState &s) noexcept
{
    xyz(s.drift, 0, 0, 0);
    xyz(s.angular_acceleration, 0, 0, 0);
}
void camera_request_record(CameraLifecycle &life, std::optional<std::uint32_t> index, std::uint32_t mode)
{
    if (index && *index >= 32)
        throw std::invalid_argument("pending camera index exceeds original storage");
    life.pending_record = index;
    life.pending_mode = mode;
}
void camera_apply_record(CameraState &s, CameraLifecycle &life, const CameraVehicle &vehicle, const CameraBank &bank,
                         const ModelTransform &template_pose, CameraMatrix &shared_model_identity)
{
    if (!life.pending_record)
        return;
    const auto &input = record(bank, *life.pending_record);
    s.mode = life.pending_mode;
    // 402971..40297D: all 80 source bytes. 40297F..4029DC then replace
    // position/target/up with the complete authored vectors, including W.
    const auto vector = [](Vec3 v, std::uint32_t w) { return CameraVector{v.x, v.y, v.z, std::bit_cast<float>(w)}; };
    s.position = vector(template_pose.position, template_pose.position_w);
    life.scale = vector(template_pose.scale, template_pose.scale_w);
    s.target = vector(template_pose.forward, template_pose.forward_w);
    s.up = vector(template_pose.up, template_pose.up_w);
    life.transform_word_48 = template_pose.tail[0];
    s.world_to_camera_reference = template_pose.tail[1];
    s.camera_to_world_reference = template_pose.tail[2];
    life.transform_word = template_pose.tail[3];
    // References are copied identities. This stage does not allocate, resolve
    // or rebuild matrix caches; those writes belong to later source consumers.
    s.position = input.position;
    s.target = input.target;
    s.up = input.up;
    s.lens = input.lens;
    life.base_lens = input.lens;
    s.twist = input.twist;
    camera_clear_velocity(s);
    camera_clear_acceleration(s);
    camera_rebuild_projection(s, shared_model_identity);
    if (life.pending_mode == 4)
        xyz(s.target, f32(double(vehicle.position.x) + vehicle.target_offset.x),
            f32(double(vehicle.position.y) + vehicle.target_offset.y),
            f32(double(vehicle.position.z) + vehicle.target_offset.z));
    life.pending_record.reset();
}
std::uint32_t camera_select_authored(const CameraLifecycle &life, const CameraVehicle &vehicle, const CameraBank &bank)
{
    (void)record(bank, 0);
    if (bank.count == 1 || vehicle.active != 1 || !life.current_record)
        return 0;
    const auto current = *life.current_record;
    const auto &a = record(bank, current);
    const auto next = current + 1 >= bank.count ? 0 : current + 1;
    const auto &b = record(bank, next);
    return squared_distance(b.position, vehicle.position) < squared_distance(a.position, vehicle.position) ? next
                                                                                                           : current;
}
void camera_authored_lens(CameraState &s, const CameraLifecycle &life, const CameraVehicle &vehicle,
                          const CameraBank &bank)
{
    if (life.clip_far < 1)
        return;
    if (!life.current_record)
        throw std::invalid_argument("authored lens requires a current camera record");
    const auto &input = record(bank, *life.current_record);
    if (input.tracking == 0)
    {
        camera_request_lens(s, input.lens);
        return;
    }
    if (input.tracking != 1 && input.tracking != 2)
        return;
    const double distance = std::sqrt(squared_distance(input.position, vehicle.position));
    const double fraction =
        (distance * double(std::bit_cast<float>(0x37d1b717u))) * (input.tracking == 1 ? 10.0 : 2.75);
    camera_request_lens(s, f32(std::min(fraction, 1.0) * input.lens));
}
void camera_prepare_authored(CameraState &s, CameraLifecycle &life, CameraVehicle &vehicle, const CameraBank &bank)
{
    if (s.mode != 10)
        throw std::invalid_argument("authored preparation requires mode10");
    const auto chosen = camera_select_authored(life, vehicle, bank);
    if (!life.current_record || chosen != *life.current_record)
    {
        xyz(vehicle.target_offset, 0, 0, 0);
        camera_request_record(life, chosen, 10);
        life.current_record = chosen;
    }
    if (record(bank, chosen).tracking != 3)
        camera_track_target(s, vehicle);
    camera_authored_lens(s, life, vehicle, bank);
}
void camera_initialize_values(CameraState &s, CameraLifecycle &life, CameraMatrix &shared_model_identity)
{
    s.lens = std::bit_cast<float>(0x404ccccdu);
    s.near_clip = 1;
    s.far_factor = 128;
    life.clip_near = 100;
    life.clip_far = 400000;
    s.half_width = std::bit_cast<float>(0x3faa9fbeu);
    s.half_height = 1;
    xyz(s.position, 0, 0, 0);
    xyz(s.target, 0, 0, 1000);
    xyz(s.up, 0, 1, 0);
    xyz(life.scale, 1, 1, 1);
    xyz(s.stabilized_up, 0, 1, 0);
    camera_rebuild_projection(s, shared_model_identity);
    s.mode = 1;
    s.control_angles = {0x1000000u, 0, 0};
    s.twist = 0;
    life.base_lens = 1;
    camera_clear_velocity(s);
    camera_clear_acceleration(s);
    life.current_record = 0;
    life.next_mode = 10;
    s.pending_follow_setup = 1;
    s.settle_ticks = 0;
    s.speed_lift = 0;
    s.height_offset = 0;
    life.unknown_1c8 = 0;
}
void camera_tick(CameraRig &rig, std::array<CameraVehicle, 6> &vehicles, const std::array<CameraRaceContext, 6> &race,
                 CameraVector gravity, const CameraBank &bank, const ModelTransform &template_pose,
                 std::uint32_t &selected, CameraModeReset reset, CameraCollision collision,
                 CameraMatrix &shared_model_identity)
{
    // Host safety checks do not alter valid original execution order.
    (void)record(bank, 0);
    if (rig.current_vehicle_slot >= vehicles.size())
        throw std::invalid_argument("global camera vehicle slot outside range");
    if (!rig.current_slot)
        throw std::invalid_argument("camera rig has no current-slot selector");
    for (const auto &slot : rig.slots)
        if (slot.active == 1)
        {
            if (!slot.state || !slot.lifecycle)
                throw std::invalid_argument("active camera slot lacks state");
            if (slot.vehicle_slot >= vehicles.size())
                throw std::invalid_argument("active camera has no vehicle slot");
            const auto mode = slot.lifecycle->pending_record ? slot.lifecycle->pending_mode : slot.state->mode;
            if (mode == 6 || mode == 7)
                throw std::invalid_argument("mounted/spline camera requires additional world inputs");
            if (slot.lifecycle->pending_record)
                (void)record(bank, *slot.lifecycle->pending_record);
            if (slot.lifecycle->current_record)
                (void)record(bank, *slot.lifecycle->current_record);
        }
    for (std::uint32_t index = 0; index < rig.slots.size(); ++index)
    {
        *rig.current_slot = index;
        auto &slot = rig.slots[index];
        if (slot.active != 1)
            continue;
        auto &s = *slot.state;
        auto &v = vehicles[slot.vehicle_slot];
        camera_apply_record(s, *slot.lifecycle, v, bank, template_pose, shared_model_identity);
        if (s.mode == 10)
            camera_prepare_authored(s, *slot.lifecycle, v, bank);
        else
            camera_prepare_follow_mode(s, v, selected, reset, shared_model_identity);
        camera_consume_lens(s, shared_model_identity);
        if (s.mode == 4 || s.mode == 5 || (s.mode >= 11 && s.mode <= 16))
            camera_chase_tick(s, v, race[rig.current_vehicle_slot], gravity, collision, shared_model_identity,
                              &vehicles[rig.current_vehicle_slot]);
        else
        {
            s.previous_target = s.target;
            camera_move(s, v, collision);
            camera_translate(s, v);
        } // 004051aa default arm
        camera_integrate_velocity(s);
        camera_integrate_angles(s);
    }
    *rig.current_slot = 0;
}
CameraRig camera_controls_rig(contact::player::Controls &controls, std::array<CameraLifecycle, 4> &life,
                              std::uint32_t current_vehicle_slot)
{
    CameraRig rig;
    rig.current_slot = &controls.current;
    rig.current_vehicle_slot = current_vehicle_slot;
    for (std::size_t i = 0; i < rig.slots.size(); ++i)
    {
        auto &controller = controls.controllers[i];
        rig.slots[i] = {&controller.camera, &life[i], static_cast<std::uint32_t>(controller.active),
                        controller.vehicle};
    }
    return rig;
}
} // namespace sgn::retail
