#include "sgn/contact_rail_projection.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
RailProjectionAudit project_rail_position(SplineView spline, Vec3 position, std::uint32_t &persistent_segment,
                                          VehicleRailState &state)
{
    const auto count = spline.descriptor.point_count;
    const auto finite = [](Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (count == 0 || !finite(position))
        throw FormatError("invalid refuel rail projection input");
    std::uint32_t segment = persistent_segment;
    int direction = 0;
    bool adjacent = false;
    for (std::uint64_t examined = 1; examined <= std::uint64_t(count) * 2 + 2; ++examined)
    {
        const auto index = std::bit_cast<std::int32_t>(segment) < 0 ? count - (0u - segment) % count : segment % count;
        // A negative multiple deliberately selects point[count]. Only an
        // existing authored adjacent point is allowed, never an invented wrap.
        const auto first = spline_point(spline, index), next = spline_point(spline, (index + 1u) % count);
        adjacent |= first.adjacent_storage_read || next.adjacent_storage_read;
        float length{};
        const auto axis = vehicle_normalize({vehicle_f32(double(next.value.x) - first.value.x),
                                             vehicle_f32(double(next.value.y) - first.value.y),
                                             vehicle_f32(double(next.value.z) - first.value.z), 0},
                                            &length);
        const double projection =
            ((double(position.z) - first.value.z) * axis.z + (double(position.y) - first.value.y) * axis.y) +
            (double(position.x) - first.value.x) * axis.x;
        const float stored = vehicle_f32(projection);
        if (!std::isfinite(stored) || !std::isfinite(length))
            throw FormatError("refuel rail projection overflow");
        if (projection < 0 && direction <= 0)
        {
            --segment;
            direction = -1;
            continue;
        }
        if (projection >= 0 && stored >= length && direction >= 0)
        {
            ++segment;
            direction = 1;
            continue;
        }
        const Vec3 displacement{vehicle_f32(double(stored) * axis.x), vehicle_f32(double(stored) * axis.y),
                                vehicle_f32(double(stored) * axis.z)};
        const Vec3 point{vehicle_f32(double(displacement.x) + first.value.x),
                         vehicle_f32(double(displacement.y) + first.value.y),
                         vehicle_f32(double(displacement.z) + first.value.z)};
        if (!finite(point))
            throw FormatError("refuel rail projected position overflow");
        persistent_segment = segment;
        state.projected_position = point;
        state.progress = (segment * 100u) / count; // exact wrapping multiply and unsigned DIV
        return {static_cast<std::uint32_t>(examined), adjacent};
    }
    throw FormatError("original refuel rail projection did not converge within two traversals");
}
} // namespace sgn::retail
