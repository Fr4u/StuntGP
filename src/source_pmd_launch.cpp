#include "vehicle_launch_provider.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <limits>
namespace sgn::retail
{
namespace detail
{
// The one friend reuses existing immutable relocation admission at dereference.
// It adds no public method and never exposes a raw pointer or unchecked span.
struct SourcePmdLaunchAccess
{
    static SourcePmdByteView part(const SourcePmdByteView &file, std::uint32_t at)
    {
        return file.relocated_part(at);
    }
};
} // namespace detail
namespace
{
using sgn::format_check::require;
std::uint32_t add(std::uint32_t at, std::uint64_t n)
{
    require(n <= std::numeric_limits<std::uint32_t>::max() - at, "launch projected offset overflow");
    return at + static_cast<std::uint32_t>(n);
}
SourcePmdByteView section_record(SourcePmdByteView file, std::uint32_t pointer, std::uint32_t stride,
                                 std::uint32_t header)
{
    const auto start = add(0x148, file.field<std::uint32_t>(header).value());
    const auto section = file.subview(start, file.field<std::uint32_t>(header + 0x94).value());
    require(pointer >= start && (pointer - start) % stride == 0, "launch projected pointer section/alignment");
    return section.subview(pointer - start, stride); // one actual record, never the entire future run
}
void observe(VehicleLaunchAudit *audit, std::uint32_t address, std::uint32_t at, std::uint32_t value) noexcept
{
    if (audit && audit->after)
        audit->after(audit->context, address, at, value);
}
struct SourceLaunchProvider
{
    SourcePmdModelView model;
    VehicleLaunchAudit *audit;
    void checkpoint(std::uint32_t address, std::uint32_t value) const noexcept
    {
        observe(audit, address, 0, value);
    }
    SourcePmdByteView part(std::uint32_t head, unsigned i) const
    {
        return detail::SourcePmdLaunchAccess::part(model.bytes(), add(head, std::uint64_t(i) * 64));
    }
    SourcePmdByteView transform(SourcePmdByteView record) const
    {
        const auto pointer = record.field<std::uint32_t>(8).value();
        return section_record(model.bytes(), pointer, 80, 0x44);
    }
    VehicleTrailModel basis_inputs() const
    {
        const auto head = model.descriptor().field<std::uint32_t>(0x48).value();
        const auto t = transform(part(head, 0));
        const auto forward = t.field<VehicleVector>(0x20).value();
        const VehicleVector up{t.field<float>(0x30).value(), t.field<float>(0x34).value(), t.field<float>(0x38).value(),
                               0};
        return {forward, up, 0};
    }
    std::uint32_t selected_head() const
    {
        const auto descriptor = model.descriptor();
        const auto current = descriptor.field<std::uint32_t>(0x5c).value();
        require(current < 4, "launch current LOD outside four source slots");
        const auto head = descriptor.field<std::uint32_t>(0x48 + current * 4).value();
        observe(audit, 0x416666, descriptor.location().offset + 0x48 + current * 4, head);
        return head;
    }
    Vec3 point(std::uint32_t head, unsigned i) const
    {
        const auto p = transform(part(head, i)).field<VehicleVector>(0).value();
        return {p.x, p.y, p.z}; // source copies positionW, but resultW is unwritten
    }
    void animate(unsigned i) const
    {
        const auto head = model.descriptor().field<std::uint32_t>(0x48).value();
        const auto record = part(head, i);
        const auto pointer = record.field<std::uint32_t>(0x2c).value();
        observe(audit, 0x4167b5, record.location().offset + 0x2c, pointer);
        const auto count = record.field<std::uint16_t>(0x26).value();
        observe(audit, 0x4167bd, record.location().offset + 0x26, count);
        for (std::uint32_t n = 0; n < count; ++n)
        {
            const auto coordinate = section_record(model.bytes(), add(pointer, std::uint64_t(n) * 16), 16, 0x28);
            const auto y = coordinate.field<float>(4);
            const float advanced = vehicle_f32(double(y.value()) + std::bit_cast<float>(std::uint32_t{0x3ca3d70a}));
            y.set(advanced);
            observe(audit, 0x4167d3, coordinate.location().offset + 4, std::bit_cast<std::uint32_t>(advanced));
            if (advanced > 1)
            {
                const float value = vehicle_f32(double(advanced) - 1.0);
                y.set(value);
                observe(audit, 0x4167ee, coordinate.location().offset + 4, std::bit_cast<std::uint32_t>(value));
            }
        }
    }
};
struct RegistryLaunchProvider
{
    ModelResources &models;
    std::size_t handle;
    VehicleLaunchAudit *audit;
    void checkpoint(std::uint32_t address, std::uint32_t value) const noexcept
    {
        observe(audit, address, 0, value);
    }
    VehicleTrailModel basis_inputs() const
    {
        if (models.source_backed(handle))
            return SourceLaunchProvider{models.source_at(handle), audit}.basis_inputs();
        auto &m = models.at(handle);
        const auto &t = m.source_part(m.source_lod_index(0), 0).transform;
        return {
            {t.forward.x, t.forward.y, t.forward.z, std::bit_cast<float>(t.forward_w)}, {t.up.x, t.up.y, t.up.z, 0}, 0};
    }
    std::uint32_t selected_head() const
    {
        if (models.source_backed(handle))
            return SourceLaunchProvider{models.source_at(handle), audit}.selected_head();
        const auto lod = models.current_detail_level(handle);
        observe(audit, 0x416666, 0, lod);
        return lod;
    }
    Vec3 point(std::uint32_t head, unsigned i) const
    {
        if (models.source_backed(handle))
            return SourceLaunchProvider{models.source_at(handle), audit}.point(head, i);
        auto &m = models.at(handle);
        return m.source_part(m.source_lod_index(head), i).transform.position;
    }
    void animate(unsigned i) const
    {
        if (models.source_backed(handle))
        {
            SourceLaunchProvider{models.source_at(handle), audit}.animate(i);
            return;
        }
        auto &m = models.at(handle);
        for (auto &coordinate : m.source_coordinates(m.source_lod_index(0), i))
        {
            const float advanced = vehicle_f32(double(std::bit_cast<float>(coordinate[1])) +
                                               std::bit_cast<float>(std::uint32_t{0x3ca3d70a}));
            coordinate[1] = std::bit_cast<std::uint32_t>(advanced);
            observe(audit, 0x4167d3, 0, coordinate[1]);
            if (advanced > 1)
            {
                coordinate[1] = std::bit_cast<std::uint32_t>(vehicle_f32(double(advanced) - 1.0));
                observe(audit, 0x4167ee, 0, coordinate[1]);
            }
        }
    }
};
template <class Provider>
void run(VehicleLaunchBindings b, VehicleModelEffects &effects, const VehicleAnchorState &anchor, const Provider &p,
         contact::effects::ImpactParticles &particles, VehicleRandom &random)
{
    const detail::LaunchProvider provider{
        &p,
        [](const void *c) { return static_cast<const Provider *>(c)->basis_inputs(); },
        [](const void *c) { return static_cast<const Provider *>(c)->selected_head(); },
        [](const void *c, std::uint32_t h, unsigned i) { return static_cast<const Provider *>(c)->point(h, i); },
        [](const void *c, unsigned i) { static_cast<const Provider *>(c)->animate(i); },
        [](const void *c, std::uint32_t a, std::uint32_t v) noexcept {
            static_cast<const Provider *>(c)->checkpoint(a, v);
        }};
    detail::run_launch(b, effects, anchor, provider, particles, random);
}
} // namespace
void update_vehicle_launch_effect(VehicleLaunchBindings b, VehicleModelEffects &effects,
                                  const VehicleAnchorState &anchor, SourcePmdModelView model,
                                  contact::effects::ImpactParticles &particles, VehicleRandom &random)
{
    run(b, effects, anchor, SourceLaunchProvider{model, b.launch_audit}, particles, random);
}
void update_vehicle_launch_effect(VehicleLaunchBindings b, VehicleModelEffects &effects,
                                  const VehicleAnchorState &anchor, ModelResources &models, std::size_t handle,
                                  contact::effects::ImpactParticles &particles, VehicleRandom &random)
{
    run(b, effects, anchor, RegistryLaunchProvider{models, handle, b.launch_audit}, particles, random);
}
} // namespace sgn::retail
