#pragma once
#include "sgn/race_session_world_tick.hpp"

namespace sgn::retail
{
// New platform bindings. These are held keys, not physical forces or poses.
struct PlayableInput
{
    bool throttle{}, brake{}, left{}, right{}, turbo{}, stunt{}, shift{}, reset{};
};
// Keyboard branch40DAD0/40C8F0: copy the ten previous input bytes, clear,
// then publish digital255 inputs with the original opposing-key priority.
void apply_playable_input(RaceSessionState &, const PlayableInput &);

class PlayableDriver
{
    RaceSession &session_;
    RaceTextureReplacement &textures_;
    std::span<const contact::effects::EffectAnimationFrames> animations_;
    bool reset_down_{}; // new platform key edge, not reconstructed game state
  public:
    PlayableDriver(RaceSession &, RaceTextureReplacement &, std::span<const contact::effects::EffectAnimationFrames>);
    // Exactly one fixed simulation step. Caller handles the display-independent
    // 60Hz scheduler and camera_begin_outer_frame before this call. No dt.
    // Current scope: unpaused local TimeTrial, no source demo/menu dispatcher.
    void tick(const PlayableInput &, SessionWorldTickAudit *audit = nullptr);
    void release_input() noexcept;
};
} // namespace sgn::retail
