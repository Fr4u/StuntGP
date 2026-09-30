#include "sgn/model_resources.hpp"
#include "vehicle_pair_trail_provider.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
struct SourcePairProvider
{
    SourcePmdModelView model;
    VehicleTrailModel basis_inputs() const
    {
        const auto part = model.part(model.source_lod_index(0), 0); // 413B6C..413B78, each append
        const auto at = part.transform().location().offset;
        const auto bytes = model.bytes();
        const auto forward = bytes.field<VehicleVector>(at + 0x20).value();
        const VehicleVector up{bytes.field<float>(at + 0x30).value(), bytes.field<float>(at + 0x34).value(),
                               bytes.field<float>(at + 0x38).value(), 0};
        return {forward, up, 0}; // only call-scoped basis inputs; no upW or part23 read
    }
};
struct LegacyPairProvider
{
    ModelResource &model;
    VehicleTrailModel basis_inputs() const
    {
        const auto &t = model.source_part(model.source_lod_index(0), 0).transform;
        return {
            {t.forward.x, t.forward.y, t.forward.z, std::bit_cast<float>(t.forward_w)}, {t.up.x, t.up.y, t.up.z, 0}, 0};
    }
};
struct RegistryPairProvider
{
    ModelResources &models;
    std::size_t handle;
    VehicleTrailModel basis_inputs() const
    {
        if (models.source_backed(handle))
            return SourcePairProvider{models.source_at(handle)}.basis_inputs();
        return LegacyPairProvider{models.at(handle)}.basis_inputs();
    }
};
template <bool second, class Provider> void run(VehiclePairTrailBindings bindings, const Provider &provider)
{
    const detail::PairTrailProvider borrow{
        &provider, [](const void *context) { return static_cast<const Provider *>(context)->basis_inputs(); }};
    if constexpr (second)
        detail::update_pair_b(bindings, borrow);
    else
        detail::update_pair_a(bindings, borrow);
}
} // namespace
void update_vehicle_pair_a_trails(VehiclePairTrailBindings b, SourcePmdModelView model)
{
    run<false>(b, SourcePairProvider{model});
}
void update_vehicle_pair_b_trails(VehiclePairTrailBindings b, SourcePmdModelView model)
{
    run<true>(b, SourcePairProvider{model});
}
void update_vehicle_pair_a_trails(VehiclePairTrailBindings b, ModelResources &models, std::size_t handle)
{
    run<false>(b, RegistryPairProvider{models, handle});
}
void update_vehicle_pair_b_trails(VehiclePairTrailBindings b, ModelResources &models, std::size_t handle)
{
    run<true>(b, RegistryPairProvider{models, handle});
}
} // namespace sgn::retail
