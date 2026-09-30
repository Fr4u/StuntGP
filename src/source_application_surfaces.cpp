#include "sgn/source_application_surfaces.hpp"

namespace sgn::retail
{
std::uint32_t source_surface_status_44e9b0(SourceApplicationSurface *surface)
{
    return surface ? surface->is_lost_slot60() : 0u;
}
std::uint32_t source_surface_restore_44e9c0(SourceApplicationSurface *surface)
{
    return surface ? surface->restore_slot6c() : 0u;
}

namespace
{
template <bool Restore> std::uint32_t visit_resources(SourceApplicationSurfaceServices &services)
{
    std::uint32_t count = services.resource_count_5d91cc();
    std::uint32_t index = 0;
    while (index < count)
    {
        // Read each word only when reached. The first COM call may change
        // the second word or the count. Equal identities are called twice.
        auto *first = services.resource_surface(index, 0);
        if (first)
        {
            const auto result = Restore ? first->restore_slot6c() : first->is_lost_slot60();
            if (result != 0)
                return 0;
        }
        auto *second = services.resource_surface(index, 8);
        if (second)
        {
            const auto result = Restore ? second->restore_slot6c() : second->is_lost_slot60();
            if (result != 0)
                return 0;
        }
        count = services.resource_count_5d91cc();
        ++index;
    }
    return 1;
}
} // namespace

std::uint32_t source_resource_status_4314c0(SourceApplicationSurfaceServices &services)
{
    return visit_resources<false>(services);
}
std::uint32_t source_resource_restore_431510(SourceApplicationSurfaceServices &services)
{
    return visit_resources<true>(services);
}

void source_application_idle_44e9d0(SourceApplicationSurfaceServices &services)
{
    if (source_surface_status_44e9b0(services.surface_61c368()) == 0 &&
        source_surface_status_44e9b0(services.surface_61c3a0()) == 0 && source_resource_status_4314c0(services) == 1)
    {
        return;
    }
    services.window_mode_44e4b0(1);
}

void source_application_restore_44ea10(SourceApplicationSurfaceServices &services)
{
    if (source_surface_restore_44e9c0(services.surface_61c368()) == 0 &&
        source_surface_restore_44e9c0(services.surface_61c3a0()) == 0 && source_resource_restore_431510(services) == 1)
    {
        services.reload_materials_42bf00();
        return;
    }
    services.window_mode_44e4b0(1);
}
} // namespace sgn::retail
