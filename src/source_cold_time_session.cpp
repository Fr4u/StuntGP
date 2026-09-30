#include "sgn/source_cold_time.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void initialize_platform_source_random_time_42f1a0(RaceSessionPlatform &owner, SourceColdTimePlatform &services)
{
    owner.require_owner_thread();
    auto &state = owner.state();
    initialize_source_random_time_42f1a0(state.source_cold_time, state.random, services);
}
void initialize_session_source_random_time_42f1a0(RaceSession &session, SourceColdTimePlatform &services)
{
    initialize_platform_source_random_time_42f1a0(*session.shared_platform(), services);
}
} // namespace sgn::retail
