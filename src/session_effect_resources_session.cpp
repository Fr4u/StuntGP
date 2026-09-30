#include "sgn/session_effect_resources.hpp"
#include "sgn/race_session.hpp"
#include "sgn/session_world_frame.hpp"

namespace sgn::retail
{
void publish_session_effect_resources(RaceSession &session, SessionWorldFrameResources &resources)
{
    session.shared_platform()->require_owner_thread();
    auto &state = session.state();
    state.effect_resources.publish(state.sprite_packets, resources);
}
std::span<const contact::effects::EffectAnimationFrames> borrow_session_effect_animation_frames(RaceSession &session)
{
    session.shared_platform()->require_owner_thread();
    return session.state().effect_resources.animations.frames();
}
} // namespace sgn::retail
