#pragma once
namespace sgn::retail
{
class FrontendState;
// Direct implementations for SourceMenuFrameChildren's two exact boundaries.
// These methods retain no owner and call no native API.
void prepare_camera_402e90(FrontendState &);
void prepare_geometry_44fd60(FrontendState &);
} // namespace sgn::retail
