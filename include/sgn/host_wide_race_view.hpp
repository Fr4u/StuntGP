#pragma once
#include "sgn/host_wide_layout.hpp"
#include "sgn/retail_camera.hpp"
#include <array>
#include <cstdint>

namespace sgn::retail
{
struct RaceSessionState;
}

// NEW HOST presentation (R20 wide_3d): Hor+ race 3D view. Not recovered game
// behaviour and never read by the simulation.
//
// Mechanism: the original projection maps a camera-space point to
// x_screen = (P.x*x)/(z+1)*scale_x + center_x and clips/culls against
// |P.x*x| <= z (4277D0 planes 4/8, 429520 masks), the side frustum planes of
// 402ECC (normals from lens and half_width 0x3FAA9FBE) and the 576C04 screen
// clip. Hor+ with factor k (= canvas width / 640) is obtained, without
// touching the clip/cull code, by using P.x/k and half_width*k for clipping
// and culling and scale_x*k (about the canvas centre) for the final 429A40
// mapping. For the single view the screen position of every originally
// visible point is unchanged (P.x/k*scale_x*k, up to binary32 rounding);
// the horizontal extent grows to the canvas. Vertical FOV (P.y, planes 2/3,
// scale_y) is untouched (portrait outputs use the symmetric Vert+ rule).
//
// Ownership/safety: a RaceViewScope lives inside one 4425D0 view draw. After
// each 402E90 camera publication (4425FC and 42E893) race_camera_published()
// saves the published 61C3E8/61C3FC words and the current controller's four
// planes, then stores the widened values. restore() (end of the 3D stages,
// before the 2D HUD at 442610, and the destructor) puts the saved words back
// when they still hold the widened values, so after the draw every owner holds
// exactly the value the original draw leaves for the next tick. With the
// original 4:3 presentation (or an exactly 4:3 output) the scope is inactive
// and every hook is a no-op: output is byte-identical to the base.
// Owner (draw) thread only; scopes do not nest across states.
//
// NEW HOST (R23) interpolation GUARD BAND. The high-refresh presenter
// (host_presentation_presenter.hpp) re-projects the older canonical capture to
// a later camera pose. A capture culled and clipped exactly at the visible
// canvas edge cannot fill the strip a turning/pitching/rolling camera uncovers
// (background at the sides and corners for up to one tick). While a guard
// angle is set (set_race_guard_band, by the presenter while it is in use) and
// the race view is the single full frame (split 0, selector 1, not cropped,
// full 2D clip), published() multiplies the Hor+ factors by a per-axis guard
// factor g = tan(atan(T)+angle)/T, T = the view's half-extent tangent
// (k/P.x, k_y/P.y). This is exactly the Hor+ mechanism with a larger k: the
// clip/cull words, frustum planes (all four), 429A40 mapping, gpu_world
// constants and the 4255E0 screen clip rectangle all see the guarded view, so
// geometry up to `angle` beyond every canvas edge exists in the capture, while
// the screen position of every visible point stays the same (P/(k*g) *
// scale*k*g, one binary32 rounding). The GPU viewport clips the rest.
// restore() puts every word back before the 2D stages exactly as for Hor+, and
// race_original_camera() keeps its original words, so a model visible only in
// the guard band keeps its PMD LOD words (42AF30/42AF90) like a Hor+-only one.
// Guard 0 (the default; interpolation off) leaves the R20 behaviour unchanged.
namespace sgn::host_wide
{

// Original (4:3) camera words saved by the active scope for the current
// publication, or nullptr when nothing is widened. Used where the draw writes
// a word the tick later reads (PMD LOD words +5C/+64 in 42AF30/42AF90): a
// model visible ONLY because of Hor+ keeps its original LOD words.
struct OriginalRaceCamera
{
    const std::array<retail::CameraVector, 4> *planes{};
    float projection_x{}, projection_y{};
};

class RaceViewScope
{
  public:
    explicit RaceViewScope(retail::RaceSessionState &) noexcept;
    ~RaceViewScope();
    RaceViewScope(const RaceViewScope &) = delete;
    RaceViewScope &operator=(const RaceViewScope &) = delete;
    void restore() noexcept; // idempotent; ends widening for this view
    bool active() const noexcept
    {
        return view_.active && !ended_;
    }
    const WideView &view() const noexcept
    {
        return view_;
    }
    // Hook body, see race_camera_published().
    void published(retail::RaceSessionState &) noexcept;
    const OriginalRaceCamera *original() const noexcept
    {
        return active() && saved_.valid ? &original_ : nullptr;
    }

  private:
    struct Saved
    {
        bool valid{};
        unsigned controller{};
        float projection_x{}, projection_y{}, written_x{}, written_y{};
        std::array<retail::CameraVector, 4> planes{}, written_planes{};
    };
    retail::RaceSessionState *state_{};
    RaceViewScope *previous_{};
    WideView view_{};
    Saved saved_{};
    OriginalRaceCamera original_{};
    bool ended_{};
};

// Called right after every 402E90 (prepare_source_camera_view_402e90) in the
// race draw. No-op unless an active RaceViewScope owns this state.
void race_camera_published(retail::RaceSessionState &) noexcept;
// The active widened race view (3D stages of one 4425D0), or nullptr.
const WideView *race_view() noexcept;
// See OriginalRaceCamera.
const OriginalRaceCamera *race_original_camera() noexcept;

// Planes of 402ECC (full source formula, retail_camera_initialization.cpp
// planes()) recomputed with half_width*factor_x / half_height*factor_y.
std::array<retail::CameraVector, 4> widened_camera_planes(const retail::CameraState &, float factor_x, float factor_y);

// NEW HOST (R23) guard band, see above. Angle in radians beyond each canvas
// edge; <= 0 or nonfinite disables it (angles are capped at max_guard_radians).
// Any thread may set it; a race view reads it once per 402E90 publication.
inline constexpr double max_guard_radians = 0.2; // ~11.5 degrees
inline constexpr float max_guard_factor = 1.5f;  // per axis, relative to the Hor+ extent
void set_race_guard_band(double radians) noexcept;
double race_guard_band() noexcept;
// Pure: per-axis factor tan(atan(t)+radians)/t in [1, max_guard_factor];
// 1 for a nonpositive/nonfinite tangent or angle.
float guard_band_factor(double half_extent_tangent, double radians) noexcept;
// True when the guard may apply to this live race state: single view (split 0),
// full-frame selector 1, not cropped, full 0..640 x 0..480 2D clip.
bool guard_band_applicable(const retail::RaceSessionState &) noexcept;

} // namespace sgn::host_wide
