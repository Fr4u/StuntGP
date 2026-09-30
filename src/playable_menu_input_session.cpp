#include "sgn/playable_menu_input.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_pause.hpp"
#include "sgn/retail_pause_toggle.hpp"

namespace sgn::retail
{
namespace
{
PlayableMenuKeyboardWorld keyboard_world(RaceSessionState &s)
{
    return {s.scene_mode, s.source_input.acquisition, s.menu_input, s.startup,
            source_menu_selectors(s.source_input.profiles)};
}
} // namespace
void PlayableMenuInput::bind_keyboard(RaceSession &session)
{
    session.shared_platform()->require_owner_thread();
    if (session_ != nullptr)
        throw FormatError("playable menu keyboard already bound");
    auto &s = session.state();
    if (s.scene_mode != 1 || s.hud_block != 0)
        throw FormatError("playable menu host requires a local race");
    unsigned player = 6;
    for (const auto &controller : s.controls.controllers)
        if (controller.active == 1)
        {
            player = controller.vehicle;
            break;
        }
    if (player >= 6)
        throw FormatError("playable menu controller has no valid active vehicle");
    bind_playable_menu_keyboard(keyboard_world(s));
    bind_frontend_race_input(s.frontend_launch, 1, player, 1);
    session_ = &session;
}
void PlayableMenuInput::publish(RaceSession &session, std::span<const bool, 256> keys)
{
    session.shared_platform()->require_owner_thread();
    if (session_ != &session)
        throw FormatError("playable menu keyboard belongs to another session");
    auto &s = session.state();
    if (pause_key_.consume(s.scene_mode, s.frame_pause, keys))
    {
        SessionPauseInvocation pause(s, session.audio_device());
        toggle_pause_40c750({s.scene_mode, pause.lifecycle()}, {1});
    }
    if (s.frame_pause == 1)
        publish_playable_menu_keyboard(keyboard_world(s), keys);
}
} // namespace sgn::retail
