#pragma once
#include "sgn/contact_particle_allocator.hpp"
#include "sgn/contact_vehicle_particle_state.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/retail_world_globals.hpp"

namespace sgn::retail::contact::effects
{
// Sole persistent source staging4D5BC0..4D5C8F and initialized4D5C94.
// Its retained fields are defined global data, unlike unwritten stack slots.
struct EffectStaging
{
    EffectParticle particle;
    std::uint32_t initialized{};
};
struct EngineParticlePart
{
    std::uint16_t flags_02;
    const ModelTransform &transform;
};
struct EngineParticleModel
{
    virtual ~EngineParticleModel() = default;
    virtual std::uint32_t part_count() const = 0;
    virtual unsigned selected_lod() const = 0;
    virtual EngineParticlePart part(unsigned lod, unsigned index) = 0;
};
class EngineParticleModelView final : public EngineParticleModel
{
    ModelResource &model_;
    unsigned lod_;

  public:
    EngineParticleModelView(ModelResource &model, unsigned lod) : model_(model), lod_(lod) {}
    std::uint32_t part_count() const override
    {
        return static_cast<std::uint32_t>(model_.part_count(model_.source_lod_index(lod_)));
    }
    unsigned selected_lod() const override
    {
        return lod_;
    }
    EngineParticlePart part(unsigned lod, unsigned index) override
    {
        const auto resident = model_.source_lod_index(lod);
        return {model_.source_part_flags_02(resident, index), model_.source_part(resident, index).transform};
    }
};
void initialize_engine_particle_staging(EffectStaging &, const WorldGravity &); // 4169E0
std::array<std::optional<unsigned>, 2> emit_engine_particles(const VehicleParameters &, const VehicleState &,
                                                             VehicleParticleState &, const WorldGravity &,
                                                             EngineParticleModel &, EffectStaging &, VehicleRandom &,
                                                             EffectParticleBank &, const ModelTransform &template_pose,
                                                             EffectParticleFrameAudit *audit = nullptr); // 416A80
// Canonical owner overload. Flags+02 gates position access; no public payload ref/span.
// Actual tick dispatch remains a separate integration step.
std::array<std::optional<unsigned>, 2> emit_engine_particles(const VehicleParameters &, const VehicleState &,
                                                             VehicleParticleState &, const WorldGravity &,
                                                             SourcePmdModelView, EffectStaging &, VehicleRandom &,
                                                             EffectParticleBank &, const ModelTransform &template_pose,
                                                             EffectParticleFrameAudit *audit = nullptr);
// Registry entry for source416A80. Current LOD belongs to the selected model.
// Legacy reference access remains guarded in the implementation; no model copy.
std::array<std::optional<unsigned>, 2> emit_engine_particles(const VehicleParameters &, const VehicleState &,
                                                             VehicleParticleState &, const WorldGravity &,
                                                             ModelResources &, std::size_t handle, EffectStaging &,
                                                             VehicleRandom &, EffectParticleBank &,
                                                             const ModelTransform &template_pose,
                                                             EffectParticleFrameAudit *audit = nullptr);
} // namespace sgn::retail::contact::effects
