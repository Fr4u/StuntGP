#pragma once
#include "sgn/retail_camera.hpp"

namespace sgn::retail
{
// Values read by 0044fbd0, with native matrices replacing guest pointers.
// For a model, rotation is transform+44; for an optional part it is +40.
struct CameraModelTransform
{
    CameraVector position{};
    CameraVector scale{1, 1, 1, 0};
    CameraVector pivot{};
    CameraMatrix rotation{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
// Complete 0044fbd0. Uses a basis already prepared by camera_view_basis.
// The optional part is read only if part_count > 1, matching the original.
// CameraLifecycle.scale is intentionally absent: neither 0044fbd0 nor
// 00450190 reads the camera's scale field. Model/part scale does participate.
CameraMatrix camera_model_view(const CameraState &, const CameraModelTransform &, std::uint32_t part_count = 1,
                               const CameraModelTransform *part = nullptr) noexcept;
// 0044fbd0 with an explicit identity model, including its intermediate stores.
CameraMatrix camera_world_view(const CameraState &) noexcept;
} // namespace sgn::retail
