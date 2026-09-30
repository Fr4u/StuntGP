#include "sgn/source_menu_presentation_backend.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include "sgn/material_texture_replacement.hpp"
#include "sgn/host_wide_menu.hpp"
#include "sgn/host_wide_layout.hpp"
#include "sgn/host_graphics_settings.hpp"
#include "sgn/host_frame_phase.hpp"
#include "sgn/host_load_timeline.hpp"
#include "sgn/host_profiler.hpp"

namespace sgn
{
namespace
{
// NEW HOST (R23): live host_graphics::Settings::original_4x3. Applied at a
// source frame boundary (BeginScene, before any capture of the frame), so the
// whole frame (race Hor+ factors, wide_hud overlay mapping, wide_menu tags)
// and every later rasterization use one layout. Owner thread only (the
// source draw thread; one backend per process).
struct AspectSetting
{
    bool known{};
    std::uint64_t revision{};
    bool original_4x3{};
};
AspectSetting &aspect_setting() noexcept
{
    static AspectSetting state;
    return state;
}
// True when host_wide::original_4x3() changed.
bool apply_aspect_setting() noexcept
{
    auto &a = aspect_setting();
    const auto revision = host_graphics::revision();
    if (a.known && revision == a.revision)
        return false;
    const bool wanted = host_graphics::current().original_4x3;
    const bool first = !a.known;
    a.known = true;
    a.revision = revision;
    if (first)
    {
        // Baseline: the command line / SGN_ASPECT (host_wide::configure) chose
        // the start mode. A settings file asking for 4:3 is honoured; a file
        // asking for widescreen never overrides an explicit --aspect-4x3.
        a.original_4x3 = wanted;
        if (!wanted || host_wide::original_4x3())
            return false;
    }
    else
    {
        if (wanted == a.original_4x3)
            return false; // another setting changed
        a.original_4x3 = wanted;
        if (host_wide::original_4x3() == wanted)
            return false;
    }
    host_wide::set_original_4x3(wanted);
    return true;
}
} // namespace
SourceMenuPresentationBackend::SourceMenuPresentationBackend(retail::FrontendState &frontend,
                                                             retail::SessionOuterFrameServices &frames)
    : frontend_(frontend), frames_(frames)
{
    frames_.require_source_frontend(frontend_);
}
SourceMenuPresentationBackend::~SourceMenuPresentationBackend() = default;
const SourceMenuPresentationBackend::Binding &SourceMenuPresentationBackend::binding() const
{
    frames_.require_source_frontend(frontend_);
    if (failed_ || !bound_ || !bound_->renderer)
        throw FormatError("menu presentation backend is failed or not yet bound to the actual renderer");
    const auto &b = *bound_;
    b.surfaces.require_binding(*b.renderer, b.context);
    if (!b.materials.bound_to_renderer(*b.renderer))
        throw FormatError("menu presentation materials belong to another or suspended renderer");
    return b;
}
SourceMenuPresentationBackend::Binding &SourceMenuPresentationBackend::binding()
{
    (void)static_cast<const SourceMenuPresentationBackend &>(*this).binding();
    return *bound_;
}
void SourceMenuPresentationBackend::bind(Renderer &renderer, PolygonDeviceContext &context, MaterialD3D12 &materials,
                                         RendererLoadingSurfaces &surfaces, LightGammaD3D12 &gamma,
                                         PolygonSubmissionCaptureLimits limits)
{
    frames_.require_source_frontend(frontend_);
    if (bound_ || failed_ || frame_)
        throw FormatError("menu presentation backend can bind only once before source work");
    surfaces.require_binding(renderer, context);
    if (!materials.bound_to_renderer(renderer) || context.capturing() || context.snapshot().scene_open)
        throw FormatError("menu presentation binding needs the same idle closed renderer/material/context owners");
    if (!limits.resource_segments || !limits.total.events || !limits.total.polygon.commands)
        throw FormatError("menu capture requires explicit nonzero host budgets");
    (void)gamma.snapshot();
    frames_.bind_source_backend(materials, gamma);
    bound_.emplace(Binding{&renderer, context, materials, surfaces, gamma, limits});
    host_presentation::set_region_observer(&host_frame_phase::region_changed); // NEW HOST (R28) phase markers
}
void SourceMenuPresentationBackend::discard_failed_frame()
{
    frontend_.shared_platform()->require_owner_thread();
    failed_ = true;
    host_frame_phase::end_capture(); // NEW HOST (R28)
    host_presentation::abandon_frame();
    host_wide_menu::abandon_frame(); // NEW R20 (wide_menu)
    if (frame_)
        frame_->discard_failed_source_frame();
    frame_.reset(); // context scene state and source words deliberately retain the failed prefix
}
const SourceCompletedFrame *SourceMenuPresentationBackend::last_completed_frame() const noexcept
{
    return last_completed_ ? &*last_completed_ : nullptr;
}
retail::LightGammaDevice &SourceMenuPresentationBackend::gamma_device() const &
{
    return binding().gamma;
}
PolygonDeviceContext &SourceMenuPresentationBackend::active_device()
{
    auto &b = binding();
    if (!frame_ || !frame_->capturing() || !b.context.capturing())
        throw FormatError("menu polygon call requires its active source frame capture");
    return b.context;
}
std::uint32_t SourceMenuPresentationBackend::surface_operation_432a60(std::uint32_t target, std::uint32_t flags)
{
    SGN_PROFILE_SCOPE("menu.flip_present"); // NEW HOST (R30) diagnostics: flip + Present of the front
    auto &b = binding();
    if (frame_ || b.context.capturing() || b.context.snapshot().scene_open)
        throw FormatError("menu Flip reached before the preceding source frame closed");
    try
    {
        b.surfaces.publish_pending_back();
        const auto result = b.surfaces.surface_operation_432a60(target, flags);
        // NEW HOST (interp R19): while interpolated frames are presented, the
        // canonical race Flip (4427C9) keeps its surface operation and result
        // but does not Present; loading/menu Flips are never suppressed.
        if (result == 0 && !host_presentation::suppress_current_present())
        {
            (void)b.surfaces.draw(*b.renderer);
            // NEW HOST (R30) load timeline: process start -> first presented frame (boot/loading/menu).
            static bool first_present_noted = false;
            if (!first_present_noted)
            {
                first_present_noted = true;
                host_load_timeline::milestone("first_present");
            }
        }
        // Real Present of the newly selected FRONT, including inside445940's
        // synchronous loop. Never display the just-drawn BACK prematurely.
        return result;
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::begin_scene()
{
    SGN_PROFILE_SCOPE("menu.begin_scene"); // NEW HOST (R30) diagnostics
    auto &b = binding();
    if (frame_ || b.context.capturing() || b.context.snapshot().scene_open)
        throw FormatError("menu BeginScene requires an idle closed capture owner");
    // NEW HOST (R23): original_4x3 switched in the options -> new layout from
    // this frame on; retained images re-rasterize lazily with it.
    if (apply_aspect_setting())
        b.surfaces.invalidate_presentation_images();
    // Construct a fresh lease after real42C480/uploads, against this same table.
    auto &storage = frames_.source_capture_storage(b.materials);
    auto next =
        b.materials.resource_entries() == 0 && storage.size() == 0
            ? std::make_unique<PolygonSubmissionCapture>(SourceUntexturedCaptureOnly{}, b.context, b.materials, storage)
            : std::make_unique<PolygonSubmissionCapture>(b.context, b.materials, storage);
    next->begin_source_frame(frontend_.world().simulation_tick, b.limits);
    host_presentation::begin_frame(); // NEW R19 (interp) host tags, no source state
    host_wide_menu::begin_frame();    // NEW R20 (wide_menu) host tags, no source state
    frame_ = std::move(next);
    // NEW HOST (R28): race region transitions become phase markers of this capture (HUD split, bloom).
    host_frame_phase::begin_capture(&b.context, [](void *owner, std::shared_ptr<const PolygonGpuCommand> command) {
        static_cast<PolygonDeviceContext *>(owner)->insert_gpu_command(std::move(command));
    });
    try
    {
        const auto result = b.context.begin_scene();
        if (result != 0)
            discard_failed_frame();
        return result;
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::end_scene()
{
    SGN_PROFILE_SCOPE("menu.end_scene"); // NEW HOST (R30) diagnostics: seal + widened copy + stage/raster
    auto &b = binding();
    host_frame_phase::end_capture(); // NEW HOST (R28)
    try
    {
        const auto result = active_device().end_scene();
        if (result != 0)
        {
            discard_failed_frame();
            return result;
        }
        auto submission = frame_->seal_source_frame();
        frame_.reset();
        auto ramp = b.gamma.snapshot();
        // NEW R19 (interp): retain the sealed frame (shared, immutable) and its tags.
        last_completed_ = SourceCompletedFrame{submission, ramp, ++completed_serial_, host_presentation::end_frame()};
        // NEW R22: a canonical race frame whose Present the active high-refresh
        // presenter suppresses is never displayed as such; its image is
        // rasterized only if a later consumer needs it (renderer_loading_surfaces).
        const bool defer_raster = last_completed_->tags && last_completed_->tags->race_view &&
                                  host_presentation::race_present_suppression_armed();
        // NEW R20 (wide_menu): associate the sealed recipe with its frontend tags;
        // the surfaces rasterize a widened presentation copy (host_wide_menu.hpp).
        host_wide_menu::remember(submission, host_wide_menu::end_frame());
        if (defer_raster)
            b.surfaces.completed_back_deferred_raster(std::move(submission), ramp);
        else
            b.surfaces.completed_back(std::move(submission), ramp);
        return result;
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::finalize_423430(std::uint32_t rectangle)
{
    auto &b = binding();
    return b.surfaces.finalize_423430(rectangle); // actual unlocked/not-locked platform result
}
std::uint32_t SourceMenuPresentationBackend::set_render_state(std::uint32_t state, std::uint32_t value)
{
    try
    {
        return active_device().set_render_state(state, value);
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::set_texture(std::uint32_t stage, std::uint32_t token)
{
    try
    {
        return active_device().set_texture(stage, token);
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::set_texture_stage_state(std::uint32_t stage, std::uint32_t state,
                                                                     std::uint32_t value)
{
    try
    {
        return active_device().set_texture_stage_state(stage, state, value);
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::draw_indexed(std::uint32_t type, std::uint32_t fvf,
                                                          std::span<const retail::PolygonSubmissionVertex> vertices,
                                                          std::span<const std::uint16_t> indices, std::uint32_t flags,
                                                          const retail::ViewportState &viewport)
{
    try
    {
        const auto result = active_device().draw_indexed(type, fvf, vertices, indices, flags, viewport);
        host_presentation::note_draw(vertices.size(), result == 0); // NEW R19 (interp): tag <-> capture draw ordinal
        host_wide_menu::note_draw(vertices.size(), result == 0);    // NEW R20 (wide_menu)
        return result;
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint32_t SourceMenuPresentationBackend::draw(std::uint32_t type, std::uint32_t fvf,
                                                  std::span<const retail::PolygonSubmissionVertex> vertices,
                                                  std::uint32_t flags, const retail::ViewportState &viewport)
{
    try
    {
        const auto result = active_device().draw(type, fvf, vertices, flags, viewport);
        host_presentation::note_draw(vertices.size(), result == 0); // NEW R19 (interp)
        host_wide_menu::note_draw(vertices.size(), result == 0);    // NEW R20 (wide_menu)
        return result;
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
std::uint16_t SourceMenuPresentationBackend::destination_width(std::uint32_t slot) const
{
    const auto &b = binding();
    if (frame_)
        return frame_->destination_width(slot);
    MaterialTextureReplacement replacement(b.materials, frames_.source_menu_replacement_storage(b.materials));
    return replacement.destination_width(slot);
}
void SourceMenuPresentationBackend::upload_to_destination(std::uint32_t slot, const Image &image)
{
    auto &b = binding();
    try
    {
        if (frame_)
        {
            frame_->upload_to_destination(slot, image);
            return;
        }
        MaterialTextureReplacement replacement(b.materials, frames_.source_menu_replacement_storage(b.materials));
        replacement.upload_to_destination(slot, image);
    }
    catch (...)
    {
        discard_failed_frame();
        throw;
    }
}
} // namespace sgn
