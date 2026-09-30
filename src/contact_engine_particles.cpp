#include "sgn/contact_engine_particles.hpp"
#include "sgn/retail_effect_template.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail::contact::effects
{
namespace
{
std::int32_t integer(double value)
{
    if (!std::isfinite(value) || value < -2147483648.0 || value >= 2147483648.0)
        throw FormatError("engine particle conversion outside signed32 domain");
    return static_cast<std::int32_t>(value);
}
void position(EffectParticle &p, const VehicleState &car, const Vec3 &local)
{
    const double x = double(local.x) * car.basis_x.x;
    const float y = vehicle_f32(double(local.x) * car.basis_x.y), z = vehicle_f32(double(local.x) * car.basis_x.z);
    const double height = 70.0 - local.y;
    const Vec3 up{vehicle_f32(height * car.up.x), vehicle_f32(height * car.up.y), vehicle_f32(height * car.up.z)};
    const Vec3 forward{vehicle_f32(double(local.z) * car.basis_z.x), vehicle_f32(double(local.z) * car.basis_z.y),
                       vehicle_f32(double(local.z) * car.basis_z.z)};
    const float yz = vehicle_f32(double(up.y) + y), zz = vehicle_f32(double(up.z) + z);
    const float final_y = vehicle_f32(double(forward.y) + yz), final_z = vehicle_f32(double(forward.z) + zz);
    p.position.x = vehicle_f32(double(car.position.x) - ((x + up.x) + forward.x));
    p.position.y = vehicle_f32(double(car.position.y) - final_y);
    p.position.z = vehicle_f32(double(car.position.z) - final_z);
}
void velocity(EffectParticle &p, const VehicleState &car, VehicleRandom &random)
{
    p.velocity.x = vehicle_f32(double(car.velocity.x) * .5);
    p.velocity.y = vehicle_f32(double(car.velocity.y) * .5);
    p.velocity.z = vehicle_f32(double(car.velocity.z) * .5);
    const float x = vehicle_f32(random.signed_value(1));
    const double z = random.signed_value(1);
    p.velocity.x = vehicle_f32(double(p.velocity.x) + x);
    p.velocity.z = vehicle_f32(double(p.velocity.z) + z);
    p.angular_velocity = vehicle_f32(random.signed_value(22.5f));
}
} // namespace
void initialize_engine_particle_staging(EffectStaging &state, const WorldGravity &gravity)
{
    auto &p = state.particle;
    p.field_68 = std::bit_cast<float>(std::uint32_t{0x3c23d70a});
    p.flags = 7;
    p.lifetime_mode = 2;
    p.render_enabled = 1;
    p.animation_binding = 0;
    p.gravity_scale = vehicle_f32(double(gravity.inverse_factor) * std::bit_cast<float>(std::uint32_t{0xbe99999a}));
    p.base_scale = .5f;
    p.scale_fade_in = 3;
    p.scale_fade_out = 12;
    p.base_alpha = 96;
    p.alpha_fade_in = 0;
    p.alpha_fade_out = 18;
    p.base_color = 0x406090;
    p.texture = 7;
    p.render_flags = 0x60;
}
namespace
{
struct LegacyAnchor
{
    EngineParticlePart borrowed;
    std::uint16_t flags() const
    {
        return borrowed.flags_02;
    }
    Vec3 position() const
    {
        return borrowed.transform.position;
    }
};
struct LegacyAnchors
{
    EngineParticleModel &model;
    unsigned part_count() const
    {
        return model.part_count();
    }
    unsigned selected_lod() const
    {
        return model.selected_lod();
    }
    LegacyAnchor part(unsigned lod, unsigned index)
    {
        return {model.part(lod, index)};
    }
};
struct CheckedAnchor
{
    SourcePmdModelView model;
    SourcePmdPartView record;
    std::uint16_t flags() const
    {
        return record.flags_02().value();
    }
    Vec3 position() const
    {
        // Only reached after the source flags02 gate. No transform snapshot/ref.
        const auto bytes = model.bytes().subview(record.transform().location().offset, 12);
        return {bytes.field<float>(0).value(), bytes.field<float>(4).value(), bytes.field<float>(8).value()};
    }
};
struct CheckedAnchors
{
    SourcePmdModelView model;
    unsigned part_count() const
    {
        return model.part_count();
    }
    unsigned selected_lod() const
    {
        return model.current_detail_level().value();
    }
    CheckedAnchor part(unsigned lod, unsigned index)
    {
        return {model, model.part(model.source_lod_index(lod), index)};
    }
};
template <class Anchors>
std::array<std::optional<unsigned>, 2> emit_engine_particles_impl(
    const VehicleParameters &parameters, const VehicleState &car, VehicleParticleState &state,
    const WorldGravity &gravity, Anchors model, EffectStaging &global, VehicleRandom &random, EffectParticleBank &bank,
    const ModelTransform &template_pose, EffectParticleFrameAudit *audit)
{
    std::array<std::optional<unsigned>, 2> result;
    if (!((model.part_count() > 17 && car.turbo_rpm <= 1 && car.speed <= 30) || car.clutch < 1))
        return result;
    if (!std::isfinite(parameters.rpm_max) || parameters.rpm_max == 0)
        throw FormatError("engine particle RPM divisor outside finite nonzero domain");
    auto interval = integer((double(car.rpm) / parameters.rpm_max) * 64 + 7);
    if (car.clutch < .25f)
        interval = vehicle_random_integer(random, 4) + 4;
    ++state.field_7628;
    if (std::bit_cast<std::int32_t>(state.field_7628) < interval)
        return result;
    const float remaining = vehicle_f32((double(parameters.rpm_max) - car.rpm) / parameters.rpm_max);
    state.field_7628 = 0;
    auto &p = global.particle;
    copy_effect_template(p, template_pose);
    const auto lod = model.selected_lod();
    const auto first = model.part(lod, 17);
    if (first.flags() & 4)
    {
        position(p, car, first.position());
        if (global.initialized == 0)
        {
            initialize_engine_particle_staging(global, gravity);
            global.initialized = 1;
        }
        p.lifetime = std::bit_cast<std::uint32_t>(integer(double(remaining) * 30 + 36));
        velocity(p, car, random);
        result[0] = allocate_effect_particle(p, bank, {}, audit);
    }
    if (model.part_count() <= 18)
        return result;
    const auto second = model.part(lod, 18);
    if (!(second.flags() & 4))
        return result;
    position(p, car, second.position());
    velocity(p, car, random);
    result[1] = allocate_effect_particle(p, bank, {}, audit);
    return result;
}
// implementation-local access contracts
} // namespace
std::array<std::optional<unsigned>, 2> emit_engine_particles(
    const VehicleParameters &parameters, const VehicleState &car, VehicleParticleState &state,
    const WorldGravity &gravity, EngineParticleModel &model, EffectStaging &global, VehicleRandom &random,
    EffectParticleBank &bank, const ModelTransform &template_pose, EffectParticleFrameAudit *audit)
{
    return emit_engine_particles_impl(parameters, car, state, gravity, LegacyAnchors{model}, global, random, bank,
                                      template_pose, audit);
}
std::array<std::optional<unsigned>, 2> emit_engine_particles(
    const VehicleParameters &parameters, const VehicleState &car, VehicleParticleState &state,
    const WorldGravity &gravity, SourcePmdModelView model, EffectStaging &global, VehicleRandom &random,
    EffectParticleBank &bank, const ModelTransform &template_pose, EffectParticleFrameAudit *audit)
{
    return emit_engine_particles_impl(parameters, car, state, gravity, CheckedAnchors{model}, global, random, bank,
                                      template_pose, audit);
}
std::array<std::optional<unsigned>, 2> emit_engine_particles(const VehicleParameters &parameters,
                                                             const VehicleState &car, VehicleParticleState &state,
                                                             const WorldGravity &gravity, ModelResources &models,
                                                             std::size_t handle, EffectStaging &global,
                                                             VehicleRandom &random, EffectParticleBank &bank,
                                                             const ModelTransform &template_pose,
                                                             EffectParticleFrameAudit *audit)
{
    if (models.source_backed(handle))
    {
        return emit_engine_particles(parameters, car, state, gravity, models.source_at(handle), global, random, bank,
                                     template_pose, audit);
    }
    EngineParticleModelView legacy(models.at(handle), models.current_detail_level(handle));
    return emit_engine_particles(parameters, car, state, gravity, legacy, global, random, bank, template_pose, audit);
}
} // namespace sgn::retail::contact::effects
