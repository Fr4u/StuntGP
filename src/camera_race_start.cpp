#include "sgn/camera_race_start.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
void valid_current(const CameraRaceStartView &view)
{
    if (view.controls.current >= 4)
        throw std::out_of_range("race camera current controller");
}
void valid_vehicle(std::uint32_t slot)
{
    if (slot >= 6)
        throw std::out_of_range("race camera vehicle slot");
}
} // namespace
void activate_race_cameras(CameraRaceStartView view, std::span<const std::uint32_t, 4> selected)
{
    const unsigned count =
        view.split_mode > 4 ? 0 : (view.split_mode == 0 ? 1 : (view.split_mode <= 2 ? 2 : view.split_mode));
    if (count)
        valid_current(view);
    for (unsigned i = 0; i < count; ++i)
        valid_vehicle(selected[i]);
    for (auto &controller : view.controls.controllers)
        controller.active = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        auto &controller = view.controls.controllers[i];
        controller.vehicle = selected[i];
        controller.active = 1;
        camera_restore_preset(view.controls.controllers[view.controls.current].camera, view.vehicles[selected[i]],
                              view.selected_preset, view.split_mode, view.effect_reset, view.shared_model_identity);
    }
    contact::player::restore_primary(view.controls);
}
void place_race_camera_position(CameraState &s, const CameraVehicle &v)
{
    const float third = std::bit_cast<float>(0x3eaaaaabu), tenth = std::bit_cast<float>(0x3dcccccdu);
    const float target_y = f32(double(v.target_offset.y) + v.position.y),
                target_z = f32(double(v.target_offset.z) + v.position.z);
    s.target.x = f32(((double(v.target_offset.x) + v.position.x) - s.target.x) * third + s.target.x);
    s.target.y = f32(double(f32(double(f32(double(target_y) - s.target.y)) * third)) + s.target.y);
    s.target.z = f32(double(f32(double(f32(double(target_z) - s.target.z)) * third)) + s.target.z);
    const double up_x = (double(s.stabilized_up.x) - v.up.x) * tenth;
    const float up_y = f32(double(f32(double(s.stabilized_up.y) - v.up.y)) * tenth);
    const float up_z = f32(double(f32(double(s.stabilized_up.z) - v.up.z)) * tenth);
    s.stabilized_up.x = f32(double(s.stabilized_up.x) - up_x);
    s.stabilized_up.y = f32(double(s.stabilized_up.y) - up_y);
    s.stabilized_up.z = f32(double(s.stabilized_up.z) - up_z);
    const auto up = s.stabilized_up;
    const double projection = (double(up.z) * v.forward.z + double(up.y) * v.forward.y) + double(up.x) * v.forward.x;
    CameraVector forward{f32(double(v.forward.x) - f32(projection * up.x)),
                         f32(double(v.forward.y) - f32(projection * up.y)),
                         f32(double(v.forward.z) - projection * up.z), 0};
    camera_normalize(forward);
    CameraVector right{f32(double(forward.y) * up.z - double(forward.z) * up.y),
                       f32(double(forward.z) * up.x - double(forward.x) * up.z),
                       f32(double(forward.x) * up.y - double(forward.y) * up.x), 0};
    camera_normalize(right);
    const double rx = double(v.orbit_offset.x) * right.x, ux = double(v.orbit_offset.y) * up.x;
    const float ry = f32(double(v.orbit_offset.x) * right.y), rz = f32(double(v.orbit_offset.x) * right.z);
    const float uy = f32(double(v.orbit_offset.y) * up.y), uz = f32(double(v.orbit_offset.y) * up.z);
    const float fx = f32(double(v.orbit_offset.z) * forward.x), fy = f32(double(v.orbit_offset.z) * forward.y),
                fz = f32(double(v.orbit_offset.z) * forward.z);
    s.position.x = f32(-((ux + rx) + fx) + v.position.x);
    s.position.y = f32(-double(f32(double(fy) + f32(double(uy) + ry))) + v.position.y);
    s.position.z = f32(-double(f32(double(fz) + f32(double(uz) + rz))) + v.position.z);
    double height = double(v.height) - 115.0;
    if (height < 0)
        height = 0;
    const float hx = f32(height * v.up.x), hy = f32(height * v.up.y);
    const double hz = height * v.up.z;
    s.position.x = f32(double(s.position.x) - hx);
    s.position.z = f32(double(s.position.z) - hz);
    s.position.y = f32(((double(s.position.y) - hy) - s.height_offset) + s.speed_lift);
}
bool initialize_race_camera(CameraRaceStartView view, std::uint32_t preset_vehicle)
{
    valid_vehicle(preset_vehicle);
    if (view.vehicles[preset_vehicle].active == 0)
        return false;
    valid_current(view);
    auto &controller = view.controls.controllers[view.controls.current];
    auto &camera = controller.camera;
    if (camera.mode != 5)
        return false;
    valid_vehicle(controller.vehicle);
    camera_restore_preset(camera, view.vehicles[preset_vehicle], view.selected_preset, view.split_mode,
                          view.effect_reset, view.shared_model_identity);
    const auto &target = view.vehicles[controller.vehicle];
    place_race_camera_position(camera, target);
    camera.target.x = f32(double(target.target_offset.x) + target.position.x);
    camera.target.y = f32(double(target.target_offset.y) + target.position.y);
    camera.target.z = f32(double(target.target_offset.z) + target.position.z);
    camera.velocity.x = 0;
    camera.velocity.y = 0;
    camera.velocity.z = 0;
    return true;
}
} // namespace sgn::retail
