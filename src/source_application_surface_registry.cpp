#include "sgn/source_application_surface_registry.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void clear_source_surface_registry_4229e0(SourceApplicationSurfaceRegistry registry)
{
    for (auto &entry : registry.entries)
        entry = nullptr;
    registry.count = 0;
}

void register_source_surface_422a00(SourceApplicationSurfaceRegistry registry,
                                    SourceApplicationSurface *const &destination_word)
{
    const auto index = registry.count;
    auto *receiver = destination_word;
    if (index >= registry.entries.size())
        throw FormatError("422A07 writes beyond the owned64-word surface registry");
    registry.entries[index] = receiver; // even null is a registered source word
    registry.count = index + 1u;
}

void release_source_surfaces_422a20(SourceApplicationSurfaceRegistry registry)
{
    std::uint32_t count = registry.count;
    std::uint32_t index = 0;
    while (index < count)
    {
        if (index >= registry.entries.size())
            throw FormatError("422A32 reads beyond the owned64-word surface registry");
        auto *&slot = registry.entries[index];
        auto *receiver = slot;
        if (receiver)
        {
            (void)receiver->release_slot08();
            slot = nullptr; // after Release, overwrites any reentrant replacement
        }
        count = registry.count; // live reload after EACH iteration
        ++index;
    }
    clear_source_surface_registry_4229e0(registry);
}
} // namespace sgn::retail
