#include "sgn/playable_camera.hpp"
#include <limits>

namespace sgn::retail
{
void playable_camera_begin_tick(RaceSession &session)
{
    auto world = camera_frame_world(session);
    camera_frame_begin_outer(world);
}
CameraFrameSnapshot playable_camera_after_tick(RaceSession &session, CameraFrameStages &stages)
{
    // 405210 restores the original current-controller pointer to slot zero.
    // The frontend waits for that first actual tick before publishing a frame.
    const auto &s = session.state();
    if (s.controls.current >= s.controls.controllers.size() || s.controls.controllers[s.controls.current].active != 1)
        throw FormatError("playable view requires the actual active controller after world tick");
    auto world = camera_frame_world(session);
    return camera_frame_normal_views(world, {}, stages, nullptr);
}
sgn::View playable_camera_view(const CameraFrameSnapshot &snapshot)
{
    const auto &camera = snapshot.camera();
    for (float value : snapshot.world_to_view())
        if (!std::isfinite(value))
            throw FormatError("playable camera has nonfinite recovered matrix");
    const float clip_near = snapshot.near_clip(), clip_far = snapshot.far_clip();
    if (!std::isfinite(clip_near) || !std::isfinite(clip_far) || clip_near < 32 || clip_far <= clip_near)
        throw FormatError("playable camera has invalid recovered clip range");
    sgn::View view;
    view.eye = {camera.position.x, camera.position.y, camera.position.z};
    view.target = {camera.target.x, camera.target.y, camera.target.z};
    view.up = {camera.up.x, camera.up.y, camera.up.z};
    view.near_plane = clip_near;
    view.far_plane = clip_far;
    view.world_to_view = snapshot.world_to_view();
    view.retail_viewport = sgn::View::RetailViewport{
        {camera.projection[0], camera.projection[5], camera.projection[10], camera.projection[14], clip_near},
        clip_far,
        true};
    return view;
}
void playable_camera_resize(RaceSession &session, unsigned width, unsigned height)
{
    if (!width || !height || width > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max()) ||
        height > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max()))
        throw FormatError("playable viewport dimensions");
    auto &s = session.state();
    auto viewport = s.viewport;
    auto presentation_split = s.split_setting;
    const auto requested = viewport.requested_selector;
    viewport.width = static_cast<std::int32_t>(width);
    viewport.height = static_cast<std::int32_t>(height);
    initialize_viewport(viewport, presentation_split);
    // Window resizing is new platform behavior, not424AA0's game-mode reset.
    // Preserve the Session split selection and reapply its viewport request.
    viewport.requested_selector = requested ? requested : 1;
    apply_requested_viewport(viewport);
    update_viewport_ui_scale(viewport);
    s.viewport = viewport;
}
} // namespace sgn::retail
