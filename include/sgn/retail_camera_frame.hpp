#pragma once
#include "sgn/retail_vehicle_storage.hpp"
#include "sgn/retail_light_effects.hpp"
#include "sgn/retail_world_globals.hpp"
#include "sgn/retail_world.hpp"
#include "sgn/retail_viewport.hpp"

namespace sgn::retail
{
class ModelResources;
// Invocation view over RaceSession's existing owners. No reference is retained.
struct CameraFrameWorld
{
    VehicleStorage &vehicles;
    contact::player::Controls &controls;
    std::array<CameraLifecycle, 4> &lifecycle;
    std::array<CameraPresentationState, 4> &presentation;
    const CameraBank &bank;
    const ModelTransform &template_pose; // required live4864E8 owner, never cached
    CameraSessionState &session;
    const SplineSet &splines;
    const contact::TrackGeometry &track;
    const WorldGravity &gravity;
    const RaceClockState &clock;
    const RaceRanking &ranking;
    unsigned &current_vehicle;
    std::uint32_t &selected_preset;
    const std::uint32_t &split_setting;
    std::array<ControllerOverlayTransition, 4> &overlays;
    contact::solver::Workspace &contact_workspace;
    contact::ContactCastState &contact_cast;
    PmdLightBank &lights;
    LightViewSelection &light_selection;
    LightFadeState &light_fade;
    LightViewAnimation &light_animation;
    std::span<std::uint8_t> light_vertices;
    std::uint32_t &light_vertex_offset;
    CameraMatrix &frame_projection;
    float &inverse_depth_scale;
    float &depth_offset;
    ViewportState &viewport;
    CameraMatrix &shared_model_identity; // 61C468, the final44F4C0 writer
    ViewMatrixArena &matrix_arena;       // sole61C7CC and6227CC/D0 owners
    ModelResources *models{};            // borrowed live26B0 owner; required only for mode6
};
// Derived render output only. GPU consumers receive this immutable view and
// have no reference back to the authoritative session or an advance function.
class CameraFrameSnapshot
{
    CameraState camera_;
    CameraMatrix world_to_view_;
    std::array<CameraVector, 4> planes_;
    std::uint32_t controller_{}, active_lights_{};
    float near_{}, far_{};

  public:
    CameraFrameSnapshot(const CameraState &, const CameraPresentationState &, const CameraLifecycle &,
                        std::uint32_t controller, std::uint32_t active_lights);
    const CameraState &camera() const noexcept
    {
        return camera_;
    }
    const CameraMatrix &world_to_view() const noexcept
    {
        return world_to_view_;
    }
    const std::array<CameraVector, 4> &planes() const noexcept
    {
        return planes_;
    }
    std::uint32_t controller() const noexcept
    {
        return controller_;
    }
    std::uint32_t active_lights() const noexcept
    {
        return active_lights_;
    }
    float near_clip() const noexcept
    {
        return near_;
    }
    float far_clip() const noexcept
    {
        return far_;
    }
};
// These required stages are owned by the world/renderer adapter. There is
// no default silent implementation and this module does not qualify them.
struct CameraFrameStages
{
    virtual ~CameraFrameStages() = default;
    virtual void stage_entities_442500(const CameraFrameSnapshot &) = 0;
    virtual void prepare_world_42e870() = 0; // 42E820,44FD60,425AD0 before second402E90
    // Capture source model consumers before42B0F6 reseeds their queues.
    // This boundary does not claim full polygon/clipping42B040/4277D0/429A40.
    virtual void consume_model_views(const CameraFrameSnapshot &) = 0;
};
void camera_frame_begin_outer(CameraFrameWorld &); // 442815, once per accepted outer frame
// Full supported405210 dispatch. Call only on the442841 branch, before world
// 418A90 and the intervening particle/TT stages. Mounted mode6 requires live model axes.
// Mode7 uses the bound authored spline and shared499F8C path parameter.
void camera_frame_before_world(CameraFrameWorld &);
// Sequential source composition with optional mounted-read interposition.
// Completed controllers and mounted prefixes stay in their existing owners on failure.
struct CameraMountedAudit;
void camera_frame_before_world(CameraFrameWorld &, CameraMountedAudit *);

// The NORMAL4425D0 route only:402E90,required442500,required42E870 prefix,
// viewport selector gate,402E90,42DCF0. The caller must resolve its separate transition-screen branch.
CameraFrameSnapshot camera_frame_normal_views(CameraFrameWorld &, CameraPathInput, CameraFrameStages &,
                                              LightGammaDevice *);
// Read-only derived capture; does not run402E90,405210,42DCF0 or path playback.
CameraFrameSnapshot camera_frame_snapshot(const CameraFrameWorld &);
// Explicit modern presentation policy, NOT evidence of original view cadence.
inline constexpr std::uint32_t modern_camera_source_frame_hz = 60;
} // namespace sgn::retail
