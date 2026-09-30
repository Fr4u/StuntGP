#pragma once
#include "sgn/retail_polygon_submission.hpp"
#include <memory>
#include <optional>
#include <variant>

namespace sgn
{
// New host presentation data, not additional authoritative game owners.
// Texture numbers remain source namespace tokens. The renderer must retain
// the matching texture resources until all frames in resource_epoch retire.
struct CapturedRenderState
{
    std::uint32_t state, value;
};
struct CapturedTexture
{
    std::uint32_t stage, token;
};
struct CapturedTextureStageState
{
    std::uint32_t stage, state, value;
};
struct CapturedPolygonDraw
{
    std::uint32_t type{}, fvf{}, flags{};
    bool indexed{};
    retail::ViewportState viewport;
    std::vector<retail::PolygonSubmissionVertex> vertices;
    std::vector<std::uint16_t> indices;
};
using CapturedPolygonCommand =
    std::variant<CapturedRenderState, CapturedTexture, CapturedTextureStageState, CapturedPolygonDraw>;
// NEW R19 host GPU work (perf_gpu/gpu_world), not a source device command: it
// has no device event, no source HRESULT and is never replayed or exported.
// The GPU recorder executes it exactly before commands[before_command]
// (before_command==commands.size(): after the last command). Ordered.
// A PLANNED insertion (draw present) is planned by the recorder like an FVF
// 0x2C4 draw of that type/flags/viewport at its position (same state, texture,
// sampler and constant validation; no vertex payload) and receives that plan.
class PolygonGpuCommand;
struct PolygonGpuDrawRequest
{
    std::uint32_t type{}, flags{};
    retail::ViewportState viewport{};
};
struct PolygonGpuInsertion
{
    std::size_t before_command{};
    std::shared_ptr<const PolygonGpuCommand> command;
    std::optional<PolygonGpuDrawRequest> draw;
};
struct CapturedPolygonFrame
{
    std::uint64_t source_tick{}, resource_epoch{};
    std::vector<CapturedPolygonCommand> commands;
    std::size_t payload_bytes{};
    std::vector<PolygonGpuInsertion> gpu_insertions{}; // NEW R19, default empty
};
using ImmutablePolygonFrame = std::shared_ptr<const CapturedPolygonFrame>;
// NEW HOST (R30 perf-tick): a mutable deep copy of `source`, equal to CapturedPolygonFrame(source) in
// every field and element, built in storage recycled from retired frames (the capture's pool); the copy
// returns its storage to that pool when its last owner releases it. For host presentation copies made
// at display rate (interpolation, wide menu), which otherwise allocate every draw's vectors per present.
std::shared_ptr<CapturedPolygonFrame> copy_polygon_frame_recycled(const CapturedPolygonFrame &source);
struct PolygonCaptureLimits
{
    std::size_t commands, payload_bytes;
};
// Accepts synchronous submission calls and copies all borrowed draw data.
// Returning S_OK means a CPU command was recorded, not that the GPU drew it.
// A failed allocation/host guard leaves the already recorded prefix intact.
class PolygonCommandCapture final : public retail::PolygonSubmissionSink
{
    CapturedPolygonFrame frame_;
    PolygonCaptureLimits limits_;
    bool sealed_{};
    void check_capacity(std::size_t payload) const;
    std::uint32_t append_draw(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>,
                              std::span<const std::uint16_t>, bool, std::uint32_t, const retail::ViewportState &,
                              bool indices_checked = false);

  public:
    PolygonCommandCapture(std::uint64_t source_tick, std::uint64_t resource_epoch, PolygonCaptureLimits);
    PolygonCommandCapture(const PolygonCommandCapture &) = delete;
    PolygonCommandCapture &operator=(const PolygonCommandCapture &) = delete;
    std::uint32_t set_render_state(std::uint32_t, std::uint32_t) override;
    std::uint32_t set_texture(std::uint32_t, std::uint32_t) override;
    std::uint32_t set_texture_stage_state(std::uint32_t, std::uint32_t, std::uint32_t) override;
    std::uint32_t draw_indexed(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>,
                               std::span<const std::uint16_t>, std::uint32_t, const retail::ViewportState &) override;
    std::uint32_t draw(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>, std::uint32_t,
                       const retail::ViewportState &) override;
    // NEW R19: host GPU command at the current stream position (see above).
    // Throws FormatError when sealed or null; consumes no command budget.
    void insert_gpu_command(std::shared_ptr<const PolygonGpuCommand>, std::optional<PolygonGpuDrawRequest> draw = {});
    // NEW R19 internal fast path for PolygonDeviceContext, which has already
    // proven every index < vertices.size() with the identical predicate.
    std::uint32_t append_prevalidated_draw(std::uint32_t type, std::uint32_t fvf,
                                           std::span<const retail::PolygonSubmissionVertex>,
                                           std::span<const std::uint16_t>, bool indexed, std::uint32_t flags,
                                           const retail::ViewportState &);
    ImmutablePolygonFrame seal();
    std::size_t command_count() const noexcept
    {
        return frame_.commands.size();
    }
    std::size_t payload_bytes() const noexcept
    {
        return frame_.payload_bytes;
    }
};
struct PolygonReplayError : std::runtime_error
{
    std::size_t command;
    std::uint32_t result;
    PolygonReplayError(std::size_t, std::uint32_t);
};
// Presentation-only replay. It never calls a game producer, advances a clock
// or accesses the Session. Epoch mismatch fails before any backend operation.
// Backend HRESULT failure is a new host error; it cannot rewind source state.
void replay_polygon_commands(const CapturedPolygonFrame &, std::uint64_t expected_resource_epoch,
                             retail::PolygonSubmissionSink &);
} // namespace sgn
