#include "sgn/source_input_cold_init.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void initialize_platform_source_physical_input_45ec30(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    initialize_source_physical_input_45ec30(
        {state.source_input, state.menu_input, state.frontend_launch, state.source_input.force_feedback});
}
void initialize_session_source_physical_input_45ec30(RaceSession &session)
{
    initialize_platform_source_physical_input_45ec30(*session.shared_platform());
}
} // namespace sgn::retail
