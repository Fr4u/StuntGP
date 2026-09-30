#pragma once
#include "sgn/session_scene_frame.hpp"
#include "sgn/retail_demo_tail.hpp"
#include "sgn/retail_loading_frame.hpp"
#include "sgn/retail_pause_menu.hpp"
#include "sgn/retail_pause_age_fade.hpp"
#include "sgn/retail_race_hud_submission.hpp"
#include <array>
#include <optional>

namespace sgn::retail
{
// Synchronous borrowing contract for the reconstructed outer source frame.
// Implemented binding must use the same Session/material/scratch/device owners
// as SessionOuterFrameContext. A view expires when its synchronous child ends.
// Getters must neither reset source state nor publish/replace a resource.
struct SessionOuterFrameSources
{
    virtual ~SessionOuterFrameSources() = default;
    virtual SessionWorldFrameResources borrow_world_resources() = 0;
    virtual RaceHudSubmission borrow_countdown() = 0;
    virtual PauseAgeFadeWorld borrow_age_fade() = 0;
    virtual LoadingFrameWorld borrow_loading_frame() = 0;
    virtual DemoTailWords borrow_demo_words() = 0; // pure persistent references, no defaults
};

// Sole retained source words must be provided by the common platform owner.
// This view allocates no new words and supplies no inferred initial values.
struct SessionOuterDeviceWords
{
    std::uint32_t &render_counter; // 612984, INC before4421F0
    std::uint32_t &scene_active;   // 5DA1C8, shared with loading begin/end
    std::uint32_t &finalized;      // 6244EC, shared with loading Lock/Unlock
};
struct SessionOuterFrameContext
{
    RaceSession &session;
    const SessionWorldMaterials &materials;
    SessionPolygonCapabilities capabilities;
    PolygonSubmissionState &scratch;
    PolygonSubmissionSink &sink;
    RaceOverlayTextures &textures;
    PauseMenuRestartSource &restart;
    SessionOuterFrameSources &sources;
    SessionOuterDeviceWords device_words;
    LoadingFrameDevice &device;
};

// Host observations only. Command/resource capture belongs to Audio's ordered
// PolygonResourceSegment/CapturedPolygonSubmission contract, not this result.
struct SessionOuterSceneResult
{
    std::array<std::optional<SessionSceneFrameResult>, 4> views;
    std::uint32_t view_count{};
};
enum class SessionOuterFrameKind
{
    not_race,
    source_gate,
    rendered
};
struct SessionOuterFrameResult
{
    SessionOuterFrameKind kind;
    std::optional<SessionOuterSceneResult> scene;
};

// Full results and demo bodies are composed by the Session adapters. Drawing
// resources remain mandatory at the source child that accesses them.
void submit_session_results_449fb0(SessionOuterFrameContext &);
void submit_session_demo_40e030(SessionOuterFrameContext &);

// Literal source order. No simulation advance, input sampling, capture seal,
// presentation, resource registry, scheduler or inferred end-of-frame restore.
SessionOuterSceneResult prepare_session_outer_scene_442640(SessionOuterFrameContext &);
SessionOuterFrameResult prepare_session_outer_frame_4427b0(SessionOuterFrameContext &);

// NEW host guard, not recovered game state/timing. Serial comes from accepted
// source outer invocations, never from simulation_tick, GPU epoch or monitor
// refresh count. Paused invocations may share one simulation_tick.
class SessionOuterFrameCadence final
{
    std::optional<std::uint64_t> last_;
    bool active_{}, poisoned_{};

  public:
    SessionOuterFrameCadence() = default;
    SessionOuterFrameCadence(const SessionOuterFrameCadence &) = delete;
    SessionOuterFrameCadence &operator=(const SessionOuterFrameCadence &) = delete;
    void begin(std::uint64_t serial);
    void complete() noexcept;
    void poison() noexcept;
};
// The caller opens CPU capture before this entry and seals it only afterwards.
// Seal failure must poison cadence too. Source writes are never rolled back.
SessionOuterFrameResult prepare_session_outer_frame_once(SessionOuterFrameContext &, SessionOuterFrameCadence &,
                                                         std::uint64_t accepted_outer_serial);
} // namespace sgn::retail
