#pragma once
#include "sgn/source_application_window_messages.hpp"
#include "sgn/window_input_owner.hpp"

namespace sgn
{
class Renderer;
class PolygonDeviceContext;
class RendererLoadingSurfaces;
namespace retail
{
class FrontendState;
// NEW HOST lifecycle protocol, not DirectDraw/COM slot emulation. The concrete
// presentation owner verifies all borrowed identities, including source words.
struct NewHostD3D12WindowOperations
{
    virtual ~NewHostD3D12WindowOperations() = default;
    // Window-only identity/owner admission. Must work with a real HWND and
    // no Renderer. Early native mode/CPU operations remain a separate stage.
    virtual void require_window_binding(FrontendState &, InputWindowHandles, SourceWindowMessageWords) const = 0;
    // GPU identity admission after the actual producer; empty image roles are legal.
    virtual void require_binding(FrontendState &, Renderer &, PolygonDeviceContext &, RendererLoadingSurfaces &,
                                 SourceWindowMessageWords) const = 0;
    virtual void idle() = 0;
    virtual void restore() = 0;
    virtual void window_mode(std::uint32_t) = 0;
};
} // namespace retail
} // namespace sgn
