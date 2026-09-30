#include "sgn/source_application_surfaces_late_bound.hpp"
#include "sgn/source_application_window_d3d12_operations.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session_platform.hpp"

namespace sgn::retail
{
SourceLateBoundApplicationSurfaces::SourceLateBoundApplicationSurfaces(FrontendState &frontend) : frontend_(frontend) {}
void SourceLateBoundApplicationSurfaces::require_frontend(const FrontendState &frontend) const
{
    frontend_.shared_platform()->require_owner_thread();
    if (&frontend != &frontend_)
        throw FormatError("Application surface bridge belongs to another frontend");
}
void SourceLateBoundApplicationSurfaces::attach(SourceApplicationSurfaceServices &services)
{
    require_frontend(frontend_);
    if (dynamic_cast<SourceLateBoundApplicationSurfaces *>(&services) || attached_once_)
        throw FormatError("Application surface bridge requires one distinct retained target");
    target_ = &services;
    attached_once_ = true;
}
void SourceLateBoundApplicationSurfaces::attach_host(NewHostD3D12WindowOperations &services)
{
    require_frontend(frontend_);
    if (attached_once_)
        throw FormatError("Application surface bridge already attached in this lifetime");
    host_ = &services;
    attached_once_ = true;
}
NewHostD3D12WindowOperations &SourceLateBoundApplicationSurfaces::require_host() const
{
    require_frontend(frontend_);
    if (!host_)
        throw FormatError("NEW HOST D3D12 window lifecycle is not attached");
    return *host_;
}
void SourceLateBoundApplicationSurfaces::detach()
{
    require_frontend(frontend_);
    target_ = nullptr;
    host_ = nullptr;
}
SourceApplicationSurfaceServices &SourceLateBoundApplicationSurfaces::require_bound() const
{
    require_frontend(frontend_);
    if (host_)
        throw FormatError("Legacy surface/COM traversal is unavailable on NEW HOST D3D12 lifecycle");
    if (!target_)
        throw FormatError("Application surface mode/restore services are not attached to this host");
    return *target_;
}
SourceApplicationSurface *SourceLateBoundApplicationSurfaces::surface_61c368()
{
    return require_bound().surface_61c368();
}
SourceApplicationSurface *SourceLateBoundApplicationSurfaces::surface_61c3a0()
{
    return require_bound().surface_61c3a0();
}
std::uint32_t SourceLateBoundApplicationSurfaces::resource_count_5d91cc()
{
    return require_bound().resource_count_5d91cc();
}
SourceApplicationSurface *SourceLateBoundApplicationSurfaces::resource_surface(std::uint32_t record,
                                                                               std::uint32_t offset)
{
    return require_bound().resource_surface(record, offset);
}
void SourceLateBoundApplicationSurfaces::window_mode_44e4b0(std::uint32_t mode)
{
    require_frontend(frontend_);
    if (host_)
    {
        host_->window_mode(mode);
        return;
    }
    require_bound().window_mode_44e4b0(mode);
}
void SourceLateBoundApplicationSurfaces::reload_materials_42bf00()
{
    require_bound().reload_materials_42bf00();
}
} // namespace sgn::retail
