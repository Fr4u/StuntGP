#include "sgn/session_outer_results.hpp"
#include "sgn/session_outer_frame.hpp"
#include "sgn/retail_results_submission.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/source_application_literals.hpp"

namespace sgn::retail
{
namespace
{
// Invocation lifetime only. AudioDriver is a reference-bearing view, not a
// second audio owner. Its address remains valid throughout each position child.
// Never retain material or alpha spans through a preceding child/upload.
class SessionResultsDrawing final : public ResultsDrawingSources
{
    SessionOuterFrameContext &frame_;
    AudioDriver audio_;

  public:
    // The entry checks the owner thread before this reference-only constructor.
    explicit SessionResultsDrawing(SessionOuterFrameContext &frame) : frame_(frame), audio_(frame.session.audio_view())
    {
    }
    SessionResultsDrawing(const SessionResultsDrawing &) = delete;
    SessionResultsDrawing &operator=(const SessionResultsDrawing &) = delete;
    SessionResultsDrawing(SessionResultsDrawing &&) = delete;
    SessionResultsDrawing &operator=(SessionResultsDrawing &&) = delete;

    std::span<const std::uint8_t> borrow_result_initial_text(std::uint32_t address, std::uint32_t site) override
    {
        // Compatibility path still requires the actual App binding. An
        // unbound preview platform cannot fabricate these source literals.
        const auto app = frame_.session.shared_platform()->source_application_literals();
        return app.result_initial_text(address, site);
    }

    TextSubmission borrow_text() override
    {
        frame_.session.shared_platform()->require_owner_thread();
        const auto &bank = frame_.materials.validate_for(frame_.session);
        (void)frame_.materials.validate_fonts_for(frame_.session);
        auto &s = frame_.session.state();
        return {s.ui.fonts,
                s.ui.selected_material,
                frame_.scratch.specular_enabled,
                bank.names.materials.active_materials(),
                s.view_quality.second,
                s.view_quality.legacy_driver_restriction,
                s.viewport};
    }

    QuadSubmission borrow_quad() override
    {
        frame_.session.shared_platform()->require_owner_thread();
        const auto &bank = frame_.materials.validate_for(frame_.session);
        if (frame_.scratch.alpha_vertices.size() < 4)
            throw FormatError("Session results require prepared shared four-vertex scratch");
        auto &s = frame_.session.state();
        std::span<PolygonSubmissionVertex, 4> vertices{frame_.scratch.alpha_vertices.data(), 4};
        return {s.ui.descriptor, vertices, s.ui.selected_material, bank.names.materials.active_materials(), s.viewport};
    }

    void submit_position_4487d0(PolygonSubmissionSink &sink, std::uint32_t index, float x, float y,
                                std::uint32_t depth) override
    {
        // borrow_text checks the thread first and obtains the current material
        // projection and font owners. It performs no child, font selection or
        // allocation. The alpha allocation is acquired afresh for this child.
        auto rendering = borrow_text();
        if (frame_.scratch.alpha_vertices.size() < 4)
            throw FormatError("Session results require prepared shared four-vertex scratch");
        auto &s = frame_.session.state();
        std::span<PolygonSubmissionVertex, 4> vertices{frame_.scratch.alpha_vertices.data(), 4};
        RaceHudSubmission view{{s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                               s.vehicles,
                               s.clock,
                               s.records,
                               s.ranking,
                               s.cars,
                               s.game_text,
                               s.split_setting,
                               s.frame_pause,
                               frame_.textures,
                               s.ui.stars,
                               s.ui.descriptor,
                               vertices,
                               rendering,
                               audio_};
        submit_race_position_4487d0(view, sink, index, x, y, depth);
    }
};
} // namespace

void submit_session_results_449fb0(SessionOuterFrameContext &frame)
{
    frame.textures.require_for(frame.session.shared_platform());
    auto &s = frame.session.state();
    SessionResultsDrawing drawing(frame);
    ResultsWorld world{{s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                       s.vehicles,
                       s.current_slot,
                       s.normal_finish,
                       s.ranking,
                       s.cars,
                       s.game_text,
                       s.clock,
                       frame.textures,
                       s.ui.descriptor,
                       drawing,
                       frame.sink};
    // Full unchanged source port. Its early returns do not acquire drawing
    // resources; later children borrow after the actual source27 upload.
    submit_race_results_449fb0(world);
}
} // namespace sgn::retail
