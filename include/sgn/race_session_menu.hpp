#pragma once
#include "sgn/race_session_pause.hpp"
#include "sgn/retail_pause_menu.hpp"
#include "sgn/session_pause_meter_labels.hpp"

namespace sgn::retail
{
class SessionWorldMaterials;

// Session menu adapter: new host composition over recovered source owners.
// One synchronous invocation. Source storage remains in the supplied Session,
// materials, submission state and texture device. All must outlive this binder.
// Do not retain it through Session/level teardown, another thread or callbacks.
class SessionMenuInvocation final
{
    class Drawing final : public PauseMenuDrawingSources
    {
        RaceSession &session_;
        const SessionWorldMaterials &materials_;
        PolygonSubmissionState &submission_;

      public:
        Drawing(RaceSession &session, const SessionWorldMaterials &materials,
                PolygonSubmissionState &submission) noexcept
            : session_(session), materials_(materials), submission_(submission)
        {
        }
        TextSubmission borrow_text() override;
        std::span<PolygonSubmissionVertex> borrow_alpha_vertices() override;
    };
    class Restart final : public PauseMenuRestartSource
    {
        RaceSessionState &state_;
        PauseMenuRestartSource &provider_;

      public:
        Restart(RaceSessionState &state, PauseMenuRestartSource &provider) noexcept : state_(state), provider_(provider)
        {
        }
        PauseMenuRestartInvocation borrow_restart() override;
    };

    static RaceSession &checked_session(RaceSession &, const RaceOverlayTextures &);
    // Order matters: the final reference-bearing world borrows earlier members.
    RaceSession &session_;
    SessionPauseInvocation pause_;
    SceneTransitionWorld transition_;
    SceneRaceResultWorld results_;
    SessionPauseMeterLabels labels_;
    Drawing drawing_;
    Restart restart_;
    PauseMenuWorld world_;

  public:
    // Required services are explicit. Construction does not call restart,
    // upload textures, draw, allocate alpha vertices or run4473F0.
    // The supplied texture bank must use the same material/backend ownership;
    // current APIs cannot verify that identity; the outer caller must ensure it.
    SessionMenuInvocation(RaceSession &, const SessionWorldMaterials &, PolygonSubmissionState &, RaceOverlayTextures &,
                          PauseMenuRestartSource &, PolygonSubmissionSink &);
    SessionMenuInvocation(const SessionMenuInvocation &) = delete;
    SessionMenuInvocation &operator=(const SessionMenuInvocation &) = delete;
    SessionMenuInvocation(SessionMenuInvocation &&) = delete;
    SessionMenuInvocation &operator=(SessionMenuInvocation &&) = delete;

    // Explicit borrow only. The future outer-frame caller owns source cadence.
    [[nodiscard]] PauseMenuWorld &view() &;
    PauseMenuWorld &view() && = delete;
    [[nodiscard]] const PauseMenuWorld &view() const &;
    const PauseMenuWorld &view() const && = delete;
};
} // namespace sgn::retail
