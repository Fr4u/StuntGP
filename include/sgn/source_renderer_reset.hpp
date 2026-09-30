#pragma once
#include "sgn/source_menu_presentation_backend.hpp"
#include "sgn/material_d3d12.hpp"
namespace sgn::retail
{
// Invocation-only borrow. Presentation must already be bound to these same
// actual owners; no renderer construction or material bootstrap occurs here.
struct SourceRendererResetContext
{
    FrontendState &frontend;
    SessionOuterFrameServices &frames;
    MaterialD3D12 &materials;
    SourceMenuPresentationBackend &presentation;
};
void reset_source_renderer_431300(SourceRendererResetContext);
} // namespace sgn::retail
