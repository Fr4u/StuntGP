#pragma once
#include "sgn/retail_frontend_state.hpp"

namespace sgn::retail
{
// NEW HOST observation only. These states never replace a source flag or make
// 442180 idempotent. Inspect on the owning thread of the supplied frontend.
enum class SourceApplicationShutdownProgress
{
    never_started,
    in_progress,
    completed,
    partial
};
struct SourceApplicationShutdownServices
{
    SourceApplicationShutdownServices() = default;
    virtual ~SourceApplicationShutdownServices() = default;
    SourceApplicationShutdownServices(const SourceApplicationShutdownServices &) = delete;
    SourceApplicationShutdownServices &operator=(const SourceApplicationShutdownServices &) = delete;
    SourceApplicationShutdownProgress new_host_shutdown_progress() const noexcept;
    bool new_host_shutdown_completed_for(const FrontendState &) const noexcept;
    virtual void close_menu_43d180() = 0;
    virtual void release_graphics_resources_431800() = 0;
    virtual void release_display_422a60() = 0;
    virtual void shutdown_input_424930() = 0;

  private:
    // Stack-linked scopes avoid allocating or overflowing a nesting counter.
    // Only the complete public source boundary may modify this observation.
    struct Attempt;
    Attempt *active_attempt_{};
    const FrontendState *observed_frontend_{};
    bool interrupted_{}, mixed_frontend_{};
    friend void shutdown_source_application_442180(FrontendState &, SourceApplicationShutdownServices &);
};
void shutdown_source_application_442180(FrontendState &, SourceApplicationShutdownServices &);
} // namespace sgn::retail
