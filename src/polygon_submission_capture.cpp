#include "sgn/polygon_submission_capture.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "sgn/format_require.hpp"
#include "sgn/host_profiler.hpp"
#include <limits>
#include <thread>
#include <utility>

namespace sgn
{
namespace
{
using sgn::format_check::require;
} // namespace
struct PolygonSubmissionCapture::Impl
{
    enum class Mode
    {
        idle,
        active,
        poisoned
    };
    PolygonDeviceContext &device;
    MaterialD3D12 &backend;
    retail::PhysicalMaterialStorage &materials;
    std::optional<MaterialTextureReplacement> delegate;
    std::uint64_t empty_generation{};
    bool empty_active{};
    const std::thread::id owner = std::this_thread::get_id();
    Mode mode = Mode::idle;
    bool owns_capture = false, uploading = false;
    struct Work
    {
        CapturedPolygonSubmission submission;
        PolygonSubmissionCaptureLimits limits;
        std::shared_ptr<const PolygonGpuTextures> bank;
        std::uint64_t material_generation{};
        bool material_active{};
        std::vector<std::uint32_t> primary_tokens;
    };
    std::unique_ptr<Work> work;
    Impl(PolygonDeviceContext &d, MaterialD3D12 &b, retail::PhysicalMaterialStorage &m, bool untextured = false)
        : device(d), backend(b), materials(m)
    {
        if (untextured)
        {
            require(b.resource_entries() == 0 && m.size() == 0,
                    "untextured capture requires actual empty GPU and physical owners");
            empty_generation = m.generation();
            empty_active = m.active();
        }
        else
            delegate.emplace(b, m); // existing nonempty constructor keeps its original admission
    }
    MaterialTextureReplacement &replacement()
    {
        require(delegate.has_value(), "untextured capture has no texture replacement destination");
        return *delegate;
    }
    void verify_material_lease()
    {
        if (delegate)
        {
            (void)delegate->destination_width(0);
            return;
        }
        require(backend.resource_entries() == 0 && materials.size() == 0 &&
                    materials.generation() == empty_generation && materials.active() == empty_active,
                "empty capture material owner changed");
    }
    void thread() const
    {
        if (std::this_thread::get_id() != owner)
            throw std::logic_error("polygon submission capture used outside owner thread");
    }
    void ready() const
    {
        thread();
        require(mode != Mode::poisoned && !uploading, "polygon submission capture is poisoned or reentrant");
    }
    void poison() noexcept
    {
        mode = Mode::poisoned;
        uploading = false;
    }
    void discard()
    {
        thread();
        poison();
        if (owns_capture && device.capturing())
            device.discard_capture();
        owns_capture = false;
        work.reset();
    }
    ~Impl()
    {
        if (std::this_thread::get_id() != owner)
            std::terminate();
        if (owns_capture)
        {
            try
            {
                if (device.capturing())
                    device.discard_capture();
            }
            catch (...)
            {
                std::terminate();
            }
        }
    }
    void lease() const
    {
        require(work && materials.active() == work->material_active &&
                    materials.generation() == work->material_generation &&
                    materials.size() == work->primary_tokens.size() &&
                    backend.resource_entries() == work->primary_tokens.size(),
                "material lifetime/count changed during source frame");
        for (std::size_t i = 0; i < work->primary_tokens.size(); ++i)
            require(backend.resource_primary_token(static_cast<std::uint32_t>(i)) == work->primary_tokens[i],
                    "resource primary identity changed during source frame");
    }
    void unchanged_bank() const
    {
        lease();
        const auto actual = backend.bindings_snapshot();
        require(same_polygon_bank(*work->bank, *actual), "unobserved resource mutation during source capture");
    }
    PolygonDeviceCaptureLimits remaining() const
    {
        const auto &total = work->limits.total;
        const auto &s = work->submission;
        require(s.commands <= total.polygon.commands && s.payload_bytes <= total.polygon.payload_bytes &&
                    s.events <= total.events,
                "source capture exceeded global budget");
        return {{total.polygon.commands - s.commands, total.polygon.payload_bytes - s.payload_bytes},
                total.events - s.events};
    }
    void healthy() const
    {
        require(owns_capture && device.capturing(), "coordinator lost its active device capture");
        const auto status = device.capture_status();
        require(status && !status->poisoned && status->refused_calls == 0 && status->unlogged_refusals == 0,
                "refused source device operation poisons submission");
    }
    void seal_segment()
    {
        healthy();
        unchanged_bank();
        require(work->submission.segments.size() < work->limits.resource_segments, "resource segment budget exhausted");
        auto frame = device.seal_capture();
        owns_capture = false;
        const auto budget = remaining();
        require(frame->polygon_commands->commands.size() <= budget.polygon.commands &&
                    frame->polygon_commands->payload_bytes <= budget.polygon.payload_bytes &&
                    frame->events.size() <= budget.events,
                "sealed device capture exceeded global budget");
        auto &s = work->submission;
        require(frame->device == s.device && frame->polygon_commands->source_tick == s.source_tick &&
                    frame->polygon_commands->resource_epoch == work->bank->epoch,
                "sealed segment identity differs");
        const auto &prior = s.segments.empty() ? s.initial : s.segments.back().device_frame->final;
        require(same_polygon_snapshot(prior, frame->initial), "sealed segment CPU state differs from prefix");
        s.commands += frame->polygon_commands->commands.size();
        s.payload_bytes += frame->polygon_commands->payload_bytes;
        s.events += frame->events.size();
        s.final = frame->final;
        s.segments.push_back({std::move(frame), work->bank}); // Capacity reserved before any source work.
    }
};
PolygonSubmissionCapture::PolygonSubmissionCapture(PolygonDeviceContext &d, MaterialD3D12 &b,
                                                   retail::PhysicalMaterialStorage &m)
    : impl_(std::make_unique<Impl>(d, b, m))
{
}
PolygonSubmissionCapture::PolygonSubmissionCapture(SourceUntexturedCaptureOnly, PolygonDeviceContext &d,
                                                   MaterialD3D12 &b, retail::PhysicalMaterialStorage &m)
    : impl_(std::make_unique<Impl>(d, b, m, true))
{
}
PolygonSubmissionCapture::~PolygonSubmissionCapture() = default;
PolygonDeviceContext &PolygonSubmissionCapture::device() noexcept
{
    return impl_->device;
}
bool PolygonSubmissionCapture::capturing() const noexcept
{
    return impl_->mode == Impl::Mode::active;
}
bool PolygonSubmissionCapture::poisoned() const noexcept
{
    return impl_->mode == Impl::Mode::poisoned;
}
void PolygonSubmissionCapture::begin_source_frame(std::uint64_t tick, PolygonSubmissionCaptureLimits limits)
{
    // NEW HOST (R30): bank snapshot/validation, lease tokens and the device capture's reserved buffers.
    SGN_PROFILE_SCOPE("capture.begin_frame");
    auto &p = *impl_;
    p.ready();
    require(p.mode == Impl::Mode::idle && !p.device.capturing(), "source capture already active or foreign");
    require(limits.resource_segments && limits.total.polygon.commands && limits.total.events,
            "source capture requires nonzero budgets");
    // This verifies the concrete delegate's persistent generation/primary lease.
    p.verify_material_lease();
    auto next = std::make_unique<Impl::Work>();
    next->limits = limits;
    next->bank = p.backend.bindings_snapshot();
    validate_polygon_bank(*next->bank);
    next->material_generation = p.materials.generation();
    next->material_active = p.materials.active();
    const auto count = p.backend.resource_entries();
    require((p.delegate.has_value() ? p.materials.active() : count == 0) && p.materials.size() == count,
            "source capture material bank is incomplete");
    next->primary_tokens.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
        next->primary_tokens.push_back(p.backend.resource_primary_token(i));
    next->submission.device = p.device.identity();
    next->submission.source_tick = tick;
    next->submission.initial = next->submission.final = p.device.snapshot();
    next->submission.segments.reserve(limits.resource_segments);
    p.device.begin_capture(tick, next->bank->epoch, limits.total);
    p.owns_capture = true;
    p.work = std::move(next);
    p.mode = Impl::Mode::active;
}
std::uint16_t PolygonSubmissionCapture::destination_width(std::uint32_t slot) const
{
    auto &p = *impl_;
    p.thread();
    try
    {
        p.ready();
        return p.replacement().destination_width(slot);
    }
    catch (...)
    {
        if (p.mode == Impl::Mode::active)
            p.poison();
        throw;
    }
}
void PolygonSubmissionCapture::upload_to_destination(std::uint32_t slot, const Image &image)
{
    auto &p = *impl_;
    p.thread();
    try
    {
        p.ready();
        if (p.mode == Impl::Mode::idle)
        {
            p.replacement().upload_to_destination(slot, image);
            return;
        }
        require(p.mode == Impl::Mode::active, "upload requires healthy coordinator");
        p.healthy();
        p.unchanged_bank();
        const auto status = *p.device.capture_status();
        const auto available = p.remaining();
        require(p.work->submission.segments.size() + 1 < p.work->limits.resource_segments &&
                    status.commands < available.polygon.commands && status.events < available.events,
                "no capture segment/event/command budget remains after replacement");
        const auto token = p.backend.resource_primary_token(slot);
        SGN_PROFILE_SCOPE("capture.replacement_segment"); // NEW HOST (R30): seal + upload + reopen
        p.seal_segment();
        const auto prior = p.work->submission.final;
        p.uploading = true;
        p.replacement().upload_to_destination(slot, image); // The sole actual upload call on the active path.
        p.lease();
        const auto next = p.backend.bindings_snapshot();
        validate_polygon_replacement(*p.work->bank, *next, token);
        require(same_polygon_snapshot(prior, p.device.snapshot()), "texture upload changed source CPU device state");
        p.work->bank = next;
        p.device.begin_capture(p.work->submission.source_tick, next->epoch, p.remaining());
        p.owns_capture = true;
        p.uploading = false;
    }
    catch (...)
    {
        if (p.mode == Impl::Mode::active)
            p.poison();
        throw;
    }
}
ImmutablePolygonSubmission PolygonSubmissionCapture::seal_source_frame()
{
    SGN_PROFILE_SCOPE("capture.seal_frame"); // NEW HOST (R30): bank recheck, device seal, validation
    auto &p = *impl_;
    p.thread();
    try
    {
        p.ready();
        require(p.mode == Impl::Mode::active, "no active source frame");
        p.seal_segment();
        (void)validate_polygon_submission(p.work->submission);
        auto result = std::make_shared<const CapturedPolygonSubmission>(std::move(p.work->submission));
        p.work.reset();
        p.mode = Impl::Mode::idle;
        return result;
    }
    catch (...)
    {
        p.poison();
        throw;
    }
}
void PolygonSubmissionCapture::release_image_resources_431810()
{
    auto &p = *impl_;
    p.ready();
    require(p.mode == Impl::Mode::idle && !p.owns_capture && !p.work && !p.device.capturing() &&
                !p.device.snapshot().scene_open,
            "source image release requires a healthy closed idle capture owner");
    // Prove the complete existing lease BEFORE releasing anything. A foreign
    // bank can have the same count and token0, so the Session bridge also
    // checks the exact backend that completed that world's loading.
    p.verify_material_lease();
    const auto count = p.backend.resource_entries();
    for (std::uint32_t slot = 1; slot < count; ++slot)
        (void)p.replacement().destination_width(slot);
    (void)p.backend.bindings_snapshot(); // Reject failed/suspended backend before mutation.
    // Pinned D3D431810 is only E9 7B FF FF FF. Its431790 target releases
    // reverse resource entries via422880/4300F0 and clears5D91CC/5D91D0.
    // The existing D3D12 boundary drops only this live bank's ownership;
    // immutable captures/recordings retain their own resources until retired.
    p.backend.release_all();
}
void PolygonSubmissionCapture::suspend_renderer_for_recreation(ImmutablePolygonSubmission &displayed)
{
    suspend_renderer_for_recreation(std::span{&displayed, std::size_t(1)});
}
void PolygonSubmissionCapture::suspend_renderer_for_recreation(std::span<ImmutablePolygonSubmission> retained)
{
    auto &p = *impl_;
    p.ready();
    require(p.mode == Impl::Mode::idle && !p.device.capturing() && !p.device.snapshot().scene_open,
            "device recreation requires a healthy closed source boundary");
    p.verify_material_lease(); // Existing physical generation/count/token lease.
    for (const auto &frame : retained)
        require(!frame || frame->device == p.device.identity(), "suspend frame belongs to another context");
    p.backend.suspend_renderer_for_recreation(retained);
}
void PolygonSubmissionCapture::rebind_renderer(Renderer &renderer, ImmutablePolygonSubmission &displayed)
{
    auto &p = *impl_;
    p.ready();
    require(p.mode == Impl::Mode::idle && !p.device.capturing() && !p.device.snapshot().scene_open,
            "device rebind requires a healthy closed source boundary");
    p.verify_material_lease();
    require(!displayed || displayed->device == p.device.identity(), "device rebind frame belongs to another context");
    p.backend.rebind_renderer(renderer, displayed);
}
PolygonSurfaceRoles PolygonSubmissionCapture::rebind_renderer(Renderer &renderer,
                                                              std::array<ImmutablePolygonSubmission, 2> &retained,
                                                              const std::array<SdrGammaRamp, 2> &ramps)
{
    auto &p = *impl_;
    p.ready();
    require(p.mode == Impl::Mode::idle && !p.device.capturing() && !p.device.snapshot().scene_open,
            "device rebind requires a healthy closed source boundary");
    p.verify_material_lease();
    for (const auto &frame : retained)
        require(!frame || frame->device == p.device.identity(), "device rebind frame belongs to another context");
    return p.backend.rebind_renderer(renderer, retained, ramps);
}
void PolygonSubmissionCapture::discard_failed_source_frame()
{
    impl_->discard();
}
} // namespace sgn
