#pragma once
#include "sgn/source_boot_config.hpp"
#include "sgn/session_outer_source_state.hpp"
#include <cstdint>
#include <optional>

namespace sgn::retail
{

// Process/window lifetime, separate from per-level resources. These words
// occupy virtual zero-filled storage in the pinned retail image.
struct SourceApplicationWindowClock
{
    SourceApplicationWindowClock() = default;
    SourceApplicationWindowClock(const SourceApplicationWindowClock &) = delete;
    SourceApplicationWindowClock &operator=(const SourceApplicationWindowClock &) = delete;
    std::uint32_t word_61c370{};
    std::uint32_t word_61c37c{};
    std::uint32_t word_61c388{};
    std::uint32_t word_61c390{};
    std::uint32_t word_61c394{};
    std::uint32_t word_61c35c{};
    float word_61c36c{};
    std::uint32_t word_61c39c{};
};

// Invocation-only references. Bind scene/target to RaceSessionState::scene_mode
// and options.selection_phase; boot/outer to the same platform's world storage.
// The two window words must come from the native window's retained source owner.
struct SourceApplicationEntryWorld
{
    SourceBootConfigState &boot;
    SessionOuterSourceState &outer;
    std::uint32_t &scene;        // 612BE8
    std::uint32_t &target_scene; // 612BEC
    const std::uint32_t &window_word_61c374;
    const std::uint32_t &window_word_61c364;
};

struct SourceApplicationEntryArguments
{
    std::uintptr_t instance;          // original ECX at call 44EAA4
    std::uintptr_t previous_instance; // original EDX
    std::int32_t show_command;        // first stack argument of 44E890
    const char *command_line;         // second stack argument of 44E890
};

enum class SourceApplicationCall : std::uint32_t
{
    initialize_42d660 = 0x42d660,
    initialize_442010 = 0x442010,
    initialize_scene_442120 = 0x442120,
    initialize_window_clock_44e5d0 = 0x44e5d0,
    idle_44e9d0 = 0x44e9d0,
    update_race_442810 = 0x442810,
    draw_race_4427b0 = 0x4427b0,
    update_menu_441f00 = 0x441f00,
    draw_menu_441d70 = 0x441d70,
    finish_frame_44e6b0 = 0x44e6b0,
    sample_end_time_44e6c0 = 0x44e6c0,
    shutdown_442180 = 0x442180,
};

// Mandatory synchronous child bindings: implementations must execute the named
// source operation, or report that it is unavailable. No default no-op handlers.
struct SourceApplicationEntryServices
{
    virtual ~SourceApplicationEntryServices() = default;
    virtual void show_cursor(bool visible) = 0;
    virtual std::uint32_t initialize_window_44e890(const SourceApplicationEntryArguments &) = 0;
    virtual void invoke(SourceApplicationCall) = 0;
};

// Native adapter owns the actual MSG, retaining its full pointer-width payload.
// peek_removed is PeekMessageA(MSG*, nullptr, 0, 0, PM_REMOVE), not a drain loop.
// Accessors/translate/dispatch are valid only after peek_removed returns true.
struct SourceApplicationMessages
{
    virtual ~SourceApplicationMessages() = default;
    virtual bool peek_removed() = 0;
    virtual std::uint32_t message_id() const = 0;
    virtual std::uint32_t source_wparam() const = 0;
    virtual void translate() = 0;
    virtual void dispatch() = 0;
};

struct SourceApplicationEntryResult
{
    // Empty means the original returned an uninitialized MSG.wParam: no
    // successful PeekMessage preceded an exit through the source exit word.
    // It does not mean a guessed exit code of zero.
    std::optional<std::uint32_t> source_return_word;
};

SourceApplicationEntryResult run_source_application_44ea60(SourceApplicationEntryWorld &,
                                                           const SourceApplicationEntryArguments &,
                                                           SourceApplicationEntryServices &,
                                                           SourceApplicationMessages &);

} // namespace sgn::retail
