#pragma once
#include "sgn/source_application_shutdown.hpp"
#include "sgn/source_application_surface_creation.hpp"

namespace sgn::retail
{
// Borrowed COM identity. The wrapper outlives Release and all source aliases.
struct SourceDisplayReleaseReceiver
{
    virtual ~SourceDisplayReleaseReceiver() = default;
    virtual std::uint32_t release_slot08() = 0;
};
struct SourceDisplayShutdownWords
{
    SourceDisplayReleaseReceiver *&draw_61c378;
    SourceDisplayReleaseReceiver *&device_61c380;
    SourceApplicationSurfaceSlot *&selected_61c358;
    SourceApplicationSurface *&primary_61c368;
    SourceApplicationSurface *&back_61c3a0;
    std::uintptr_t &window_61c398;
};
struct SourceDisplayWindowDestruction
{
    virtual ~SourceDisplayWindowDestruction() = default;
    virtual void destroy_window(std::uintptr_t window) = 0;
};
void release_source_display_interfaces_4228e0(SourceDisplayShutdownWords);
void destroy_source_window_422930(std::uintptr_t *word, SourceDisplayWindowDestruction &);
void release_source_display_422a60(SourceDisplayShutdownWords, SourceApplicationSurfaceRegistry,
                                   SourceDisplayWindowDestruction &);

// Retains required lower menu/input/audio services from the shutdown interface.
class SourceApplicationDisplayShutdown : public SourceApplicationShutdownServices
{
    SourceDisplayShutdownWords words_;
    SourceApplicationSurfaceRegistry registry_;
    SourceDisplayWindowDestruction &windows_;

  public:
    SourceApplicationDisplayShutdown(SourceDisplayShutdownWords words, SourceApplicationSurfaceRegistry registry,
                                     SourceDisplayWindowDestruction &windows)
        : words_(words), registry_(registry), windows_(windows)
    {
    }
    void release_display_422a60() final;
};
} // namespace sgn::retail
