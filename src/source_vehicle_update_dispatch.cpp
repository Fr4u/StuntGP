#include "sgn/source_vehicle_update_dispatch.hpp"
#include "sgn/source_vehicle_strip_producer.hpp"
#include "sgn/source_world_strip.hpp"
#include "sgn/race_session.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_presentation.hpp"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>

extern "C" std::uint32_t sgn_source_vehicle_ambience_418150(const void *) noexcept;
extern "C" void sgn_source_vehicle_pose_4181f5(void *, const void *const *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using H = std::uint16_t;
using B = std::uint8_t;
static_assert(sizeof(void *) == 8 && sizeof(VehicleVector) == 16 && sizeof(Vec3) == 12);
static_assert(sizeof(ModelTransform) == 80 && offsetof(ModelTransform, forward) == 32 &&
              offsetof(ModelTransform, up) == 48);
void admit(RaceSessionPlatform &p, W site)
{
    p.require_owner_thread();
    if (!p.source_construction_only_entry())
        throw SourceAllocationError(site, "vehicle source dispatcher requires actual source platform");
}
unsigned reached_slot(unsigned slot, W site)
{
    if (slot >= VehicleStorage::physical_slots)
        throw SourceAllocationError(site, "reached vehicle pointer outside six actual owners");
    return slot;
}
unsigned current(RaceSessionState &w, W site)
{
    return reached_slot(w.current_slot, site);
}
W add(W base, W offset, W site)
{
    if (offset > std::numeric_limits<W>::max() - base)
        throw SourceAllocationError(site, "reached source PMD offset leaves DWORD domain");
    return base + offset;
}
SourcePmdModelView model(RaceSessionPlatform &p, std::optional<std::size_t> handle, W site)
{
    if (!handle)
        throw SourceAllocationError(site, "reached null vehicle model pointer");
    return p.source_scene_resources().models().source_at(*handle);
}
template <class T> T read(SourcePmdModelView m, W base, W offset, W site)
{
    if (base == 0)
        throw SourceAllocationError(site, "reached null PMD pointer");
    return m.bytes().field<T>(add(base, offset, site)).value();
}
template <class T> void write(SourcePmdModelView m, W base, W offset, T value, W site)
{
    if (base == 0)
        throw SourceAllocationError(site, "reached null PMD pointer");
    m.bytes().field<T>(add(base, offset, site)).set(value);
}
void required(SourceVehicleDispatchRequired &r, RaceSessionPlatform &p, W site, W child,
              std::optional<unsigned> captured = std::nullopt)
{
    r.invoke(p, site, child, captured);
}
void ambience_418150(RaceSessionPlatform &p)
{
    auto &w = p.state();
    const auto slot = current(w, 0x418157);
    const W value = sgn_source_vehicle_ambience_418150(&w.vehicles.model_effects[slot].route_envelope);
    const auto handle = w.vehicles.models[slot].model; // 418168, after full conversion
    model(p, handle, 0x41816f).ambience().set(static_cast<H>(value));
}
void regular_model_42b850(RaceSessionPlatform &p, std::optional<std::size_t> handle, ModelTransform &pose, W flags)
{
    auto &w = p.state();
    allocate_source_outer_model_matrices_44fdb0(pose, w.view_matrices); // 42B85C before model access
    const auto m = model(p, handle, 0x42b863);
    allocate_source_model_part_matrices_450150(m, w.view_matrices);
    auto &q = w.model_draws.regular;
    const auto cursor = q.cursor; // 42B868
    pose.tail[0] = 0;             // 42B875, before reached destination validation
    if (!cursor || *cursor >= q.records.size())
        throw SourceAllocationError(0x42b87f, "reached regular queue command outside owner");
    q.records[*cursor].model_identity = p.source_scene_resources().models().identity(*handle);
    static_assert(sizeof(ModelTransform) == 80);
    std::memcpy(&q.records[*cursor].pose, &pose, sizeof(pose));
    const auto d = m.descriptor();
    if (flags != 0)
    {
        const W selector = d.field<W>(0x5c).value(); // 42B7B0
        const auto model_flags = d.field<B>(2);
        model_flags.set(static_cast<B>(model_flags.value() | 8u));
        if (selector >= 4)
            throw SourceAllocationError(0x42b7ba, "reached PMD flag selector outside four slots");
        W part = d.field<W>(0x48 + selector * 4).value();
        W count = d.field<W>(0x24).value();
        const H mask = static_cast<H>((flags << 8u) | 0x80u);
        while (count != 0)
        {
            if (read<H>(m, part, 0, 0x42b7c8) == 1)
            {
                const H old = read<H>(m, part, 2, 0x42b7ce);
                write<H>(m, part, 2, static_cast<H>(old | mask), 0x42b7ce);
            }
            part += 0x40u;
            --count; // unsigned source ADD; validate the next dereference only
        }
    }
    else
    {
        const auto mf = d.field<H>(2);
        mf.set(static_cast<H>(mf.value() & 0xfff7u));
    }
    // 42B895 reread,42B89F publication,42B8A5 reached terminator. No preflight
    // for the following record before the command/flags/cursor partial writes.
    if (!q.cursor)
        throw SourceAllocationError(0x42b89f, "reached null regular queue advance");
    q.cursor = *q.cursor + 1u;
    if (*q.cursor >= q.records.size())
        throw SourceAllocationError(0x42b8a5, "reached regular queue terminator outside owner");
    q.records[*q.cursor].model_identity = 0;
}
std::string boundary_text(W site, W child)
{
    std::ostringstream out;
    out << "source vehicle child not bound at 0x" << std::hex << site << " -> 0x" << child;
    return out.str();
}
} // namespace
SourceVehicleDispatchBoundary::SourceVehicleDispatchBoundary(W site, W target)
    : FormatError(boundary_text(site, target)), callsite(site), child(target)
{
}
void SourceVehicleDispatchUnavailable::invoke(RaceSessionPlatform &, W site, W child, std::optional<unsigned>)
{
    throw SourceVehicleDispatchBoundary(site, child);
}
void SourceVehicleDispatchUnavailable::leader_notice(RaceSessionPlatform &, unsigned, std::string_view)
{
    throw SourceVehicleDispatchBoundary(0x41e3de, 0x442cf0);
}

