#pragma once
#include "sgn/contact_takeoff_particles.hpp"
#include "sgn/model_resources.hpp"
#include <algorithm>
#include <bit>

namespace sgn::retail::contact::effects
{
// The source REP MOVSD copies the current, mutable 4864E8 object. Matrix
// references and uninterpreted words are copied as bits, never synthesized.
inline void copy_effect_template(EffectParticle &particle, const ModelTransform &pose) noexcept
{
    const auto words = std::bit_cast<std::array<std::uint32_t, 20>>(pose);
    particle.position = std::bit_cast<Vector4>(std::array<std::uint32_t, 4>{words[0], words[1], words[2], words[3]});
    std::copy_n(words.begin() + 4, 16, particle.frame_payload.begin());
}
} // namespace sgn::retail::contact::effects
