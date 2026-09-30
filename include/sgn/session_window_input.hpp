#pragma once
#include "sgn/window_input_owner.hpp"
#include <memory>

namespace sgn::retail
{
class RaceSession;
class RaceSessionPlatform;
class FrontendState;
struct RaceSessionState;
// New host wrapper. Its one strong platform reference retains the SAME world
// and input owner after Session/frontend destruction. It holds no Session,
// frontend, keyboard snapshot, registry copy or callback back to the platform.
// Creation/rebinding is host attachment ONLY; source events remain explicit.
class SessionWindowInput final
{
  public:
    SessionWindowInput(std::shared_ptr<RaceSessionPlatform>, std::shared_ptr<RaceSessionState>, InputWindowHandles);
    SessionWindowInput(RaceSession &, std::uintptr_t instance);
    SessionWindowInput(FrontendState &, InputWindowHandles);
    ~SessionWindowInput(); // all wrapper destruction belongs to the owner thread
    SessionWindowInput(const SessionWindowInput &) = delete;
    SessionWindowInput &operator=(const SessionWindowInput &) = delete;
    SessionWindowInput(SessionWindowInput &&) = delete;
    SessionWindowInput &operator=(SessionWindowInput &&) = delete;

    [[nodiscard]] std::uint32_t initialize_4249b0();
    void shutdown_424930();
    // Generic frontend/platform borrow and a Session-checked borrow. References
    // last only while their wrapper/platform and native input lifetime live.
    [[nodiscard]] WindowInputBorrow borrow_initialized();
    [[nodiscard]] WindowInputBorrow borrow_initialized(RaceSession &);
    void require_session(const RaceSession &) const;

  private:
    std::shared_ptr<RaceSessionPlatform> platform_;
    const InputWindowHandles handles_;
};
} // namespace sgn::retail
