#pragma once
#include "sgn/retail_pause_gate.hpp"
#include "sgn/retail_pause_lifecycle.hpp"
#include <array>

namespace sgn::retail
{
struct RaceSessionState;
class RaceSession;
class RaceSessionPlatform;

// New host wiring only. The array and driver below are invocation-local views;
// all source data remains in one RaceSessionState. Member order supplies stable
// storage for views referring to earlier members. Never copy, move or retain
// this binder across source-owner/resource destruction or another thread.
class SessionPauseInvocation final
{
    std::array<VehicleAudioCar, 6> cars_;
    AudioDriver audio_;
    VehicleAudioWorld vehicle_audio_;
    PauseLifecycleWorld lifecycle_;

  public:
    // Explicit device supports an OS-boundary qualification recorder. Caller
    // owns synchronization and all lifetimes. Construction only binds addresses;
    // it does not validate/read vehicle mappings or advance any source state.
    SessionPauseInvocation(RaceSessionState &, AudioDevice &, AudioRequestObserver * = nullptr) noexcept;
    // Same binding for an actual shared platform; checks the host owner thread
    // before obtaining its state. The explicit device is still caller-owned.
    SessionPauseInvocation(RaceSessionPlatform &, AudioDevice &, AudioRequestObserver * = nullptr);
    SessionPauseInvocation(const SessionPauseInvocation &) = delete;
    SessionPauseInvocation &operator=(const SessionPauseInvocation &) = delete;
    SessionPauseInvocation(SessionPauseInvocation &&) = delete;
    SessionPauseInvocation &operator=(SessionPauseInvocation &&) = delete;
    // Borrow only from a live lvalue binder; no dangling temporary factory view.
    [[nodiscard]] const PauseLifecycleWorld &lifecycle() const & noexcept
    {
        return lifecycle_;
    }
    const PauseLifecycleWorld &lifecycle() const && = delete;
    void enter();  // actual full4422C0; first source-word read is inhibit
    void resume(); // actual full442350; no eager clear/reset
};

// These factories borrow current mutable owners; they neither call the gate
// nor invent source input producers/cadence. Display presentation must not tick
// the gate. Returned views remain valid only while the world remains alive.
[[nodiscard]] PauseGateWorld session_pause_gate_view(RaceSessionState &) noexcept;
[[nodiscard]] PauseGateWorld session_pause_gate_view(RaceSessionPlatform &);

// Fresh invocation on the actual Session device, after its owner-thread guard.
// These are explicit entry points, not automatic tick/START/menu/input hooks.
void enter_session_pause(RaceSession &);
void resume_session_pause(RaceSession &);
} // namespace sgn::retail
