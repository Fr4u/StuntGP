#pragma once
#include "sgn/track_contact.hpp"
#include <array>
#include <cstdint>

namespace sgn::retail
{
struct VehicleState;
struct VehicleRandom;
struct ModelTransform;
namespace contact::effects
{
// Source-defined fields have typed storage. Authored frame words whose meaning
// is not needed by this emitter remain opaque data, not a guessed matrix.
struct EffectParticle
{
    std::uint32_t active{}, flags{};               // 00/04
    Vector4 position{};                            // 08
    std::array<std::uint32_t, 16> frame_payload{}; // 18..54, source template
    Vector4 velocity{};                            // 58; its fourth word is not initialized by the emitter
    float field_68{}, gravity_scale{};
    std::uint32_t age{}, lifetime{}, lifetime_mode{}, render_enabled{}; // 70..7C
    std::uint32_t animation_binding{}, animation_frame{}, texture{},
        base_color{};                                                     // 80..8C; binding is never dereferenced here
    std::uint32_t secondary_color{}, field_94{}, render_flags{};          // 90/94/98
    float base_scale{}, scale_fade_in{}, scale_fade_out{}, scale{};       // 9C..A8
    float base_alpha{}, alpha_fade_in{}, alpha_fade_out{}, alpha{};       // AC..B8
    float angle{}, angular_velocity{}, color_fade_in{}, color_fade_out{}; // BC..C8
    std::uint32_t color{};                                                // CC, written by the later update stage
    // Exact source-staging holes: 64,90,94,C0,C4,C8,CC. Native retains their
    // prior opaque payload on allocation; this is not a claimed retail value.
    // Consumers must not treat such a value as defined before a source writer.
    std::array<bool, 7> source_undefined{};
};
// Shared 64-slot source pool at 0062A078, distinct from ImpactParticles32.
struct EffectParticleBank
{
    std::array<EffectParticle, 64> particles{};
    std::uint32_t active_signal{}; // 0062D478, set to one only on successful allocation
};

// Full 004171C0 contract: 32 allocation attempts and 224 shared RNG draws,
// including when the bank is full. This stage does not age or render particles.
void emit_takeoff_particles(const VehicleState &, VehicleRandom &, EffectParticleBank &,
                            const ModelTransform &template_pose);
} // namespace contact::effects
} // namespace sgn::retail
