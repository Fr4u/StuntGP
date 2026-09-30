#pragma once
#include "sgn/source_pmd_model.hpp"
#include "sgn/retail_spline.hpp"

namespace sgn::retail
{
class SourcePmdSplineSet;
// A checked reference to one physical descriptor in the sole bank allocation.
// The legacy SplineView exists only inside synchronous calls into recovered
// math. It is never stored in this object or passed to a callback.
// NEW R18 policy (DESIGN A1, root approval): SourcePmdSplineSet::view may
// return it as a call-scoped value to recovered native math through
// spline_resolution.hpp; its storage is only the checked spline point section.
// Persistent writers store identities (CameraSplineBinding::bind_source), never
// a SplineView.
class SourcePmdSpline
{
    SourcePmdModelView model_;
    std::uint32_t index_{};
    SourcePmdSpline(SourcePmdModelView model, std::uint32_t index) : model_(std::move(model)), index_(index) {}
    SplineView borrow() const;
    friend class SourcePmdSplineSet;

  public:
    SplineDescriptor descriptor() const; // value snapshot; not authoritative storage
    SourcePmdLocation location() const;
    SplinePoint point(std::uint32_t index) const;
    SplineSample sample(float distance) const;
    IndexedSplineSample indexed(float distance) const;
    SplineProjection project(Vec3 position, std::int32_t segment) const;
    StartingGrid grid(const std::array<std::int32_t, 6> &active, std::int32_t global_mode) const;
    DistanceStartPlacement distance_start(float distance) const;
    RespawnPlacement respawn(std::int32_t requested_segment, std::span<const float, 128> mask) const;
};
class SourcePmdSplineSet
{
    SourcePmdModelView model_;

  public:
    explicit SourcePmdSplineSet(SourcePmdModelView model) : model_(std::move(model))
    {
        (void)model_.track();
    }
    std::uint32_t size() const
    {
        return model_.track().spline_count();
    }
    std::optional<SourcePmdSpline> find(std::uint32_t id) const; // 428800 first current match
    std::array<SourcePmdSpline, 3> ai_routes() const;            // 405260 IDs3/4/5 fallback1
    // NEW R18: call-scoped view of descriptor `index` (empty if index>=size()).
    std::optional<SplineView> view(std::uint32_t index) const;
};
} // namespace sgn::retail
