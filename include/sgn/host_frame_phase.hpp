#pragma once
#include "sgn/host_presentation.hpp"
#include <cstdint>
#include <memory>
#include <optional>

// NEW HOST (R28) frame phase markers. Not recovered game behaviour: the 2001
// renderer drew the 3D view and the 2D overlay into one surface. The host
// marks the region transitions of the canonical race draw (the
// host_presentation::RegionScope tags of source_core_race_draw.cpp) in the
// captured polygon stream, so that the renderer can split a presented frame
// at the 3D -> 2D boundary: post effects (FXAA, bloom) then touch the 3D scene
// only and the HUD is drawn afterwards, unaffected.
//
// A marker is a PolygonGpuCommand without a planned draw. Recorded without an
// active MarkerRecorder it emits no GPU command at all, so a stream with
// markers rasterizes exactly like the same stream without them (the recorder
// only closes its draw batch at the marker; D3D12 keeps the primitive order).
// Markers are inserted only while a post effect is enabled in
// host_graphics::Settings (bloom or FXAA); with both off no frame carries one.
// Owner thread only (the single source draw / presentation thread).
namespace sgn
{
class PolygonGpuCommand;
struct PolygonGpuInsertion;
struct PolygonGpuRecordContext;
struct CapturedPolygonFrame;
} // namespace sgn

namespace sgn::host_frame_phase
{
enum class Marker : std::uint8_t
{
    world_begin,  // host_presentation::Region::world3d entered (4425D0 world submission 44260B)
    world_end,    // world3d left: the 3D -> 2D split point of one race view
    screen_begin, // Region::screen3d entered (432F60 screen polygons: screen trails, flares)
    screen_end    // screen3d left
};

// ---- capture side (SourceMenuPresentationBackend) ----
// Inserts `command` at the current position of the active capture (for the
// game: PolygonDeviceContext::insert_gpu_command). May throw; a failure only
// leaves that marker out (the frame then falls back to the single pass).
using Inserter = void (*)(void *owner, std::shared_ptr<const PolygonGpuCommand> command);
// Between BeginScene and EndScene of one source frame. Reads the post-effect
// settings once per settings revision; markers are inserted only when wanted.
void begin_capture(void *owner, Inserter) noexcept;
void end_capture() noexcept;
// host_presentation::RegionObserver: region transition of the canonical race draw.
void region_changed(host_presentation::Region from, host_presentation::Region to) noexcept;

// ---- stream inspection ----
std::optional<Marker> marker_of(const PolygonGpuInsertion &) noexcept;
// True when the frame holds a GPU insertion that is not a phase marker (for
// example a gpu_world draw). Markers alone keep a frame interpolatable.
bool has_foreign_insertions(const CapturedPolygonFrame &) noexcept;

// ---- renderer side ----
class MarkerRecorder
{
  public:
    // Called at the marker's exact stream position while the polygon recorder
    // emits the frame. The same contract as a PolygonGpuCommand: it records
    // into ctx.list and must leave the polygon render/depth targets bound.
    virtual void record_marker(Marker, const PolygonGpuRecordContext &) = 0;

  protected:
    ~MarkerRecorder() = default;
};
// The recorder markers reach while a frame is recorded (nullptr: none).
void set_active_recorder(MarkerRecorder *) noexcept;
MarkerRecorder *active_recorder() noexcept;
} // namespace sgn::host_frame_phase
