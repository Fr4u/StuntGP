#pragma once
#include "sgn/source_application_surfaces.hpp"

namespace sgn::retail
{
class FrontendState;
struct NewHostD3D12WindowOperations;
// NEW HOST forwarding bridge. Construct before HWND, attach a real service
// after its native dependencies exist. Never stands in for a COM surface.
class SourceLateBoundApplicationSurfaces final : public SourceApplicationSurfaceServices
{
    FrontendState &frontend_;
    SourceApplicationSurfaceServices *target_{};
    NewHostD3D12WindowOperations *host_{};
    bool attached_once_{};

  public:
    explicit SourceLateBoundApplicationSurfaces(FrontendState &);
    SourceLateBoundApplicationSurfaces(const SourceLateBoundApplicationSurfaces &) = delete;
    SourceLateBoundApplicationSurfaces &operator=(const SourceLateBoundApplicationSurfaces &) = delete;
    void attach(SourceApplicationSurfaceServices &); // actual legacy target only
    void attach_host(NewHostD3D12WindowOperations &);
    NewHostD3D12WindowOperations &require_host() const;
    void detach(); // host lifetime action, never a source Release or rollback
    SourceApplicationSurfaceServices &require_bound() const;
    void require_frontend(const FrontendState &) const;
    SourceApplicationSurface *surface_61c368() override;
    SourceApplicationSurface *surface_61c3a0() override;
    std::uint32_t resource_count_5d91cc() override;
    SourceApplicationSurface *resource_surface(std::uint32_t, std::uint32_t) override;
    void window_mode_44e4b0(std::uint32_t) override;
    void reload_materials_42bf00() override;
};
} // namespace sgn::retail
