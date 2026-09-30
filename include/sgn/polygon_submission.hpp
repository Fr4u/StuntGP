#pragma once
#include "sgn/polygon_device_context.hpp"

namespace sgn
{
struct PolygonResourceSegment
{
    ImmutablePolygonDeviceFrame device_frame;
    std::shared_ptr<const PolygonGpuTextures> textures;
};
struct CapturedPolygonSubmission
{
    PolygonDeviceIdentityRef device;
    std::uint64_t source_tick{};
    PolygonDeviceSnapshot initial, final;
    std::vector<PolygonResourceSegment> segments;
    std::size_t commands{}, payload_bytes{}, events{};
};
using ImmutablePolygonSubmission = std::shared_ptr<const CapturedPolygonSubmission>;
struct PolygonSubmissionWorkload
{
    std::size_t draws{}, upload_bytes{};
};
inline constexpr std::size_t polygon_submission_draw_limit = polygon_gpu_srv_draw_limit;
inline constexpr std::size_t polygon_submission_upload_limit = 256u * 1024u * 1024u;

bool same_polygon_state(const PolygonDeviceState &, const PolygonDeviceState &) noexcept;
bool same_polygon_snapshot(const PolygonDeviceSnapshot &, const PolygonDeviceSnapshot &) noexcept;
// Resource handles and creation metadata, not source material alias values.
bool same_polygon_bank(const PolygonGpuTextures &, const PolygonGpuTextures &) noexcept;
void validate_polygon_bank(const PolygonGpuTextures &);
// Exactly one resource changes; its source token/format/metadata stay fixed.
void validate_polygon_replacement(const PolygonGpuTextures &, const PolygonGpuTextures &,
                                  std::optional<std::uint32_t> expected_primary = {});
// CPU preflight only. GPU format/device/state validation remains in record().
PolygonSubmissionWorkload polygon_submission_workload(const CapturedPolygonFrame &);
PolygonSubmissionWorkload validate_polygon_submission(const CapturedPolygonSubmission &);
} // namespace sgn
