#pragma once
#include "sgn/retail_camera.hpp"

namespace sgn::retail
{
// Borrowed values of vehicle+26B0 -> descriptor+48 -> part0+8. These are
// source LOD0's live transform axes, independently of selected render LOD.
struct CameraMountedModelAxes
{
    CameraVector up{}, forward{};
};
// Full404450 mode6: common40445D..404478, branch404D01..404FAC,
// including403460's mode6 guard and403340. Integrators remain caller-owned.
void camera_mounted_tick(CameraState &, CameraVehicle &, const CameraMountedModelAxes &);
class SourcePmdModelView;
class ModelResources;
// Optional receiving interposition at the two actual model-read boundaries.
// May change the model allocation/handle; camera and physical observations stay stable.
struct CameraMountedAudit
{
    void *context{};
    void (*before_axis)(void *, std::uint32_t source_site){};
};
// Independent raw48/part0/transform reads at404E1C and404EC8. No render LOD dependency.
void camera_mounted_tick(CameraState &, CameraVehicle &, SourcePmdModelView, CameraMountedAudit * = nullptr);
void camera_mounted_tick(CameraState &, CameraVehicle &, ModelResources &, std::size_t, CameraMountedAudit * = nullptr);
} // namespace sgn::retail
