#pragma once
#include "sgn/retail_pause_menu.hpp"
#include <array>
#include <optional>

namespace sgn::retail
{
class RaceSession;

// New host wiring. Construction binds only the Session/observer identities;
// it must not capture source inputs before 446AF0's stop_audio_music returns.
// The adapter, Session, and optional observer must remain alive throughout each
// synchronous menu call. Keeping shared_state() alone does not retain the
// Session's level resources. All use belongs to the Session's owning thread.
class SessionPauseRestartSource final : public PauseMenuRestartSource
{
    struct Invocation final
    {
        VehicleResetParameters reset_parameters;
        std::array<ResetVehicle, 6> reset_vehicles;
        std::array<std::uint32_t, 4> selected_vehicles;
        AudioDriver audio;
        AudioStartupWorld audio_startup;
        VehicleResetWorld reset;

        Invocation(RaceSession &, AudioRequestObserver *);
        Invocation(const Invocation &) = delete;
        Invocation &operator=(const Invocation &) = delete;
        Invocation(Invocation &&) = delete;
        Invocation &operator=(Invocation &&) = delete;
    };

    RaceSession &session_;
    AudioRequestObserver *observer_;
    std::optional<Invocation> invocation_;

  public:
    explicit SessionPauseRestartSource(RaceSession &, AudioRequestObserver * = nullptr) noexcept;
    SessionPauseRestartSource(const SessionPauseRestartSource &) = delete;
    SessionPauseRestartSource &operator=(const SessionPauseRestartSource &) = delete;
    SessionPauseRestartSource(SessionPauseRestartSource &&) = delete;
    SessionPauseRestartSource &operator=(SessionPauseRestartSource &&) = delete;

    // For the existing restart_pause_menu_446af0 caller only. Acquires fresh
    // views/parameters after the completed music child. The returned views are
    // valid until the next borrow_restart() call or this adapter's destruction.
    // Calls must not overlap/reenter or retain an earlier invocation across
    // another borrow. No initialize_race, cursor publication or resume occurs
    // here: the actual menu implementation owns that exact source sequence.
    [[nodiscard]] PauseMenuRestartInvocation borrow_restart() override;
};
} // namespace sgn::retail
