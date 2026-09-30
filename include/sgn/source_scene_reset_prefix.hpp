#pragma once
#include <cstdint>

namespace sgn::retail
{
class RaceSessionPlatform;
namespace contact::effects
{
struct EffectParticleBank;
}
// Existing full leaf; shared by445FFE and42E7A0, no duplicate effect bank.
void reset_scene_effects_45e0e0(contact::effects::EffectParticleBank &) noexcept;
enum class SourceSceneResetStatus
{
    overlay_owner_required,
    scalar_owner_required,
    callee_required,
    completed
};
struct SourceSceneResetResult
{
    SourceSceneResetStatus status;
    std::uint32_t blocked_address{};
    std::uint32_t missing_source_word{};
};
// 445FA0 reached prefix only. Every admitted scalar belongs to the retained
//  platform world, including61B634. No overlay resource/device is required.
//  The earlier source stores retain their order before44600F.
SourceSceneResetResult reset_source_scene_445fa0_prefix(RaceSessionPlatform &);
} // namespace sgn::retail
