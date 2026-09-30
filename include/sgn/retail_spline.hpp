#pragma once
#include "sgn/vehicle_math.hpp"
#include <optional>

namespace sgn::retail
{
struct SplineDescriptor
{
    std::uint32_t id{}, point_index{}, point_count{};
    std::array<std::uint32_t, 3> opaque{};
    float length{};
    std::size_t point_offset{};
};
struct SplineView
{
    SplineDescriptor descriptor;
    std::span<const std::uint8_t> storage;
    std::size_t point_storage_end{}; // zero means the supplied span is point storage
};
// Owns unchanged PMD bytes; views remain valid while this object's storage lives.
class SplineSet
{
    Bytes storage_;
    std::vector<SplineDescriptor> descriptors_;
    std::size_t point_storage_end_{};

  public:
    explicit SplineSet(Bytes pmd); // 0042ee50, with new input validation
    std::span<const SplineDescriptor> descriptors() const
    {
        return descriptors_;
    }
    std::optional<SplineView> find(std::uint32_t id) const; // 00428800, first match
};
struct SplinePoint
{
    VehicleVector value;
    bool adjacent_storage_read{};
};
// Original point[count] accesses are allowed only within authored point storage.
// Padding, relocated descriptors and memory beyond that section are rejected.
// No invented sentinel or modulo repair is used.
SplinePoint spline_point(SplineView, std::uint32_t index);
struct SplineSample
{
    VehicleVector value;
    bool adjacent_storage_read{};
};
SplineSample sample_spline(SplineView, float distance); // 004288d0
struct IndexedSplineSample
{
    VehicleVector value;
    std::uint32_t selected_index{};
    bool adjacent_storage_read{};
};
IndexedSplineSample sample_spline_indexed(SplineView, float distance); // 004289a0
struct SplineProjection
{
    std::int32_t segment{};
    float percent{};
    std::uint32_t segments_examined{};
    bool adjacent_storage_read{};
};
SplineProjection project_spline(SplineView, Vec3 position, std::int32_t segment); // 00405370
struct GridPose
{
    Vec3 position;
    std::array<Vec3, 3> basis;              // right, up, forward; original XYZ only
    std::uint32_t physical_rank{};          // vehicle+24ac; never the segment at +24a4
    std::optional<float> copied_position_w; // defined only in numeric mode 3
};
using StartingGrid = std::array<std::optional<GridPose>, 6>;
// Geometry of 00412bf0. Vehicle reset callees and position side effects belong
// to the vehicle module; this result does not claim to be a full vehicle reset.
StartingGrid starting_grid(SplineView, const std::array<std::int32_t, 6> &active, std::int32_t global_mode);
struct DistanceStartPlacement
{
    VehicleVector position;
    Vec3 heading;
    std::uint32_t selected_index{}, progress_current{}, progress_previous{};
    float progress_percent{};
    std::array<std::uint32_t, 3> route_indices{};
    bool adjacent_storage_read{};
};
// 00412b10..00412bd2: state prepared BEFORE calling full vehicle reset 00412940.
// TimeTrial dispatcher 00443100 supplies spline.length - uint32 start offset.
DistanceStartPlacement prepare_distance_start(SplineView, float distance);
struct RespawnPlacement
{
    VehicleVector position;
    Vec3 heading; // retail local W is undefined and is deliberately absent
    std::int32_t persistent_segment{};
    std::array<std::int32_t, 3> route_indices{}; // +2644,+264c,+2654
    std::uint32_t selected_index{}, advanced_points{};
    bool adjacent_storage_read{};
};
using RespawnMask = std::array<float, 128>;
// 00434a70 called by 00446968..00446979; original key and exact binary32 values.
RespawnMask decode_respawn_mask(std::span<const std::uint8_t> level_config);
RespawnPlacement select_respawn(SplineView, std::int32_t requested_segment,
                                std::span<const float, 128> mask); // 004127e0
// 00405260 route selection only: IDs 3/4/5 each fall back to ID 1.
std::array<SplineView, 3> ai_spline_routes(const SplineSet &);
} // namespace sgn::retail
