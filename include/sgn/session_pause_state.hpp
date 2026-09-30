#pragma once
#include "sgn/retail_pause_lifecycle.hpp"

namespace sgn::retail
{
// Five source objects absent from the existing shared world. Pinned PE zeros
// apply once at represented-world construction, never on binding or level
// detach/restart. See docs/SERIES6-PAUSE-STORAGE-CONTRACT.md. This does not
// establish original process-global lifetime across new platform instances.
struct SessionPauseState
{
    SessionPauseState() = default;
    SessionPauseState(const SessionPauseState &) = delete;
    SessionPauseState &operator=(const SessionPauseState &) = delete;
    SessionPauseState(SessionPauseState &&) = delete;
    SessionPauseState &operator=(SessionPauseState &&) = delete;
    std::uint32_t age{};                         // 612954; shared by4422C0/4423D0/442200;442350 retains
    std::uint32_t status{};                      // 61295C;442350 clears after its children
    PausePhysicalInputIdentity physical_input{}; // 61B744;0=null,1..4=physical slot0..3
    std::uint32_t raw_60d620{};                  // raw gate input; producer/meaning unresolved
    std::uint32_t raw_activity{};                // 612970;45EFE5/45F0DE/45F130 writers outside this port
};
static_assert(sizeof(SessionPauseState) == 5 * sizeof(std::uint32_t));
} // namespace sgn::retail
