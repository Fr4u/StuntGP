#include "sgn/source_random_seed.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void seed_platform_source_random_42f190(RaceSessionPlatform &owner, std::uint32_t seed)
{
    owner.require_owner_thread();
    seed_source_random_42f190(owner.state().random, seed);
}
void seed_session_source_random_42f190(RaceSession &session, std::uint32_t seed)
{
    seed_platform_source_random_42f190(*session.shared_platform(), seed);
}
} // namespace sgn::retail