void append_source_vehicle_draw_418180(RaceSessionPlatform &p, SourceVehicleDispatchRequired &r)
{
    admit(p, 0x418180);
    auto &w = p.state();
    w.current_slot = 0;
    {
        SGN_PROFILE_SCOPE("entities.v.strip_41d1c0");
        (void)append_source_vehicle_strip_draw_41d1c0(p);
    } // 418193 before any vehicle child
    for (unsigned remaining = 6; remaining != 0; --remaining)
    {
        if (w.vehicles.race[current(w, 0x4181a6)].active == 1)
        {
            // NEW host diagnostics only: scopes around the unchanged source call order.
            {
                SGN_PROFILE_SCOPE("entities.v.detail_413e10");
                required(r, p, 0x4181b3, 0x413e10);
            }
            {
                SGN_PROFILE_SCOPE("entities.v.parts_414350");
                required(r, p, 0x4181be, 0x414350, w.current_slot);
            }
            {
                SGN_PROFILE_SCOPE("entities.v.shadow_41c420");
                required(r, p, 0x4181c3, 0x41c420);
            }
            {
                SGN_PROFILE_SCOPE("entities.v.rings_414cf0_4153a0");
                required(r, p, 0x4181c8, 0x414cf0);
                required(r, p, 0x4181cd, 0x414e50);
                required(r, p, 0x4181d2, 0x415040);
                required(r, p, 0x4181d7, 0x4153a0);
            }
            {
                SGN_PROFILE_SCOPE("entities.v.glows_4154c0");
                required(r, p, 0x4181dc, 0x4154c0);
            }
            SGN_PROFILE_SCOPE("entities.v.pose_model_42b850");
            const auto captured = w.current_slot;
            ModelTransform pose;
            // clang-format off: keep the trailing comment on one line (ReflowComments would split it)
            std::memcpy(&pose, &w.model_draws.template_pose, sizeof(pose)); // 4181F3 explicitly copies this80-byte local
            // clang-format on
            const auto slot = reached_slot(captured, 0x418206);
            const auto &v = w.vehicles.physics[slot];
            const std::array<const void *, 5> inputs{&v.position, &v.up, &v.basis_z, &v.basis_y_w,
                                                     &w.vehicles.contact[slot].rig.origin_y_offset};
            sgn_source_vehicle_pose_4181f5(&pose, inputs.data());
            ambience_418150(p);
            const auto after = current(w, 0x4182a7);
            const W hold = w.vehicles.race[after].collision_hold;
            w.vehicles.models[after].field_2708 = (hold == 0 || (hold < 60u && (hold & 7u) < 3u)) ? 0u : 16u;
            const W flags = w.vehicles.models[after].field_2708;
            const auto handle = w.vehicles.models[after].model;
            regular_model_42b850(p, handle, pose, flags);
        }
        host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
        w.current_slot = w.current_slot + 1u;
    }
    w.current_slot = 0;
}
} // namespace sgn::retail
