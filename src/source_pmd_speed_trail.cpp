#include "sgn/model_resources.hpp"
#include "vehicle_speed_trail_provider.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
struct SourceSpeedProvider
{
    SourcePmdModelView model;
    float position_z() const
    {
        const auto part = model.part(model.source_lod_index(0), 23); // 414CB2..414CBB, raw0 only
        return model.bytes().field<float>(part.transform().location().offset + 8).value();
    }
    VehicleTrailModel basis_inputs() const
    {
        const auto part = model.part(model.source_lod_index(0), 0); // 413B6C..413B78, after part23Z
        const auto at = part.transform().location().offset;
        const auto bytes = model.bytes();
        const auto forward = bytes.field<VehicleVector>(at + 0x20).value();
        const VehicleVector up{bytes.field<float>(at + 0x30).value(), bytes.field<float>(at + 0x34).value(),
                               bytes.field<float>(at + 0x38).value(), 0};
        return {forward, up, 0}; // call-scoped basis inputs; upW and part23 are unused by413B60
    }
};
struct LegacySpeedProvider
{
    ModelResource &model;
    float position_z() const
    {
        return model.source_part(model.source_lod_index(0), 23).transform.position.z;
    }
    VehicleTrailModel basis_inputs() const
    {
        const auto &t = model.source_part(model.source_lod_index(0), 0).transform;
        return {
            {t.forward.x, t.forward.y, t.forward.z, std::bit_cast<float>(t.forward_w)}, {t.up.x, t.up.y, t.up.z, 0}, 0};
    }
};
struct RegistrySpeedProvider
{
    ModelResources &models;
    std::size_t handle;
    float position_z() const
    {
        if (models.source_backed(handle))
            return SourceSpeedProvider{models.source_at(handle)}.position_z();
        return LegacySpeedProvider{models.at(handle)}.position_z();
    }
    VehicleTrailModel basis_inputs() const
    {
        if (models.source_backed(handle))
            return SourceSpeedProvider{models.source_at(handle)}.basis_inputs();
        return LegacySpeedProvider{models.at(handle)}.basis_inputs();
    }
};
template <class Provider> void run(VehicleSpeedTrailBindings bindings, const Provider &provider)
{
    detail::update_speed_trail(
        bindings, {&provider, [](const void *context) { return static_cast<const Provider *>(context)->position_z(); },
                   [](const void *context) { return static_cast<const Provider *>(context)->basis_inputs(); }});
}
} // namespace
void update_vehicle_speed_trail(VehicleSpeedTrailBindings b, SourcePmdModelView model)
{
    run(b, SourceSpeedProvider{model});
}
void update_vehicle_speed_trail(VehicleSpeedTrailBindings b, ModelResources &models, std::size_t handle)
{
    run(b, RegistrySpeedProvider{models, handle});
}
} // namespace sgn::retail
