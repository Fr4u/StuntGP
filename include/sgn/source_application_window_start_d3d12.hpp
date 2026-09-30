#pragma once
#include "sgn/source_application_entry.hpp"
#include "sgn/source_application_window_messages.hpp"
#include "sgn/window_input_owner.hpp"
#include <Windows.h>
#include <exception>

namespace sgn
{
class Renderer;
class PolygonDeviceContext;
class RendererLoadingSurfaces;
namespace retail
{
class FrontendState;
class SourceLateBoundApplicationSurfaces;
struct SourceApplicationSurfaceServices;
struct SourceColdInitializationContext;
// NEW HOST invocation view over the actual native owner. The HWND reference
// aliases App::window, which is published before delegated creation messages.
// The HINSTANCE is an immutable native identity, not a source readiness word.
struct NewHostNativeWindowBorrow
{
    HINSTANCE instance;
    HWND &window;
    InputWindowHandles current() const noexcept
    {
        return {reinterpret_cast<std::uintptr_t>(instance), reinterpret_cast<std::uintptr_t>(window)};
    }
};
// NEW HOST phase names, never source globals or COM/presentation success bits.
enum class NewHostD3D12StartupStage
{
    before_window_adoption,
    adopting_window,
    window_adopted,
    admitting_gpu,
    gpu_admitted,
    failed
};
// NEW HOST startup admission, NOT recovered44E890/DirectDraw equivalence.
// Window/input adoption is independent of GPU construction. Only the actual
// 42FD80 producer may admit its real renderer/presentation owners afterwards.
class NewHostD3D12WindowStartup final
{
    FrontendState &frontend_;
    NewHostNativeWindowBorrow native_;
    SourceWindowMessageWords words_;
    SourceLateBoundApplicationSurfaces &application_surfaces_;
    const std::exception_ptr &native_failure_;
    NewHostD3D12StartupStage stage_{NewHostD3D12StartupStage::before_window_adoption};
    void require_owner() const;
    void require_unchanged_handles(InputWindowHandles) const;

  public:
    NewHostD3D12WindowStartup(FrontendState &, NewHostNativeWindowBorrow, SourceWindowMessageWords,
                              SourceLateBoundApplicationSurfaces &, const std::exception_ptr &native_failure);
    NewHostD3D12WindowStartup(const NewHostD3D12WindowStartup &) = delete;
    NewHostD3D12WindowStartup &operator=(const NewHostD3D12WindowStartup &) = delete;
    NewHostD3D12StartupStage stage() const;
    void require_window_words(SourceWindowMessageWords) const;
    void require_entry_words(const std::uint32_t &active, const std::uint32_t &mode) const;
    void require_gpu_admitted() const;
    // Legal after window adoption, including before GPU production. The early
    // typed endpoint handles native mode/CPU effects without requiring Renderer.
    void idle();
    // NEW HOST44E890 branch: real HWND/input only, including existing44E870.
    // No GPU check or construction, no surface flags, no4249B0 before442161.
    void prepare(const SourceApplicationEntryArguments &, FrontendState &, SourceColdInitializationContext &,
                 SourceApplicationSurfaceServices &);
    // Reached ONLY after actual42FD80 constructs/publishes the same Renderer,
    // presentation.bind and the early lifecycle.attach_gpu have succeeded.
    // No source device/depth flags are inferred;430020/4313D5 still produce them.
    void admit_gpu(Renderer &, PolygonDeviceContext &, RendererLoadingSurfaces &);
};
} // namespace retail
} // namespace sgn
