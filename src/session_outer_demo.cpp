#include "sgn/session_outer_frame.hpp"
#include "sgn/retail_demo_tail.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/race_overlay_textures.hpp"

namespace sgn::retail
{
namespace
{
// New Session borrow adapter, not another retained game-state owner.
class Drawing final : public DemoTailDrawingSources
{
    SessionOuterFrameContext &frame_;

  public:
    explicit Drawing(SessionOuterFrameContext &frame) noexcept : frame_(frame) {}
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
            throw FormatError("demo requires prepared shared four-vertex scratch");
        auto &s = frame_.session.state();
        std::span<PolygonSubmissionVertex, 4> vertices{frame_.scratch.alpha_vertices.data(), 4};
        return {s.ui.descriptor, vertices, s.ui.selected_material, bank.names.materials.active_materials(), s.viewport};
    }
};
} // namespace

// Session adapter for the full demo tail. The source provider borrows its
// three persistent words from the shared Session owner.
void submit_session_demo_40e030(SessionOuterFrameContext &frame)
{
    frame.session.shared_platform()->require_owner_thread();
    auto &s = frame.session.state();
    Drawing drawing(frame);
    DemoTailWorld world{s.hud_block,
                        s.demo_ticks,
                        frame.sources.borrow_demo_words(),
                        {s.controls, s.hud_countdown, s.hud_layout, s.selected_hud, s.hud_margins},
                        s.game_text,
                        frame.textures,
                        s.ui.descriptor,
                        drawing,
                        frame.sink};
    submit_demo_tail_40e030(world);
}
} // namespace sgn::retail
