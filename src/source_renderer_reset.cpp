#include "sgn/source_renderer_reset.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session_platform.hpp"
namespace sgn::retail
{
void reset_source_renderer_431300(SourceRendererResetContext context)
{
    context.frontend.shared_platform()->require_owner_thread();
    context.frames.require_source_frontend(context.frontend);
    for (unsigned pass = 0; pass < 3; ++pass)
    {
        // Reborrow at each actual child; a presentation must never retain an
        // earlier material span across source release/repopulation boundaries.
        auto &state = context.frames.source_loading_state(context.materials);
        auto frame = context.frames.borrow_source_loading_frame(state, context.materials);
        clear_loading_frame_4328c0(frame, 0, context.presentation.sink(), context.presentation.device());
        prepare_loading_surface_432a60(context.presentation.device());
    }
    context.materials.release_all();
    context.materials.reset_transparency_430690();
}
} // namespace sgn::retail
