#pragma once
#include "sgn/race_session.hpp"
#include "sgn/retail_time_trial_lifecycle.hpp"
#include "sgn/retail_challenge_lifecycle.hpp"

namespace sgn::retail
{
// The renderer/resource registry supplies the actual430D40 operation. This is
// not a notification sink: it must honor the original source/destination width
// guard, replace the bound image, and update the one61B634 owner on that path.
struct RaceTextureReplacement
{
    virtual ~RaceTextureReplacement() = default;
    virtual std::uint32_t selected_overlay_texture() const = 0;
    virtual void replace_source_texture(std::uint32_t destination_slot, std::uint32_t source_texture) = 0;
};

// Concrete recovered sound calls plus the mandatory actual texture registry.
// AudioDriver and texture identities are borrowed for the duration of a call.
class SessionRacePresentation final : public RaceStartPresentation
{
    AudioDriver &audio_;
    RaceTextureReplacement &textures_;

  public:
    SessionRacePresentation(AudioDriver &audio, RaceTextureReplacement &textures) : audio_(audio), textures_(textures)
    {
    }
    void sound(std::uint32_t id, std::uint32_t volume) override;
    std::uint32_t selected_texture() const override;
    void texture(std::uint32_t slot, std::uint32_t id) override;
};

// One443A80 invocation; never advances a clock, physics, camera or presentation
// independently. The enclosing442810 composer must call the real418A90 between
// the first three invocations. No callback defaults or hidden countdown loop.
// Complete441E20 and445530 in the game_type3 domain. Publishes source pending
// scene transition; executing441DC0 and the menu resource chain is separate.
void request_challenge_scene_transition(RaceSessionState &);
void start_challenge_session_camera(RaceSessionState &); // full4028D0(7)/403310/shared44EE10
bool tick_session_lifecycle(RaceSessionState &, const VehicleResetParameters &, RaceStartPresentation &,
                            NormalRacePresentation * = nullptr, ChallengeClockAudio * = nullptr,
                            ChallengeRaceServices * = nullptr);
bool tick_session_lifecycle(RaceSession &, RaceTextureReplacement &);
bool tick_session_lifecycle(RaceSession &, RaceTextureReplacement &, AudioDriver &);
} // namespace sgn::retail
