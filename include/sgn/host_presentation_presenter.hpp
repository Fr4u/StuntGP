#pragma once
#include "sgn/host_presentation.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace sgn
{
class Renderer;
class SourceMenuPresentationBackend;
} // namespace sgn
namespace sgn::retail
{
class FrontendState;
struct FrontendPerformanceCounter;
} // namespace sgn::retail

// NEW HOST high-refresh presentation (interp, R19). Not recovered behaviour.
//
// The source keeps running its unchanged pass (442810 update + 4427B0 draw +
// 44E6B0) once per due 60 Hz tick; that canonical draw produces the capture of
// frame N and its FrameTags (plain camera/vehicle snapshots + per-vertex
// groups). Between ticks this presenter shows the state interpolated between
// the previous canonical frame (N-1) and N:
//   state = now - 1.5 ticks (R22; R19: now - 1 tick), blended between the two
//   canonical frames around it (N-2..N kept; T = due time of the producing tick)
// i.e. the display runs 1.5 ticks behind the simulation and never extrapolates;
// the half tick covers the pass of the next tick (no frozen pose at 240 Hz).
// Each displayed frame is a copy of capture N-1 (R22; R19 used N) whose world
// vertices are un-projected with the exact 429A40/41E410 mapping and
// re-projected FORWARD with the interpolated camera and vehicle poses; 2D
// overlays are kept as captured. Forward re-projection keeps the capture's
// near-plane cut behind the camera; moving N backward exposed it as a gap at
// the bottom of the screen. At alpha 1 (and on a cut) capture N is shown as is.
// It is presented with Renderer::present_polygon_submission_transient, which
// does not touch the source front/back roles. While active, the Present of the
// canonical race Flip (4427C9 -> 432A60) is suppressed; the Flip itself (its
// source-visible surface operation and result) is unchanged.
//
// No source function runs here and no source owner is written: the simulation,
// its tick-coupled per-pass work and all its values are identical with the
// presenter on or off. Disabled -> the original 60 Hz presentation.
//
// NEW HOST (R23) screen edges and corners. A re-projected capture can only
// show what frame N-1 kept: (1) geometry culled/clipped at N-1's view edges
// is missing where a turning, pitching or rolling camera uncovers a strip
// (sides, top/bottom, and the corners under roll); (2) vertices that the
// forward re-projection moved behind the near plane (N-1's near-cut edge when
// driving fast, > ~50 units per tick) were clamped at w_min, producing
// stretched/bent triangles in the bottom corners. Fixes:
//  - GUARD BAND: while the presenter is in use it sets host_wide's race guard
//    angle (host_wide_race_view.hpp) for the next captures: culling, clipping
//    and the 2D screen clip extend beyond the canvas, the projection of every
//    visible point is unchanged, the GPU viewport clips the rest. The angle is
//    1.25 x the largest camera motion (rotation + translation parallax) of the
//    last 8 canonical transitions, clamped to [1.5, 6] degrees (3 before any
//    measurement). All source words are restored before the 2D stages.
//  - NEAR CLIP: triangles whose re-projected vertices cross the source near
//    plane (w = clip_near+1) are clipped there (one-plane Sutherland-Hodgman,
//    winding kept, attributes interpolated along the 3D edge); such a draw is
//    presented as an indexed triangle list. The w_min clamp remains only for
//    vertices no triangle uses and for draws that cannot be clipped.
// Live setting: host_graphics::Settings::interpolation is polled in
// observe_completed_frames() (between source passes, never inside a capture).
// The value present at construction is the baseline (presenter_options, i.e.
// command line / SGN_INTERP, decided the initial state); each later change
// enables (collection on, failure cleared) or disables (deactivate(),
// collection off, race Present no longer suppressed) the presenter. The next
// canonical race Flip then presents the canonical frame; its deferred image is
// rasterized first by RendererLoadingSurfaces (stale-image refresh).
namespace sgn::host_presentation
{
struct PresenterOptions
{
    bool enabled{true};
    // A camera or vehicle moving farther than this (world units) or turning more
    // than max_turn_radians between two canonical frames is a cut: shown as N.
    double cut_distance{2000.0};
    double max_turn_radians{1.0471975511965976};
    // NEW HOST (R30): host diagnostics (one line per distinct event, appended):
    // "host interp: disabled: <reason>" and held-back source frames. Empty: none.
    // presenter_options() defaults it to <exe dir>/reports/source-menu-session.log
    // when the executable lives in the game root (<exe dir>/data/retail exists).
    std::filesystem::path report_log{};
};
// --no-interpolation (command line) or SGN_INTERP=0 disables; SGN_INTERP=1 enables.
PresenterOptions presenter_options(bool command_line_enabled);

// NEW HOST (R27) frame rate limiter hook. present() calls open() after the back
// buffer of this Present is available and BEFORE it samples the display time
// (R22 order: buffer, then time), so a pacing wait inside open() delays the
// sampled pose and the Present together. open() returning false presents
// nothing (no failure; the caller decides what to do next).
class PresentGate
{
  public:
    virtual bool open() = 0;
    // NEW HOST (R30): the displayed state is sampled this many seconds before the
    // time sample (negative: after it). The pacing puts a vsync-paced present on
    // its display slot with it; 0 (default) keeps the R22 sample exactly.
    virtual double pose_delay_seconds() const noexcept
    {
        return 0.0;
    }

  protected:
    ~PresentGate() = default;
};

class InterpolatingPresenter
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    InterpolatingPresenter(retail::FrontendState &, SourceMenuPresentationBackend &,
                           const std::unique_ptr<Renderer> &renderer_slot, retail::FrontendPerformanceCounter &,
                           PresenterOptions);
    ~InterpolatingPresenter();
    InterpolatingPresenter(const InterpolatingPresenter &) = delete;
    InterpolatingPresenter &operator=(const InterpolatingPresenter &) = delete;
    // Pacing adapter, owner thread, only between source passes.
    void observe_completed_frames();     // new canonical frame -> history, activity, race Present suppression
    bool active() const noexcept;        // interpolated frames are being presented
    bool frame_waiting() const noexcept; // a canonical frame has not yet been shown
    bool wants_present() const noexcept; // active and a new or moving frame can be shown
    // Build + present one frame; false if nothing was presented. NEW HOST (R27):
    // an optional gate runs between the buffer wait and the time sample.
    bool present(PresentGate *gate = nullptr);
    void deactivate() noexcept;    // back to the original presentation (history dropped)
    bool enabled() const noexcept; // NEW HOST (R23): enabled by options/settings and not failed
    const std::string &last_failure() const noexcept;
};
} // namespace sgn::host_presentation
