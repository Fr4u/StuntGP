#include "sgn/source_rail_reset.hpp"
#include "sgn/race_session.hpp"
#include <type_traits>

namespace sgn::retail
{
static_assert(std::is_trivially_destructible_v<SplineView>);

void reset_rail_4192d0(RailState &state) noexcept
{
    state.tube_glass_texture.reset(); // 004192D2: source FFFFFFFF
    state.spline.reset();             // 004192DC: source NULL; no PMD release
    state.source_word_4d5ca0 = 0;     // 004192E1: opaque raw DWORD
}

void reset_platform_source_rail_4192d0(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    reset_rail_4192d0(platform.state().rail);
}
} // namespace sgn::retail
