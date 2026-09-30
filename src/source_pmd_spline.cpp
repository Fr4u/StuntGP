#include "sgn/source_pmd_spline.hpp"
#include <limits>

namespace sgn::retail
{
SplineView SourcePmdSpline::borrow() const
{
    const auto track = model_.track();
    const auto record = track.spline_descriptor(index_);
    const auto points = track.spline_point_storage();
    const auto first = points.location().offset;
    // This validates the current relocated pointer and descriptor count against
    // actual authored storage before the original math can read any vector.
    const auto current = track.spline_points(index_);
    const auto at = current.location().offset;
    SplineDescriptor d;
    d.id = record.field<std::uint32_t>(0).value();
    d.point_index = (at - first) / 16;
    d.point_count = current.size();
    for (unsigned i = 0; i < 3; ++i)
        d.opaque[i] = record.field<std::uint32_t>(12 + i * 4).value();
    d.length = record.field<float>(24).value();
    // NEW R18 (DESIGN A1): storage is the checked spline point section only,
    // so an opaque byte elsewhere in the PMD no longer rejects the borrow. The
    // point offset is rebased into that section; point_storage_end=0 makes the
    // section end the authored bound, identical to the former first+size bound.
    d.point_offset = at - first;
    return {d, detail::checked_pmd_bytes(points.owner_, points.offset_, points.length_), 0};
}
std::optional<SplineView> SourcePmdSplineSet::view(std::uint32_t index) const
{
    if (index >= size())
        return std::nullopt;
    return SourcePmdSpline(model_, index).borrow();
}
SplineDescriptor SourcePmdSpline::descriptor() const
{
    return borrow().descriptor;
}
SourcePmdLocation SourcePmdSpline::location() const
{
    return model_.track().spline_descriptor(index_).location();
}
SplinePoint SourcePmdSpline::point(std::uint32_t index) const
{
    return spline_point(borrow(), index);
}
SplineSample SourcePmdSpline::sample(float distance) const
{
    return sample_spline(borrow(), distance);
}
IndexedSplineSample SourcePmdSpline::indexed(float distance) const
{
    return sample_spline_indexed(borrow(), distance);
}
SplineProjection SourcePmdSpline::project(Vec3 position, std::int32_t segment) const
{
    return project_spline(borrow(), position, segment);
}
StartingGrid SourcePmdSpline::grid(const std::array<std::int32_t, 6> &active, std::int32_t global_mode) const
{
    return starting_grid(borrow(), active, global_mode);
}
DistanceStartPlacement SourcePmdSpline::distance_start(float distance) const
{
    return prepare_distance_start(borrow(), distance);
}
RespawnPlacement SourcePmdSpline::respawn(std::int32_t requested_segment, std::span<const float, 128> mask) const
{
    return select_respawn(borrow(), requested_segment, mask);
}
std::optional<SourcePmdSpline> SourcePmdSplineSet::find(std::uint32_t id) const
{
    const auto track = model_.track();
    const auto count = track.spline_count();
    for (std::uint32_t i = 0; i < count; ++i)
        if (track.spline_descriptor(i).field<std::uint32_t>(0).value() == id)
            return SourcePmdSpline(model_, i);
    return std::nullopt;
}
std::array<SourcePmdSpline, 3> SourcePmdSplineSet::ai_routes() const
{
    auto route = [&](std::uint32_t id) {
        auto found = find(id);
        if (!found)
            found = find(1);
        if (!found)
            throw FormatError("source PMD AI route and fallback1 are both absent");
        return *found;
    };
    return {route(3), route(4), route(5)};
}
} // namespace sgn::retail
