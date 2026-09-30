#pragma once
#include "sgn/source_pmd_model.hpp"
#include "sgn/live_scene_geometry.hpp"

namespace sgn
{
// Checked PMD source reference plus explicit texture-prefix metadata.
// It owns no PMD vertices, polygons, transforms or per-LOD texture namespace.
// capture() produces the existing modern GPU transport, not a new source owner.
class SourcePmdGeometryAdapter
{
    retail::SourcePmdModelView source_;
    std::string track_texture_prefix_;

  public:
    // Prefix is the explicit current source60DFAC string. No level/script
    // selection or fallback occurs in this adapter; empty means source no-op.
    SourcePmdGeometryAdapter(retail::SourcePmdModelView, std::string track_texture_prefix);
    unsigned source_lod_index(unsigned source_slot) const;
    retail::SourceAllocationRef allocation() const
    {
        return source_.allocation();
    }
    LiveGeometry capture(unsigned source_lod_slot) const;
    // Read-only450150/44FF90+44FBD0 presentation result. It does not allocate
    // or publish the source ViewMatrixArena, transform-tail references or LOD.
    LiveModelViews capture_views(unsigned source_lod_slot, const retail::CameraState &,
                                 const retail::CameraModelTransform &outer) const;
    // Actual Session consumer: only resolves the published canonical tail[0].
    // This neither computes a replacement rotation nor changes arena state.
    LiveModelViews capture_published_views(unsigned source_lod_slot, const retail::CameraState &,
                                           const retail::CameraModelTransform &, const retail::ViewMatrixArena &,
                                           const retail::CameraMatrix &) const;
};
LiveGeometry model_live_geometry(retail::ModelResources &, std::size_t handle, unsigned source_lod_slot,
                                 std::string_view track_prefix);
LiveModelViews model_live_views(retail::ModelResources &, std::size_t handle, unsigned source_lod_slot,
                                const retail::CameraState &, const retail::CameraModelTransform &,
                                const retail::ViewMatrixArena &, const retail::CameraMatrix &);
} // namespace sgn
