#pragma once
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/renderer.hpp"

namespace sgn::retail
{
// Modern presentation bridge. The world tick already runs405210; these calls
// must never dispatch that camera tick a second time.
void playable_camera_begin_tick(RaceSession &); // 442815, before the world tick
CameraFrameSnapshot playable_camera_after_tick(RaceSession &, CameraFrameStages &);
// Converts an immutable recovered camera result, with explicit retail view and
// projection coefficients. Renderer never falls back to a guessed LookAt.
sgn::View playable_camera_view(const CameraFrameSnapshot &);
// Window changes affect viewport presentation only, not gameplay or camera ticks.
void playable_camera_resize(RaceSession &, unsigned width, unsigned height);
} // namespace sgn::retail
