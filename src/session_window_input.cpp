#include "sgn/session_window_input.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <exception>
#include <utility>

namespace sgn::retail
{
namespace
{
InputWindowHandles session_handles(const RaceSession &session, std::uintptr_t instance)
{
    session.shared_platform()->require_owner_thread();
    const auto window = session.window();
    if (!std::in_range<std::uintptr_t>(window))
        throw FormatError("Session HWND does not fit the native pointer width");
    return {instance, static_cast<std::uintptr_t>(window)};
}
} // namespace
SessionWindowInput::SessionWindowInput(std::shared_ptr<RaceSessionPlatform> platform,
                                       std::shared_ptr<RaceSessionState> world, InputWindowHandles handles)
    : platform_(std::move(platform)), handles_(handles)
{
    if (!platform_ || !world || world.owner_before(platform_) || platform_.owner_before(world))
        throw FormatError("window input requires the actual world alias of one platform control block");
    platform_->require_owner_thread();
    if (world.get() != &platform_->state())
        throw FormatError("window input world does not belong to the supplied platform");
    (void)platform_->attach_window_input(handles_.instance, handles_.window);
}
SessionWindowInput::SessionWindowInput(RaceSession &session, std::uintptr_t instance)
    : SessionWindowInput(session.shared_platform(), session.shared_state(), session_handles(session, instance))
{
}
SessionWindowInput::SessionWindowInput(FrontendState &frontend, InputWindowHandles handles)
    : SessionWindowInput(frontend.shared_platform(), frontend.shared_world(), handles)
{
}
SessionWindowInput::~SessionWindowInput()
{
    try
    {
        platform_->require_owner_thread();
    }
    catch (...)
    {
        std::terminate();
    }
}
std::uint32_t SessionWindowInput::initialize_4249b0()
{
    auto &owner = platform_->window_input(handles_.instance, handles_.window);
    return owner.initialize_4249b0(platform_->state().source_input.acquisition, handles_);
}
void SessionWindowInput::shutdown_424930()
{
    auto &owner = platform_->window_input(handles_.instance, handles_.window);
    owner.shutdown_424930(platform_->state().source_input.acquisition, handles_);
}
WindowInputBorrow SessionWindowInput::borrow_initialized()
{
    auto &owner = platform_->window_input(handles_.instance, handles_.window);
    return owner.borrow_initialized(platform_->state().source_input.acquisition, handles_);
}
void SessionWindowInput::require_session(const RaceSession &session) const
{
    platform_->require_owner_thread();
    const auto &candidate = session.shared_platform();
    const auto &world = session.shared_state();
    if (candidate.get() != platform_.get() || candidate.owner_before(platform_) || platform_.owner_before(candidate) ||
        !world || world.owner_before(platform_) || platform_.owner_before(world) ||
        world.get() != &platform_->state() ||
        (session.readiness() != RaceSessionReadiness::source_attachment_only &&
         session.window() != static_cast<std::uint64_t>(handles_.window)))
        throw FormatError("window input and Session must share the same world, control block and HWND");
    (void)platform_->window_input(handles_.instance, handles_.window);
}
WindowInputBorrow SessionWindowInput::borrow_initialized(RaceSession &session)
{
    require_session(session);
    return borrow_initialized();
}
} // namespace sgn::retail
