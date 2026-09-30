#pragma once
#include "sgn/retail_camera.hpp"
#include <optional>
#include <string_view>

namespace sgn::retail
{
struct ModelTransform;
namespace contact::player
{
struct Controls;
}
struct AuthoredCamera
{
    CameraVector position{}, target{}, up{};
    float lens{};
    std::uint32_t tracking{};
    float twist{};
    std::uint32_t opaque{};
};
static_assert(sizeof(AuthoredCamera) == 64);
struct CameraBank
{
    std::array<AuthoredCamera, 32> records{};
    std::uint32_t count{};
};
// Bounded host parser for the named original CAM schema (00401f00). Existing
// opaque/fourth lanes are preserved, as in the original loader's xyz writes.
void camera_load_authored_text(CameraBank &, std::string_view);

// Native record indices replace guest addresses. The fixed 32-slot bank keeps
// slot zero available even when the original count is zero.
struct CameraLifecycle
{
    CameraVector scale{};
    std::uint32_t transform_word_48{}; // camera +0x48, opaque template word
    std::uint32_t transform_word{};    // camera +0x54, copied opaque template word
    float clip_near{}, clip_far{}, base_lens{};
    std::uint32_t unknown_1c8{}, next_mode{};
    std::optional<std::uint32_t> current_record, pending_record;
    std::uint32_t pending_mode{};
};
void camera_clear_velocity(CameraState &) noexcept;     // 004023d0, xyz + speed only
void camera_clear_acceleration(CameraState &) noexcept; // 004023f0, both xyz vectors
void camera_request_record(CameraLifecycle &, std::optional<std::uint32_t>, std::uint32_t mode); // 004028f0
// 00402971..0040297D copies the live 4864E8 owner, including W and tail words.
void camera_apply_record(CameraState &, CameraLifecycle &, const CameraVehicle &, const CameraBank &,
                         const ModelTransform &, CameraMatrix &shared_model_identity);                    // 00402950
std::uint32_t camera_select_authored(const CameraLifecycle &, const CameraVehicle &, const CameraBank &); // 00403800
void camera_authored_lens(CameraState &, const CameraLifecycle &, const CameraVehicle &,
                          const CameraBank &);                                                       // 004038c0
void camera_prepare_authored(CameraState &, CameraLifecycle &, CameraVehicle &, const CameraBank &); // 004039b0 mode10
// Numeric/state portion 00402469..00402574. Asset/spline loading, four-slot
// duplication and global selection are separate, not silently implemented here.
void camera_initialize_values(CameraState &, CameraLifecycle &, CameraMatrix &shared_model_identity);

struct CameraSlot
{
    CameraState *state{};
    CameraLifecycle *lifecycle{};
    std::uint32_t active{}, vehicle_slot{};
};
struct CameraRig
{
    std::array<CameraSlot, 4> slots;
    std::uint32_t *current_slot{};
    std::uint32_t current_vehicle_slot{}; // DAT_004d5bb8, separate from each target
};
// This is a borrowed invocation view. States and the current-slot selector are
// owned by the player Controls object; the rig does not duplicate their state.
CameraRig camera_controls_rig(contact::player::Controls &, std::array<CameraLifecycle, 4> &,
                              std::uint32_t current_vehicle_slot);
// Complete 00405210 dispatcher for all modes except mounted6/spline7. Those
// modes require extra world inputs and are rejected before mutation. The
// physics/race owner supplies observations captured before the world tick.
void camera_tick(CameraRig &, std::array<CameraVehicle, 6> &, const std::array<CameraRaceContext, 6> &,
                 CameraVector gravity, const CameraBank &, const ModelTransform &, std::uint32_t &selected_preset,
                 CameraModeReset, CameraCollision, CameraMatrix &shared_model_identity);
} // namespace sgn::retail
