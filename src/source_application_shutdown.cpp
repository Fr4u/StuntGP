#include "sgn/source_application_shutdown.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_allocation_bank.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/source_music_calls.hpp"

namespace sgn::retail
{
struct SourceApplicationShutdownServices::Attempt
{
    SourceApplicationShutdownServices &services;
    Attempt *previous;
    bool returned{};
    Attempt(SourceApplicationShutdownServices &owner, FrontendState &frontend) noexcept
        : services(owner), previous(owner.active_attempt_)
    {
        if (!services.observed_frontend_)
            services.observed_frontend_ = &frontend;
        else if (services.observed_frontend_ != &frontend)
            services.mixed_frontend_ = true;
        services.active_attempt_ = this;
    }
    ~Attempt() noexcept
    {
        if (!returned)
            services.interrupted_ = true;
        services.active_attempt_ = previous;
    }
    Attempt(const Attempt &) = delete;
    Attempt &operator=(const Attempt &) = delete;
};
SourceApplicationShutdownProgress SourceApplicationShutdownServices::new_host_shutdown_progress() const noexcept
{
    if (active_attempt_)
        return SourceApplicationShutdownProgress::in_progress;
    if (!observed_frontend_)
        return SourceApplicationShutdownProgress::never_started;
    if (interrupted_ || mixed_frontend_)
        return SourceApplicationShutdownProgress::partial;
    return SourceApplicationShutdownProgress::completed;
}
bool SourceApplicationShutdownServices::new_host_shutdown_completed_for(const FrontendState &frontend) const noexcept
{
    return observed_frontend_ == &frontend &&
           new_host_shutdown_progress() == SourceApplicationShutdownProgress::completed;
}
void shutdown_source_application_442180(FrontendState &frontend, SourceApplicationShutdownServices &services)
{
    const auto platform = frontend.shared_platform();
    platform->require_owner_thread();
    if (&platform->state() != &frontend.world())
        throw FormatError("442180 frontend and platform belong to different worlds");
    // Observe the complete function, including archive, audio and allocation
    // children outside the four virtual services. Never retry or skip a child.
    SourceApplicationShutdownServices::Attempt attempt(services, frontend);
    if (frontend.source.scene.menu_initialized == 1)
        services.close_menu_43d180();
    services.release_graphics_resources_431800();
    release_source_archive_44b1a0(platform->state().boot_config.files);
    services.release_display_422a60();
    services.shutdown_input_424930();
    shutdown_source_audio_4621d0(*platform);
    platform->source_allocations().shutdown_42eaf0();
    attempt.returned = true; // Nested failures remain sticky even if a caller caught them.
}
} // namespace sgn::retail
