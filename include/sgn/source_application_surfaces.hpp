#pragma once
#include <cstdint>
#include <array>
#include <optional>

namespace sgn::retail
{
// Borrowed receiver identities, never truncated native pointers. Implementations
// map these calls to the same retained surface; HRESULT bits remain unchanged.
struct SourceApplicationSurface
{
    virtual ~SourceApplicationSurface() = default;
    virtual std::uint32_t is_lost_slot60() = 0;
    virtual std::uint32_t restore_slot6c() = 0;
    // Adapter object lifetime is independent of the native COM reference.
    // Release must not destroy this wrapper while source slots still name it.
    virtual std::uint32_t release_slot08() = 0;
    // 44E4DE supplies an uninitialized four-DWORD local. A failed call need
    // not write it; absence preserves that fact instead of inventing zeros.
    virtual std::uint32_t capabilities_slot38(std::array<std::optional<std::uint32_t>, 4> &output) = 0;
};

// Live access to existing owners. No snapshot of count, records or surface IDs.
// Missing host bindings must throw; nullptr means an actual source null word.
struct SourceApplicationSurfaceServices
{
    virtual ~SourceApplicationSurfaceServices() = default;
    virtual SourceApplicationSurface *surface_61c368() = 0;
    virtual SourceApplicationSurface *surface_61c3a0() = 0;
    virtual std::uint32_t resource_count_5d91cc() = 0;
    virtual SourceApplicationSurface *resource_surface(std::uint32_t record,
                                                       std::uint32_t source_byte_offset) = 0; // record+0, record+8 only
    virtual void window_mode_44e4b0(std::uint32_t mode) = 0;
    virtual void reload_materials_42bf00() = 0;
};

std::uint32_t source_surface_status_44e9b0(SourceApplicationSurface *);
std::uint32_t source_surface_restore_44e9c0(SourceApplicationSurface *);
std::uint32_t source_resource_status_4314c0(SourceApplicationSurfaceServices &);
std::uint32_t source_resource_restore_431510(SourceApplicationSurfaceServices &);
void source_application_idle_44e9d0(SourceApplicationSurfaceServices &);
void source_application_restore_44ea10(SourceApplicationSurfaceServices &);
} // namespace sgn::retail
