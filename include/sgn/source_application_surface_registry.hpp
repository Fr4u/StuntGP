#pragma once
#include "sgn/source_application_surfaces.hpp"
#include <span>

namespace sgn::retail
{
// Invocation-only view of5727D4..5728D3 and5728D4. Native identities retain
// pointer width; no extra AddRef, deduplication or ownership transfer occurs.
struct SourceApplicationSurfaceRegistry
{
    std::span<SourceApplicationSurface *, 64> entries;
    std::uint32_t &count;
};
void clear_source_surface_registry_4229e0(SourceApplicationSurfaceRegistry);
void register_source_surface_422a00(SourceApplicationSurfaceRegistry,
                                    SourceApplicationSurface *const &destination_word);
void release_source_surfaces_422a20(SourceApplicationSurfaceRegistry);
} // namespace sgn::retail
