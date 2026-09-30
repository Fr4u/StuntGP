#pragma once
#include "sgn/polygon_d3d12.hpp" // Existing location of PolygonDeviceState; no GPU calls here.
#include "sgn/retail_device_setup.hpp"
#include <array>
#include <memory>
#include <optional>
#include <vector>
#include <variant>

namespace sgn
{
// New CPU platform receiver, not a recovered driver or a source-global owner.
// Unknown entries stay unknown. Light state is a separate API namespace.
using PolygonDeviceLightState = std::array<std::optional<std::uint32_t>, 256>;
struct PolygonDeviceIdentity final
{
};
using PolygonDeviceIdentityRef = std::shared_ptr<const PolygonDeviceIdentity>;
// NEW HOST transform state for this pretransformed-only receiver. Source431420
// passes an unwritten stack region to slot3; preserve that fact, never invent
// matrix coefficients or read uninitialized C++ storage. No hardware T&L claim.
struct PolygonSourceUninitializedTransform
{
    bool operator==(const PolygonSourceUninitializedTransform &) const = default;
};
using PolygonDeviceTransform =
    std::variant<std::monostate, std::array<std::uint32_t, 16>, PolygonSourceUninitializedTransform>;
struct PolygonDeviceSnapshot
{
    PolygonDeviceState polygon;
    PolygonDeviceLightState light;
    bool scene_open{};
    std::uint64_t operation_sequence{};
    std::array<PolygonDeviceTransform, 3> transforms{}; // device slots1,2,3; initially unknown
};
enum class PolygonDeviceOperation
{
    begin_scene,
    end_scene,
    render_state,
    light_state,
    texture,
    texture_stage_state,
    draw_indexed,
    draw,
    transform
};
enum class PolygonDeviceRejection
{
    none,
    scene_order,
    missing_capture,
    selector,
    draw_layout,
    draw_index,
    capacity,
    allocation
};
struct PolygonDeviceCall
{
    std::uint64_t sequence{};
    PolygonDeviceOperation operation{};
    std::array<std::uint32_t, 3> arguments{};
    std::uint32_t hresult{};
    bool cpu_accepted{};
    PolygonDeviceRejection rejection{};
    std::optional<std::size_t> polygon_command;
    std::optional<PolygonDeviceTransform> transform_value; // owned CPU event payload, never a source matrix bank
};
struct PolygonDeviceCaptureLimits
{
    PolygonCaptureLimits polygon;
    std::size_t events;
};
struct PolygonDeviceCaptureStatus
{
    bool poisoned{};
    std::size_t commands{}, payload_bytes{}, events{};
    std::uint64_t refused_calls{}, unlogged_refusals{};
    std::optional<PolygonDeviceCall> first_refusal, last_refusal;
};
struct CapturedPolygonDeviceFrame
{
    PolygonDeviceIdentityRef device;
    PolygonDeviceSnapshot initial, final;
    ImmutablePolygonFrame polygon_commands;
    // Begin/End/light operations are CPU state events, not FVF2C4 GPU commands.
    std::vector<PolygonDeviceCall> events;
};
using ImmutablePolygonDeviceFrame = std::shared_ptr<const CapturedPolygonDeviceFrame>;

class PolygonDeviceContext final : public retail::DeviceSetupDevice, public retail::PolygonSubmissionSink
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    // Initial polygon values are mandatory caller input. Empty light entries
    // mean unknown, never an inferred Direct3D default. A new host scene is closed.
    explicit PolygonDeviceContext(PolygonDeviceState, PolygonDeviceLightState = {});
    ~PolygonDeviceContext();
    PolygonDeviceContext(const PolygonDeviceContext &) = delete;
    PolygonDeviceContext &operator=(const PolygonDeviceContext &) = delete;
    PolygonDeviceContext(PolygonDeviceContext &&) = delete;
    PolygonDeviceContext &operator=(PolygonDeviceContext &&) = delete;
    PolygonDeviceIdentityRef identity() const noexcept;
    PolygonDeviceSnapshot snapshot() const noexcept;
    std::optional<PolygonDeviceCall> last_call() const noexcept;
    bool capturing() const noexcept;
    std::optional<PolygonDeviceCaptureStatus> capture_status() const noexcept;
    // Capture bounds and scene bounds are independent. Begin/End must pair;
    // draw requires BOTH an open scene and an active capture. Sealing/discarding
    // does not close the scene, roll back state, change an epoch or advance time.
    // Any refused CPU API call poisons an active capture. Source calls can still
    // finish with literal HRESULTs, but seal refuses to publish it as a complete
    // frame. Inspect capture_status(), then explicitly discard and recover.
    void begin_capture(std::uint64_t source_tick, std::uint64_t resource_epoch, PolygonDeviceCaptureLimits);
    ImmutablePolygonDeviceFrame seal_capture();
    void discard_capture();
    // NEW R19 host GPU command at the current capture position (not a source
    // device call: no event, sequence or HRESULT). Requires capture+open scene.
    void insert_gpu_command(std::shared_ptr<const PolygonGpuCommand>, std::optional<PolygonGpuDrawRequest> draw = {});
    // These HRESULTs describe this CPU receiver only: S_OK means a state update
    // or owned command was accepted, not a GPU draw or historical driver result.
    static constexpr std::uint32_t ok = 0, invalid_argument = 0x80070057u, invalid_order = 0x8000ffffu,
                                   out_of_memory = 0x8007000eu;
    std::uint32_t begin_scene() override;
    std::uint32_t end_scene() override;
    std::uint32_t set_render_state(std::uint32_t, std::uint32_t) override;
    std::uint32_t set_light_state(std::uint32_t, std::uint32_t) override;
    // Both update this real CPU receiver and its diagnostic event sequence.
    // HRESULT is NEW HOST acceptance; source431420 ignores it. FVF2C4 draw
    // already contains transformed vertices and never reads these matrices.
    std::uint32_t set_transform_represented(std::uint32_t, std::span<const std::uint32_t, 16>);
    std::uint32_t set_transform_uninitialized_source_stack(std::uint32_t);
    std::uint32_t set_texture(std::uint32_t, std::uint32_t) override;
    std::uint32_t set_texture_stage_state(std::uint32_t, std::uint32_t, std::uint32_t) override;
    std::uint32_t draw_indexed(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>,
                               std::span<const std::uint16_t>, std::uint32_t, const retail::ViewportState &) override;
    std::uint32_t draw(std::uint32_t, std::uint32_t, std::span<const retail::PolygonSubmissionVertex>, std::uint32_t,
                       const retail::ViewportState &) override;
};

// Independent CPU surface receiver for the 61C358 boundary. It never accesses a
// material texture, a device context, or source-global storage.
struct PolygonDeviceSurfaceKey
{
    std::uint32_t flags;
    std::array<std::uint32_t, 2> values;
};
class PolygonDeviceSurface final : public retail::DeviceSetupSurface
{
    std::optional<PolygonDeviceSurfaceKey> key_;

  public:
    explicit PolygonDeviceSurface(std::optional<PolygonDeviceSurfaceKey> initial) : key_(initial) {}
    std::uint32_t set_color_key(std::uint32_t, std::span<const std::uint32_t, 2>) override;
    std::optional<PolygonDeviceSurfaceKey> color_key() const noexcept
    {
        return key_;
    }
};
} // namespace sgn
