#pragma once
#include "sgn/retail_camera_lifecycle.hpp"
#include "sgn/contact_player.hpp"
#include "sgn/camera_path.hpp"
#include "sgn/source_camera_spline.hpp"

namespace sgn::retail
{
struct ViewMatrixArena;
struct CameraSessionState
{
    std::uint32_t authored_cycle_index{};
    float path_speed{}, path_parameter{};
};
struct CameraPresentationState
{
    CameraSplineBinding spline;
    CameraVector up_axis{}, forward_axis{};
    std::array<CameraVector, 4> planes{};
};
struct CameraInitializationBindings
{
    contact::player::Controls &controls;
    std::array<CameraLifecycle, 4> &lifecycle;
    std::array<CameraPresentationState, 4> &presentation;
    CameraBank &bank;
    CameraSessionState &session;
    std::array<CameraVehicle, 6> &observed_vehicles;
    std::uint32_t &selected_preset;
    CameraModeReset effect_reset;
    CameraMatrix &shared_model_identity; // borrowed sole61C468, required
};
struct CameraLevelResources
{
    const AssetStore &assets;
    std::string_view track_name;
    std::span<const std::uint8_t> track_pmd;
    const SplineSet &splines;
};
// 401f00 selection/parser + 401e50 fallback. null text means missing, not bad.
// An empty fallback preserves the previous bank/count/cycle, as in the source.
void camera_prepare_resources(CameraBank &, CameraSessionState &, std::uint32_t scene_state,
                              std::optional<std::string_view> text, std::span<const std::uint8_t> pmd);
// 402427..402575 followed by the complete 4025d0 tail on represented fields.
void camera_initialize_controllers(CameraInitializationBindings, const SplineSet &, std::uint32_t split_setting);
void camera_initialize_level(CameraInitializationBindings, const CameraLevelResources &, std::uint32_t scene_state,
                             std::uint32_t split_setting);
void camera_begin_outer_frame(contact::player::Controls &); // 442815 -> 4023f0, selected slot only
// Mutating402E90. A supplied arena publishes full450190 pointer/arena state;
// null is the isolated numerical-view test API and is never used by Session.
// Each mode3 call samples AND advances path state;
// there is no hidden tick/render policy. Undefined original mode3 output W
// is not imported: native position/target W are retained, XYZ is qualified.
void camera_prepare_view(contact::player::Controls &, std::array<CameraLifecycle, 4> &,
                         std::array<CameraPresentationState, 4> &, CameraSessionState &, const SplineSet &,
                         CameraPathInput, CameraModeReset, CameraMatrix &shared_model_identity,
                         ViewMatrixArena *arena = nullptr);
// Borrowed publication of actual frame projection/depth globals, no new storage.
struct CameraProjectionPublication
{
    CameraMatrix &projection;
    float &inverse_depth_scale;
    float &depth_offset;
};
// Source menu path: no track/spline owner is needed outside mode3. Publishes
// 44F4C0 first, then rejects unresolved mode3 at its actual428800 boundary.
// Uses the same recovered view implementation and the supplied actual arena.
void camera_prepare_view_non_path(contact::player::Controls &, std::array<CameraLifecycle, 4> &,
                                  std::array<CameraPresentationState, 4> &, CameraSessionState &,
                                  CameraMatrix &shared_model_identity, ViewMatrixArena &, CameraProjectionPublication);
// Actual428800 zero-count path, borrowing the existing624498 owner. The count
// is read only after44F4C0 and only for mode3. Nonzero requires a real table walk.
void camera_prepare_view_source_zero_splines(contact::player::Controls &, std::array<CameraLifecycle, 4> &,
                                             std::array<CameraPresentationState, 4> &, CameraSessionState &,
                                             CameraMatrix &shared_model_identity, ViewMatrixArena &,
                                             CameraProjectionPublication, const std::uint32_t &source_spline_count,
                                             CameraModeReset);

} // namespace sgn::retail

namespace sgn::retail
{
// Shared reached numerical children. The source path uses the same existing
// orientation,450190 arena and402C80 plane implementations as the preview path.
void camera_free_orientation_402ad0(CameraState &);
void camera_finish_view_402ecc(CameraState &, CameraPresentationState &, ViewMatrixArena *);
} // namespace sgn::retail
