#pragma once
#include "sgn/retail_camera.hpp"
#include "sgn/track_collision.hpp"

namespace sgn::retail
{
struct CameraSweepResult
{
    bool hit{};
    CameraVector position{}, normal{}, center{}, surface{};
    std::uint32_t polygon{0xffffffffu};
};
// Original camera query 004215b0. This deliberately does not call vehicle sweep.
CameraSweepResult camera_sweep(const contact::TrackGeometry &, const CameraSweep &);
// Original 00403460 collision branch: at most 33 sweeps, 0.01 reflection.
CameraVector camera_resolve_collision(const contact::TrackGeometry &, const CameraSweep &);
CameraCollision camera_collision_adapter(const contact::TrackGeometry &);
} // namespace sgn::retail
