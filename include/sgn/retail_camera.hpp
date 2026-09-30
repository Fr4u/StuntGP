#pragma once
#include <array>
#include <cstdint>

namespace sgn::retail
{
// Four-component storage preserves copied retail padding; arithmetic uses xyz.
// These structures are native values, never views into guest memory.
struct CameraVector
{
    float x{}, y{}, z{}, w{};
};
using CameraMatrix = std::array<float, 16>;
// Exact44EE10 writer. Shared by model and camera code without a second owner.
inline void initialize_model_matrix_identity(CameraMatrix &matrix) noexcept
{
    matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
struct CameraState
{
    std::uint32_t mode{};
    CameraVector position{}, target{}, previous_target{}, up{}, side{}, velocity{}, drift{}, stabilized_up{};
    float twist{}, lens{}, near_clip{}, far_factor{}, half_width{}, half_height{};
    std::uint32_t collision_bypass_ticks{}, settle_ticks{}, pending_lens{};
    float speed_lift{}, height_offset{}, requested_lens{};
    CameraMatrix projection{};
    CameraMatrix camera_to_world_rotation{}, world_to_camera_rotation{};
    // Native encodings of source camera+50/+4C. The Session arena owns the
    // matrices; the value arrays above are derived numerical/snapshot storage.
    std::uint32_t camera_to_world_reference{}, world_to_camera_reference{};
    float inverse_depth_scale{}, depth_offset{};
    // 00403080 / 00402f60 retain independent position and angle integrators.
    float linear_speed{};
    std::array<std::uint32_t, 3> control_angles{};
    CameraVector angular_velocity{}, angular_acceleration{};
    std::uint32_t pending_follow_setup{};
};
struct CameraVehicle
{
    // 0x71ec is the contact counter, not the airborne timer at 0x71fc.
    // Camera comparisons interpret its bits as unsigned, including sentinel values.
    std::uint32_t active{}, adaptive_distance{}, contact_class{}, contact_counter{};
    CameraVector position{}, velocity{}, right{}, up{}, forward{};
    CameraMatrix orientation{};
    CameraVector orbit_offset{}, target_offset{}, angles{}, shift{}, previous_shift{};
    float distance{}, approach_scale{}, speed{}, progress{}, height{}, contact_height{};
    std::uint32_t camera_mode{};
};
struct CameraRaceContext
{
    // 00404360 selects the next ranked car, or the current car if last.
    float next_progress{}, next_speed{}, track_length{};
};
struct CameraSweep
{
    CameraVector position{}, velocity{};
    float speed{}, radius{125.0f}, radius_squared{15625.0f}, inverse_mass{1.0f};
};
// The solver implements 004215b0/00421c30 including the original 33-hit bound.
// Missing collision reconstruction is an error, never an invented no-hit path.
struct CameraCollision
{
    void *context{};
    CameraVector (*solve)(void *, const CameraSweep &){};
};
// 00412910 visits six slots in order and calls 004128f0 only for active==1.
// The vehicle module owns those eight effect fields. The callback must perform
// their actual reset; an omitted callback is rejected if any slot is active.
struct CameraModeReset
{
    void *context{};
    std::array<std::uint32_t, 6> active{};
    void (*reset)(void *, std::uint32_t slot) noexcept {};
};

CameraMatrix camera_rotation(std::int32_t x, std::int32_t y, std::int32_t z); // 0044fb00
// Optional test-only observation of the last44FB00 write to61C3A4; never read
// back by this function. The matrix persists until another source writer runs.
CameraVector camera_rotate_degrees(CameraVector angles, CameraVector value,
                                   CameraMatrix *audit_rotation = nullptr); // 0044ec00
void camera_normalize(CameraVector &);                                      // 0044ed40
void camera_orbit_offset(CameraVehicle &); // 004032a0, xyz only; w is uninitialized retail stack data
void camera_projection(CameraState &);     // 0044f4e0, symmetric retail frustum only
// 0044F4C0: projection first, then44F4D4/44F4D9 into the mandatory sole61C468 owner.
// No cached/default matrix and no delayed parent publication.
void camera_rebuild_projection(CameraState &, CameraMatrix &shared_model_identity);
void camera_set_lens(CameraState &, float, CameraMatrix &shared_model_identity);                   // 00403310
void camera_translate(CameraState &, const CameraVehicle &);                                       // 00403340
void camera_twist(CameraState &);                                                                  // 00403420
void camera_settle(CameraState &, std::uint32_t increment);                                        // 00403650
void camera_track_target(CameraState &, const CameraVehicle &);                                    // 00403770
void camera_speed_lens(CameraState &, const CameraVehicle &, CameraMatrix &shared_model_identity); // 00403bc0
void camera_orbit_angle(CameraVehicle &, float increment);                                         // 004031e0
void camera_request_lens(CameraState &, float);                                                    // 00402ab0
void camera_consume_lens(CameraState &, CameraMatrix &shared_model_identity);                      // 00402a80
void camera_integrate_velocity(CameraState &);                                                     // 00403080
void camera_integrate_angles(CameraState &);                                                       // 00402f60
void camera_select_mode(CameraState &, std::uint32_t mode, CameraModeReset); // 004028d0 / 00412910
// Presets 1,2,3,4 correspond to 0040d280,0040d200,0040d300,0040d380.
// selected_preset represents DAT_0060f020, not an invented per-car preference.
void camera_apply_preset(CameraState &, CameraVehicle &, std::uint32_t preset, std::uint32_t &selected_preset,
                         CameraModeReset, CameraMatrix &shared_model_identity);
void camera_restore_preset(CameraState &, CameraVehicle &, std::uint32_t &selected_preset, std::uint32_t force_close,
                           CameraModeReset, CameraMatrix &shared_model_identity); // 00404310
// 004039b0 for modes 11..16, including the original repeated mode reset.
// Other modes are no-ops, except mode 10, whose authored camera input is required.
void camera_prepare_follow_mode(CameraState &, CameraVehicle &, std::uint32_t &selected_preset, CameraModeReset,
                                CameraMatrix &shared_model_identity);
void camera_finish_orbit(CameraState &, CameraVehicle &);                  // 00403e80
void camera_adaptive_distance(CameraVehicle &, const CameraRaceContext &); // 00404360
void camera_move(CameraState &, const CameraVehicle &, CameraCollision);   // 00403460
void camera_view_basis(CameraState &); // 00450190, values of the two allocated matrices
// 00404450 camera modes 4, 5, and 11..16. Mode 6 (model-mounted) and mode 7
// (spline) require distinct inputs and are rejected by this chase-camera API.
// adaptive_vehicle is the original global current vehicle (004d5bb8). It may
// differ from this camera's target in a multi-camera dispatcher. Null preserves
// the explicit same-vehicle case used by single-camera callers and core tests.
void camera_chase_tick(CameraState &, CameraVehicle &, const CameraRaceContext &, CameraVector gravity_direction,
                       CameraCollision, CameraMatrix &shared_model_identity, CameraVehicle *adaptive_vehicle = nullptr);
} // namespace sgn::retail
