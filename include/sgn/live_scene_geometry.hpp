#pragma once
#include "sgn/scene.hpp"
#include "sgn/retail_vehicle_strip.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/retail_camera_transform.hpp"
namespace sgn::retail
{
struct ViewMatrixArena;
}

namespace sgn
{
// Values represent the source4338E0 blend dispatch, not guessed effect names.
enum class LiveBlend : std::uint8_t
{
    opaque,
    alpha,
    alpha_add,
    add,
    multiply
};
struct LiveMaterial
{
    LiveBlend blend{};
};
struct LiveGeometry
{
    Model model;
    std::vector<LiveMaterial> materials; // one for each surface
};
LiveBlend source_polygon_blend(std::uint8_t flags, bool force_translucent = false) noexcept;
std::uint32_t source_polygon_color(std::uint32_t argb, std::uint8_t alpha, std::uint8_t flags,
                                   bool force_translucent = false) noexcept;
// Pure presentation snapshot of the borrowed world strip. No advancement,
// replacement rings, altered active counts or conventionalized first quad.
LiveGeometry strip_live_geometry(const retail::VehicleStripGeometry &);
// 450150 processes every selected-LOD part in order. Shared transform identities
//  observe the last binding, including a disabled part's identity rotation.
struct LiveModelViews
{
    std::vector<SceneInstance::Part> parts; // visibility only; matrices below own presentation transforms
    std::vector<retail::CameraMatrix> model_to_view;
};
LiveModelViews model_live_views(retail::ModelResource &, unsigned lod, const retail::CameraState &,
                                const retail::CameraModelTransform &);
// Source consumer: read already published part+40 references from the sole
// arena. Never regenerate rotations or allocate during model-view capture.
LiveModelViews model_live_views(retail::ModelResource &, unsigned lod, const retail::CameraState &,
                                const retail::CameraModelTransform &, const retail::ViewMatrixArena &,
                                const retail::CameraMatrix &shared_identity);
LiveGeometry model_live_geometry(retail::ModelResource &, unsigned lod);
} // namespace sgn
