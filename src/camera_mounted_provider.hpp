#pragma once
#include "sgn/retail_camera_mounted.hpp"
namespace sgn::retail::detail {
// Synchronous invocation only. Neither callback nor context is retained.
struct MountedCameraProvider {
    void* context;
    void (*orbit_written)(void*,CameraVector);
    CameraVector (*axis)(void*,std::uint32_t source_site);
};
void camera_mounted_core(CameraState&,CameraVehicle&,const MountedCameraProvider&);
CameraVector mounted_axis(SourcePmdModelView,std::uint32_t source_site);
CameraVector mounted_axis(ModelResources&,std::size_t,std::uint32_t source_site);
}
