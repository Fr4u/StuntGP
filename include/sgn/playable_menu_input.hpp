#pragma once
#include "sgn/menu_keyboard_acquisition.hpp"
#include <span>

namespace sgn::retail
{
class RaceSession;

// New preview-host binding. These are existing source owners; no input poll,
// device startup, vehicle input, simulation, menu dispatch or drawing occurs.
struct PlayableMenuKeyboardWorld
{
    const std::uint32_t &scene;
    MenuKeyboardRawState &raw;
    SessionMenuInputState &menu;
    RaceInitializationState &keyboard;
    MenuDeviceSelectorsView selectors;
};
// Explicit single-keyboard preview admission. Does not represent4249B0.
// Reject active other devices before any write. Retain edges and payloads.
void bind_playable_menu_keyboard(PlayableMenuKeyboardWorld);
// Once per accepted host menu/capture step, only while paused. Maps the
// existing host VK snapshot to high-bit source bytes, then calls full40D860.
void publish_playable_menu_keyboard(PlayableMenuKeyboardWorld, std::span<const bool, 256>);

class PlayablePauseKey final
{
    bool escape_down_{};

  public:
    [[nodiscard]] bool consume(std::uint32_t scene, std::uint32_t frame_pause,
                               std::span<const bool, 256> keys) noexcept;
    // Focus loss: first observed key-up rearms Escape. No source state write.
    void release_host_input() noexcept
    {
        escape_down_ = true;
    }
};

class PlayableMenuInput final
{
    RaceSession *session_{};
    PlayablePauseKey pause_key_;

  public:
    PlayableMenuInput() = default;
    PlayableMenuInput(const PlayableMenuInput &) = delete;
    PlayableMenuInput &operator=(const PlayableMenuInput &) = delete;
    PlayableMenuInput(PlayableMenuInput &&) = delete;
    PlayableMenuInput &operator=(PlayableMenuInput &&) = delete;
    // After PreviewSession startup, once, before publishing input. Physical
    // slot0 borrows the same first enabled player as PlayableDriver.
    void bind_keyboard(RaceSession &);
    // Before choosing driver.tick versus input-free advance_session_iteration.
    // Escape entry uses actual40C750 + actual Session audio lifecycle. Paused
    // keys use40D860.447100 consumes them later in the ONE source capture.
    void publish(RaceSession &, std::span<const bool, 256> keys);
    void release_host_input() noexcept
    {
        pause_key_.release_host_input();
    }
};
} // namespace sgn::retail
