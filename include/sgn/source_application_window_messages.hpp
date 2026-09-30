#pragma once
#include "sgn/source_application_surfaces.hpp"

namespace sgn::retail
{
// Invocation view. Bind these to the same slots used by entry, mode and menu.
struct SourceWindowMessageWords
{
    std::uint32_t &active_61c374;
    const std::uint32_t &mode_61c364;
    std::uint8_t &character_62ddfc;
};
struct SourceWindowMessage
{
    std::uintptr_t window;
    std::uint32_t id;
    std::uintptr_t wparam;
    std::intptr_t lparam;
};
struct SourceWindowMessagePlatform
{
    virtual ~SourceWindowMessagePlatform() = default;
    virtual void post_quit_zero() = 0;
    virtual bool is_iconic(std::uintptr_t window) = 0;
    virtual std::intptr_t default_window_proc(const SourceWindowMessage &) = 0;
};
std::intptr_t source_window_proc_44e6d0(const SourceWindowMessage &, SourceWindowMessageWords,
                                        SourceApplicationSurfaceServices &, SourceWindowMessagePlatform &);

// Native API calls only; registering WNDPROC and binding the retained world
// remain responsibilities of the window owner, not a global singleton here.
class Win32SourceWindowMessagePlatform final : public SourceWindowMessagePlatform
{
    std::uint32_t thread_;
    void require_thread() const;

  public:
    Win32SourceWindowMessagePlatform();
    void post_quit_zero() override;
    bool is_iconic(std::uintptr_t) override;
    std::intptr_t default_window_proc(const SourceWindowMessage &) override;
};
} // namespace sgn::retail
