#include "sgn/source_application_window_start_d3d12.hpp"
#include "sgn/source_application_window_d3d12_operations.hpp"
#include "sgn/source_application_surfaces_late_bound.hpp"
#include "sgn/source_application_window_start_win32.hpp"
#include "sgn/source_cold_initialize.hpp"
#include "sgn/race_session.hpp"
#include "sgn/renderer.hpp"
#include "sgn/renderer_loading_surfaces.hpp"

namespace sgn::retail
{
NewHostD3D12WindowStartup::NewHostD3D12WindowStartup(FrontendState &frontend, NewHostNativeWindowBorrow native,
                                                     SourceWindowMessageWords words,
                                                     SourceLateBoundApplicationSurfaces &application_surfaces,
                                                     const std::exception_ptr &native_failure)
    : frontend_(frontend), native_(native), words_(words), application_surfaces_(application_surfaces),
      native_failure_(native_failure)
{
}
void NewHostD3D12WindowStartup::require_owner() const
{
    frontend_.shared_platform()->require_owner_thread();
    if (native_failure_)
        std::rethrow_exception(native_failure_);
}
void NewHostD3D12WindowStartup::require_unchanged_handles(InputWindowHandles expected) const
{
    const auto current = native_.current();
    if (current.instance != expected.instance || current.window != expected.window)
        throw FormatError("NEW HOST native owner changed during a synchronous startup operation");
}
NewHostD3D12StartupStage NewHostD3D12WindowStartup::stage() const
{
    frontend_.shared_platform()->require_owner_thread();
    return stage_;
}
void NewHostD3D12WindowStartup::require_entry_words(const std::uint32_t &active, const std::uint32_t &mode) const
{
    require_owner();
    if (&active != &words_.active_61c374 || &mode != &words_.mode_61c364)
        throw FormatError("NEW HOST entry and callback require the same active/mode words");
}
void NewHostD3D12WindowStartup::require_window_words(SourceWindowMessageWords words) const
{
    require_entry_words(words.active_61c374, words.mode_61c364);
    if (&words.character_62ddfc != &words_.character_62ddfc)
        throw FormatError("NEW HOST startup must retain the staged navigation character byte");
}
void NewHostD3D12WindowStartup::require_gpu_admitted() const
{
    require_owner();
    if (stage_ != NewHostD3D12StartupStage::gpu_admitted)
        throw FormatError("NEW HOST draw requires GPU admission at the actual42FD80 producer");
}
void NewHostD3D12WindowStartup::idle()
{
    require_owner();
    if (stage_ != NewHostD3D12StartupStage::window_adopted && stage_ != NewHostD3D12StartupStage::gpu_admitted)
        throw FormatError("NEW HOST idle requires completed window adoption outside an active startup operation");
    try
    {
        // The same stable endpoint has a valid pre-GPU native/CPU stage. It
        // owns current renderer recovery after GPU admission; no stale ref here.
        auto &host = application_surfaces_.require_host();
        const auto handles = native_.current();
        host.require_window_binding(frontend_, handles, words_);
        host.idle();
        require_unchanged_handles(handles);
        require_owner();
    }
    catch (...)
    {
        stage_ = NewHostD3D12StartupStage::failed;
        throw;
    }
}
void NewHostD3D12WindowStartup::prepare(const SourceApplicationEntryArguments &arguments, FrontendState &frontend,
                                        SourceColdInitializationContext &cold,
                                        SourceApplicationSurfaceServices &application_surfaces)
{
    require_owner();
    if (stage_ != NewHostD3D12StartupStage::before_window_adoption)
        throw FormatError("NEW HOST window/input adoption cannot be repeated");
    stage_ = NewHostD3D12StartupStage::adopting_window;
    try
    {
        const auto platform = frontend_.shared_platform();
        if (&frontend != &frontend_ || &cold.frontend != &frontend_ || !platform->source_construction_only_entry() ||
            &frontend_.world() != &platform->state())
            throw FormatError("NEW HOST window adoption requires the same cold frontend/platform world");
        const auto handles = native_.current();
        if (!handles.instance || !handles.window || arguments.instance != handles.instance ||
            cold.input_handles.instance != handles.instance || cold.input_handles.window != handles.window)
            throw FormatError("NEW HOST startup must use the actual retained HWND/HINSTANCE and cold input handles");
        if (&application_surfaces != &application_surfaces_)
            throw FormatError("NEW HOST startup and callbacks must share the same application surface bridge");
        application_surfaces_.require_frontend(frontend_);
        auto &host = application_surfaces_.require_host();
        host.require_window_binding(frontend_, handles, words_); // explicitly no Renderer
        // Actual platform owner. Reuse the same pre-navigation attachment;
        // 4249B0 stays at442161 and no source device flags are published here.
        auto &input = platform->attach_window_input(handles.instance, handles.window);
        require_owner();
        require_unchanged_handles(handles);
        acquire_source_keyboard_44e870(frontend_.world().startup);
        input.require_binding(frontend_.world().source_input.acquisition, handles);
        require_owner();
        require_unchanged_handles(handles);
        stage_ = NewHostD3D12StartupStage::window_adopted;
    }
    catch (...)
    {
        stage_ = NewHostD3D12StartupStage::failed; // retain reached effects, no rollback
        throw;
    }
}
void NewHostD3D12WindowStartup::admit_gpu(Renderer &renderer, PolygonDeviceContext &context,
                                          RendererLoadingSurfaces &loading_surfaces)
{
    require_owner();
    if (stage_ != NewHostD3D12StartupStage::window_adopted)
        throw FormatError("NEW HOST42FD80 GPU admission requires prior window adoption and cannot repeat");
    stage_ = NewHostD3D12StartupStage::admitting_gpu;
    try
    {
        const auto handles = native_.current();
        const auto platform = frontend_.shared_platform();
        // Verifies the same real input/window lifetime retained at44E890.
        auto &input = platform->window_input(handles.instance, handles.window);
        if (reinterpret_cast<std::uintptr_t>(renderer.native_window()) != handles.window)
            throw FormatError("NEW HOST42FD80 renderer belongs to another HWND");
        auto &host = application_surfaces_.require_host();
        host.require_window_binding(frontend_, handles, words_);
        host.require_binding(frontend_, renderer, context, loading_surfaces, words_);
        loading_surfaces.require_binding(renderer, context);
        if (context.capturing() || context.snapshot().scene_open)
            throw FormatError("NEW HOST42FD80 admission requires an idle closed GPU submission context");
        renderer.check_health();
        input.require_binding(frontend_.world().source_input.acquisition, handles);
        require_owner();
        require_unchanged_handles(handles);
        // Host admission only: no global device, depth, source readiness flag,
        // image, clear, restore, gamma update or replay is produced here.
        stage_ = NewHostD3D12StartupStage::gpu_admitted;
    }
    catch (...)
    {
        stage_ = NewHostD3D12StartupStage::failed;
        throw;
    }
}
} // namespace sgn::retail
