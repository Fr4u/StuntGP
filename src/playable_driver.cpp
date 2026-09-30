#include "sgn/playable_driver.hpp"
#include "sgn/race_session_iteration.hpp"

namespace sgn::retail
{
namespace
{
unsigned player(const RaceSessionState &s)
{
    // 40DAD0 iterates enabled input devices; it does not use4722D0, the last
    // camera-loop cursor.40D500 legitimately leaves that cursor at inactive3.
    // This new single-player keyboard binding selects the enabled controller.
    for (const auto &control : s.controls.controllers)
        if (control.active == 1)
        {
            if (control.vehicle >= 6)
                throw FormatError("playable controller vehicle identity");
            return control.vehicle;
        }
    throw FormatError("playable controller has no active vehicle");
}
void previous_input(VehicleState &physical, VehicleRestartState &restart)
{
    const auto &input = physical.input;
    restart.previous_input = {input.throttle,  input.brake,          input.left,  input.right,
                              input.rear_lock, input.steering_boost, input.turbo, input.auxiliary};
    physical.previous_shift = input.shift;
    physical.previous_stunt = input.stunt;
}
} // namespace
void apply_playable_input(RaceSessionState &s, const PlayableInput &keys)
{
    const auto slot = player(s);
    s.current_slot = slot;
    // 40DB66..40DB9D.2280 is the source input-device publication guard.
    if (s.vehicles.startup[slot].field_2280 == 0)
        return;
    auto &physical = s.vehicles.physics[slot];
    auto &race = s.vehicles.race[slot];
    previous_input(physical, s.vehicles.restart[slot]);
    physical.input = {};
    // 40DBC1..40DBD7 keyboard selection; authoredkeycodes are replaced by
    // the host's explicit action bindings, not borrowed from an old project.
    race.control_kind = 2;
    s.vehicles.parameters[slot].steering_sensitivity = 1;
    const auto byte = [](bool down) { return static_cast<std::uint8_t>(down ? 255 : 0); };
    auto &input = physical.input;
    input.throttle = byte(keys.throttle);
    input.brake = byte(keys.brake && !keys.throttle);
    input.right = byte(keys.right);
    input.left = byte(keys.left && !keys.right);
    input.shift = byte(keys.shift);
    input.stunt = byte(keys.stunt);
    input.turbo = byte(keys.turbo);
}
PlayableDriver::PlayableDriver(RaceSession &session, RaceTextureReplacement &textures,
                               std::span<const contact::effects::EffectAnimationFrames> animations)
    : session_(session), textures_(textures), animations_(animations)
{
}
void PlayableDriver::tick(const PlayableInput &input, SessionWorldTickAudit *audit)
{
    auto &s = session_.state();
    if (s.frame_pause != 0 || s.hud_block != 0)
        throw FormatError("playable driver requires unpaused local race, not source menu/demo mode");
    apply_playable_input(s, input);
    const auto slot = player(s);
    // NewRkey command requests the existing763C recovery path.418830 invokes
    // the real418320/412940 placement; no pose or velocity is authored here.
    if (input.reset && !reset_down_ && s.clock.phase == 2 && s.vehicles.race[slot].control_enabled != 0)
        s.vehicles.race[slot].respawn_requested = 1;
    reset_down_ = input.reset;
    advance_session_iteration(session_, textures_, animations_, audit);
}
void PlayableDriver::release_input() noexcept
{
    auto &s = session_.state();
    for (const auto &controller : s.controls.controllers)
        if (controller.active == 1 && controller.vehicle < 6)
        {
            auto &p = s.vehicles.physics[controller.vehicle];
            p.input = {};
            p.previous_shift = 0;
            p.previous_stunt = 0;
            s.vehicles.restart[controller.vehicle].previous_input = {};
        }
    reset_down_ = false;
}
} // namespace sgn::retail
