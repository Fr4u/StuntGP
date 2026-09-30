#pragma once
#include "sgn/source_force_feedback_state.hpp"
#include <span>

namespace sgn::retail
{
class RaceSession;
class RaceSessionPlatform;
class Win32MenuDirectInputStartup;
struct SourceInputStorage;
struct SourceInputWorld;
// Required same platform/input/startup lifetime. The Session overload delegates
// to that existing platform. No copied physics/RNG bank.
// Every child is synchronous; no resource/game callback or stored world view.
struct SourceForceFeedbackWorld
{
    RaceSessionPlatform &platform;
    SourceForceFeedbackWorld(RaceSession &, SourceInputStorage &, Win32MenuDirectInputStartup &);
    // Same retained owners without constructing a per-level preview Session.
    SourceForceFeedbackWorld(RaceSessionPlatform &, SourceInputStorage &, Win32MenuDirectInputStartup &);
    SourceInputStorage &input;
    Win32MenuDirectInputStartup &startup;
};
// New host identity guard. Called only at a reached40DCE1 child boundary.
void require_force_feedback_dispatch_binding(const SourceInputWorld &, SourceForceFeedbackWorld &);
std::uint32_t force_feedback_45ed60(SourceForceFeedbackWorld &, unsigned physical);
std::uint32_t force_feedback_443770(SourceForceFeedbackWorld &);
void force_feedback_410a10(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_4240e0(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_424310(SourceForceFeedbackWorld &, unsigned physical);
std::uint32_t force_feedback_423e40(SourceForceFeedbackWorld &, unsigned record);
bool force_feedback_423d30(SourceForceFeedbackWorld &, unsigned record, unsigned effect,
                           std::span<const std::uint32_t, 6> condition);
void force_feedback_423dd0(SourceForceFeedbackWorld &, unsigned record, unsigned effect,
                           std::span<const std::uint32_t, 6> condition);
void force_feedback_423f40(SourceForceFeedbackWorld &, unsigned record, unsigned effect,
                           const std::array<std::uint32_t, 4> &guid, std::uint32_t duration);
void force_feedback_423ff0(SourceForceFeedbackWorld &, unsigned record);
void force_feedback_424030(SourceForceFeedbackWorld &, unsigned record);
void force_feedback_424070(SourceForceFeedbackWorld &, unsigned record);
void force_feedback_410780(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_410800(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_4108c0(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_410950(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_4109e0(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_4241a0(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_424210(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_424290(SourceForceFeedbackWorld &, unsigned physical);
void force_feedback_424120(SourceForceFeedbackWorld &, unsigned physical);
} // namespace sgn::retail
