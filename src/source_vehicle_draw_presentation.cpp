#include "sgn/source_vehicle_draw_presentation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "sgn/host_presentation.hpp"
#include "source_vehicle_draw_presentation_literals.inc"
#include <bit>
#include <cstring>
#include <limits>

extern "C"
{
    void sgn_vdp_normalize_44ed40(void *) noexcept;
    void sgn_vdp_steering_44f800(void *, std::uint32_t, const std::uint32_t *) noexcept;
    void sgn_vdp_multiply_44efd0(void *, const void *, const void *) noexcept;
    void sgn_vdp_cross_413b60(void *, const void *) noexcept;
    void sgn_vdp_segment_start(void *, const void *const *) noexcept;
    void sgn_vdp_segment_step(void *, const void *const *) noexcept;
    void sgn_vdp_segment_world(void *, const void *const *) noexcept;
    void sgn_vdp_anchor(void *, const void *const *) noexcept;
    void sgn_vdp_project_415190(void *, const void *const *) noexcept;
    std::uint32_t sgn_vdp_speed_alpha(const void *, void *) noexcept;
    std::uint32_t sgn_vdp_launch_alpha(const void *) noexcept;
    std::uint32_t sgn_vdp_distance_alpha(const void *, const void *) noexcept;
    std::uint32_t sgn_vdp_glow(void *, const void *const *) noexcept;
}
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using T = SourceTransportWord;
static_assert(sizeof(VehicleVector) == 16 && sizeof(VehicleMatrix) == 64 && sizeof(SourceSpriteRecord) == 104);
static_assert(sizeof(CameraVector) == 16 && sizeof(contact::Vector4) == 16);
using sgn::bit_reinterpret::as_float;
using sgn::bit_reinterpret::as_word;
W load_word(const void *p, unsigned offset)
{
    W x;
    std::memcpy(&x, static_cast<const std::byte *>(p) + offset, 4);
    return x;
}
void admit(RaceSessionPlatform &p, W site)
{
    p.require_owner_thread();
    if (!p.source_construction_only_entry())
        throw SourceAllocationError(site, "vehicle presentation requires source platform");
}
unsigned current(RaceSessionState &w, W site)
{
    const auto slot = w.current_slot;
    if (slot >= VehicleStorage::physical_slots)
        throw SourceAllocationError(site, "current vehicle outside source owners");
    return slot;
}
unsigned camera(RaceSessionState &w, W site)
{
    const auto i = w.controls.current;
    if (i >= w.controls.controllers.size())
        throw SourceAllocationError(site, "current camera outside source owners");
    return i;
}
SourcePmdModelView model(RaceSessionPlatform &p, unsigned slot, W site)
{
    const auto handle = p.state().vehicles.models[slot].model;
    if (!handle)
        throw SourceAllocationError(site, "null vehicle PMD");
    return p.source_scene_resources().models().source_at(*handle);
}
W offset(W p, W n, W site)
{
    if (!p || n > std::numeric_limits<W>::max() - p)
        throw SourceAllocationError(site, "null or overflowing PMD address");
    return p + n;
}
W read(SourcePmdModelView m, W p, W n, W site)
{
    return m.bytes().field<W>(offset(p, n, site)).value(site);
}
void write(SourcePmdModelView m, W p, W n, W v, W site)
{
    m.bytes().field<W>(offset(p, n, site)).set(v);
}
T transport(SourcePmdModelView m, W p, W n, W site)
{
    return m.bytes().load_transport_word(offset(p, n, site), site);
}
void write(SourcePmdModelView m, W p, W n, const T &v, W site)
{
    m.bytes().store_transport_word(offset(p, n, site), v, site);
}

W selected(SourcePmdModelView m, W site)
{
    const auto d = m.descriptor();
    const W lod = d.field<W>(0x5c).value();
    if (lod >= 4)
        throw SourceAllocationError(site, "selected source LOD outside four slots");
    return d.field<W>(0x48 + lod * 4).value();
}
void copy_local(SourcePmdModelView m, W dst, W at, const void *data, unsigned n, W site)
{
    for (unsigned i = 0; i < n; ++i)
        write(m, dst, at + i * 4, load_word(data, i * 4), site);
}
void copy_actual(SourcePmdModelView m, W dst, W dst_at, W src, W src_at, W site)
{
    for (W i = 0; i < 4; ++i)
    {
        const auto x = transport(m, src, src_at + i * 4, site);
        write(m, dst, dst_at + i * 4, x, site);
    }
}
// These are the same guest stack DWORDs, not values reloaded from mutable PMD.
// 414350 gives body/wheels/basis EBP=S-12; see the sealed stack-provenance packet.
struct BodyStackWords
{
    T up, forward;
};
BodyStackWords body_4138a0(RaceSessionPlatform &p, SourceVehicleDrawStack &stack)
{
    auto &w = p.state();
    auto slot = current(w, 0x4138a6);
    const auto m = model(p, slot, 0x4138b4);
    const W head = selected(m, 0x4138b7);
    VehicleVector up{w.vehicles.physics[slot].body_roll, 1, 0, 0};
    sgn_vdp_normalize_44ed40(&up);
    slot = current(w, 0x4138da);
    VehicleVector forward{0, w.vehicles.physics[slot].body_pitch, 1, 0};
    sgn_vdp_normalize_44ed40(&forward);
    W dst = read(m, head, 8, 0x4138ff);
    copy_local(m, dst, 0x30, &up, 3, 0x413908);
    const T up_w = stack.transport(0x4138a0, 0x413917, -4);
    write(m, dst, 0x3c, up_w, 0x41391a);
    copy_local(m, dst, 0x20, &forward, 3, 0x413923);
    const T forward_w = stack.transport(0x4138a0, 0x413932, -0x14);
    write(m, dst, 0x2c, forward_w, 0x413935);
    slot = current(w, 0x413938);
    write(m, dst, 4, as_word(w.vehicles.physics[slot].body_heave), 0x413944);
    dst = read(m, head, 0x148, 0x413947);
    copy_local(m, dst, 0x30, &up, 3, 0x413953);
    write(m, dst, 0x3c, up_w, 0x413965);
    copy_local(m, dst, 0x20, &forward, 3, 0x41396e);
    write(m, dst, 0x2c, forward_w, 0x413980);
    write(m, dst, 4, as_word(w.vehicles.physics[slot].body_heave), 0x413989);
    if (w.vehicles.trail_inputs[slot].field_28a0 != 1)
        return {up_w, forward_w};
    dst = read(m, head, 0x288, 0x413997);
    copy_local(m, dst, 0x30, &up, 3, 0x4139a3);
    write(m, dst, 0x3c, up_w, 0x4139b5);
    copy_local(m, dst, 0x20, &forward, 3, 0x4139be);
    write(m, dst, 0x2c, forward_w, 0x4139d0);
    write(m, dst, 4, as_word(w.vehicles.physics[slot].body_heave), 0x4139d9);
    return {up_w, forward_w};
}
void wheels_4139f0(RaceSessionPlatform &p, SourceVehicleDrawStack &stack, const T &body_forward_w)
{
    auto &w = p.state();
    auto slot = current(w, 0x4139f9);
    const auto m = model(p, slot, 0x413a08);
    const W head = selected(m, 0x413a0b);
    VehicleMatrix steering{};
    sgn_vdp_steering_44f800(steering.data(), as_word(w.vehicles.physics[slot].steering), vdp_literals::trig.data());
    slot = current(w, 0x413a1e);
    W dst = read(m, head, 0x48, 0x413a23);
    copy_local(m, dst, 0x30, w.vehicles.wheels[slot][0].rotation.data() + 4, 4, 0x413a34);
    copy_local(m, dst, 0x20, w.vehicles.wheels[slot][0].rotation.data() + 8, 4, 0x413a54);
    dst = read(m, head, 0x88, 0x413a68);
    copy_local(m, dst, 0x30, w.vehicles.wheels[slot][1].rotation.data() + 4, 4, 0x413a7c);
    copy_local(m, dst, 0x20, w.vehicles.wheels[slot][1].rotation.data() + 8, 3, 0x413a9c);
    const W third = read(m, head, 0xc8, 0x413aaa);
    write(m, dst, 0x2c, as_word(w.vehicles.wheels[slot][1].rotation[11]), 0x413ab3);
    VehicleMatrix product{};
    sgn_vdp_multiply_44efd0(product.data(), w.vehicles.wheels[slot][2].rotation.data(), steering.data());
    const W fourth = read(m, head, 0x108, 0x413ad1);
    copy_local(m, third, 0x30, product.data() + 4, 3, 0x413ad7);
    const T up_w = stack.transport(0x4139f0, 0x413ae5, -0x24);
    write(m, third, 0x3c, up_w, 0x413ae8);
    copy_local(m, third, 0x20, product.data() + 8, 3, 0x413aee);
    // 413AFF reads S-32, already captured at413932; 44EFD0 never stores this lane.
    const T forward_w = body_forward_w;
    write(m, third, 0x2c, forward_w, 0x413b03);
    slot = current(w, 0x413b06); // 44EFD0 reads/writes nine XYZ lanes; W remains typed metadata
    sgn_vdp_multiply_44efd0(product.data(), w.vehicles.wheels[slot][3].rotation.data(), steering.data());
    copy_local(m, fourth, 0x30, product.data() + 4, 3, 0x413b24);
    write(m, fourth, 0x3c, up_w, 0x413b35);
    copy_local(m, fourth, 0x20, product.data() + 8, 3, 0x413b3b);
    write(m, fourth, 0x2c, forward_w, 0x413b4c);
}
W basis_413b60(RaceSessionPlatform &p, SourceVehicleDrawStack &stack, const T &known_xw, std::optional<T> known_yw = {})
{
    auto &w = p.state();
    const auto m = model(p, current(w, 0x413b66), 0x413b72);
    const W head = m.descriptor().field<W>(0x48).value();
    const W transform = read(m, head, 8, 0x413b78);
    // Numerical local contains only read operands. The raw PMD forward remains
    // the actual source of44F050's later interleaved reads, including W.
    std::array<W, 8> operands{};
    // Only the six reached XYZ lanes, in first-access order. Neither W is
    // observed here: forward.W is a later44F0CC access after matrix writes.
    operands[1] = read(m, transform, 0x24, 0x413b7b);
    operands[6] = read(m, transform, 0x38, 0x413b7e);
    operands[2] = read(m, transform, 0x28, 0x413b81);
    operands[5] = read(m, transform, 0x34, 0x413b84);
    operands[4] = read(m, transform, 0x30, 0x413b92);
    operands[0] = read(m, transform, 0x20, 0x413b98);
    std::array<VehicleVector, 2> axes{};
    sgn_vdp_cross_413b60(axes.data(), operands.data());
    sgn_vdp_normalize_44ed40(&axes[0]);
    sgn_vdp_normalize_44ed40(&axes[1]);
    auto out = [&](unsigned at, const auto &x) { w.model_basis.store(at, x); };
    const W x = as_word(axes[0].x);
    out(0, 0);
    out(1, 0);
    out(2, 0);
    out(3, 0);
    out(0, x);
    const W y = as_word(axes[0].y);
    out(4, 0);
    out(1, y);
    const W z = as_word(axes[0].z);
    const T xw = known_xw;
    out(5, 0);
    out(2, z);
    out(6, 0);
    out(7, 0);
    out(8, 0);
    out(3, xw);
    out(4, as_word(axes[1].x));
    out(5, as_word(axes[1].y));
    const W yz = as_word(axes[1].z);
    const T yw = known_yw ? *known_yw : stack.transport(0x413b60, 0x44f09b, -0x14);
    out(6, yz);
    out(7, yw);
    out(9, 0);
    out(10, 0);
    const W zx = read(m, transform, 0x20, 0x44f0ad);
    out(11, 0);
    out(8, zx);
    out(12, 0);
    const W zy = read(m, transform, 0x24, 0x44f0b8);
    out(13, 0);
    out(9, zy);
    out(14, 0);
    const W zz = read(m, transform, 0x28, 0x44f0c4);
    out(10, zz);
    const T zw = transport(m, transform, 0x2c, 0x44f0cc);
    out(15, 0x3f800000);
    out(11, zw);
    return yz; // 44ED91/44ED6F leaves this exact DWORD at S-36 in414350.
}
void segments_413e60(RaceSessionPlatform &p, W basis_yz)
{
    auto &w = p.state();
    const auto slot = current(w, 0x413e66);
    if (w.vehicles.presentation[slot].field_3134 == 0)
        return;
    auto &v = w.vehicles.physics[slot];
    auto &anchor = w.vehicles.anchors[slot];
    std::array<VehicleVector, 3> local{};
    const std::array<const void *, 3> initial{&anchor.position, w.model_basis.xyz_data(0x413e60), &v.body_heave};
    sgn_vdp_segment_start(local.data(), initial.data());
    const auto m = model(p, slot, 0x413f21);
    const W head = selected(m, 0x413f27);
    W dst = read(m, head, 0x2c8, 0x413f2b);
    for (unsigned i = 0; i < 6; ++i)
    {
        const VehicleVector old = local[0];
        const std::array<const void *, 3> inputs{&w.vehicles.segments[slot].items[i].direction,
                                                 w.model_basis.xyz_data(0x413e60), &anchor.extent};
        sgn_vdp_segment_step(local.data(), inputs.data());
        copy_local(m, dst, 0x20, &local[1], 4, 0x413fa0);
        copy_local(m, dst, 0, &old, 3, 0x41400e);
        // 41401F reads S-36, the previous normalized basis-Y.Z.
        write(m, dst, 12, basis_yz, 0x41402a);
        if (i != 5)
            dst = offset(dst, 0x50, 0x41403f);
    }
    const std::array<const void *, 5> inputs{&local[0], &v.basis_x, &v.up, &v.basis_z, &v.position};
    sgn_vdp_segment_world(&w.vehicles.rail[slot].effect_position, inputs.data());
}
void anchors_414130(RaceSessionPlatform &p, W basis_yz)
{
    auto &w = p.state();
    const auto slot = current(w, 0x414136);
    auto &a = w.vehicles.anchors[slot];
    if (a.effect_available[0] == 0)
        return;
    const auto m = model(p, slot, 0x414153);
    const W head = selected(m, 0x414156);
    const W first = read(m, head, 0x4c8, 0x41415d);
    const W body_up = read(m, head, 8, 0x414163);
    copy_actual(m, first, 0x30, body_up, 0x30, 0x41416e);
    const W body_forward = read(m, head, 8, 0x41418a);
    copy_actual(m, first, 0x20, body_forward, 0x20, 0x414193);
    VehicleVector local;
    const std::array<const void *, 3> inputs{&a.effect_position[0], w.model_basis.xyz_data(0x414130),
                                             &w.vehicles.physics[slot].body_heave};
    sgn_vdp_anchor(&local, inputs.data());
    copy_local(m, first, 0, &local, 3, 0x414209);
    // 414364 tail-jumps here: EBP=S-8, so EBP-1C is the same S-36.
    const W position_w = basis_yz;
    write(m, first, 12, position_w, 0x41424a);
    if (a.effect_available[1] == 0)
        return;
    // 414260 reads first up DWORD before capturing part20's transform.
    const W up_x = read(m, body_up, 0x30, 0x414260);
    const W second = read(m, head, 0x508, 0x414262);
    write(m, second, 0x30, up_x, 0x41426b);
    const W up_y = read(m, body_up, 0x34, 0x41426e);
    write(m, second, 0x34, up_y, 0x414271);
    const W up_z = read(m, body_up, 0x38, 0x414274);
    const T up_w = transport(m, body_up, 0x3c, 0x414277);
    write(m, second, 0x38, up_z, 0x41427a);
    write(m, second, 0x3c, up_w, 0x41427d);
    const W fx = read(m, body_forward, 0x20, 0x414288);
    write(m, second, 0x20, fx, 0x41428a);
    const W fy = read(m, body_forward, 0x24, 0x41428c);
    write(m, second, 0x24, fy, 0x41428f);
    const W fz = read(m, body_forward, 0x28, 0x414292);
    const T fw = transport(m, body_forward, 0x2c, 0x414295);
    write(m, second, 0x28, fz, 0x414298);
    write(m, second, 0x2c, fw, 0x41429b);
    const std::array<const void *, 3> second_inputs{&a.effect_position[1], w.model_basis.xyz_data(0x414130),
                                                    &w.vehicles.physics[slot].body_heave};
    sgn_vdp_anchor(&local, second_inputs.data());
    copy_local(m, second, 0, &local, 3, 0x4142fe);
    write(m, second, 12, position_w, 0x41433c);
}
bool mounted(RaceSessionState &w, unsigned cam, unsigned slot)
{
    const auto &c = w.controls.controllers[cam];
    return c.vehicle == slot && c.camera.mode < 10;
}
void ring(RaceSessionPlatform &p, SourceVehicleRingRequired &r, W site, SourceVehicleRingKind kind, unsigned vehicle,
          unsigned side, W profile, W cursor, W count, float scale, float alpha)
{
    r.append_41f2f0(p, site, {kind, vehicle, side, profile, cursor, count, scale, alpha});
}
void sprite(RaceSessionPlatform &p, const SourceSpriteRecord &local)
{
    auto &w = p.state();
    const auto i = camera(w, 0x434590);
    // NEW HOST (R28): glow sprites move with their vehicle (interpolation tag, read-only).
    host_presentation::EffectSpriteScope effect(w.sprite_packets, host_presentation::attached_effect(w.current_slot));
    append_source_sprite_packet_434590(w.sprite_packets, local,
                                       {w.controls.controllers[i].camera, w.camera_lifecycle[i],
                                        w.camera_presentation[i], w.overlays[i], &w.view_matrices,
                                        &w.shared_model_identity});
}
void project(RaceSessionState &w, unsigned slot, const VehicleVector &anchor, CameraVector &out)
{
    auto &v = w.vehicles.physics[slot];
    const std::array<const void *, 7> in{
        &anchor, w.model_basis.xyz_data(0x415190), &v.basis_x, &v.up, &v.basis_z, &v.position, &v.body_heave};
    sgn_vdp_project_415190(&out, in.data());
}
} // namespace
std::span<const VehicleVector> borrow_source_vehicle_ring_41f2f0(RaceSessionPlatform &p,
                                                                 const SourceVehicleRingCapture &c)
{
    admit(p, 0x41f2f0);
    auto &w = p.state();
    if (c.vehicle >= VehicleStorage::physical_slots || c.side >= 2)
        throw SourceAllocationError(0x41f2f0, "vehicle ring identity outside existing owners");
    switch (c.kind)
    {
    case SourceVehicleRingKind::speed:
        if (c.side != 0 || c.profile_address != 0x474c08)
            break;
        return w.vehicles.trails[c.vehicle].speed;
    case SourceVehicleRingKind::effect:
        if (c.profile_address != 0x474af8)
            break;
        return w.vehicles.model_effects[c.vehicle].trails[c.side];
    case SourceVehicleRingKind::pair_a:
        if (c.profile_address != 0x474d18)
            break;
        return w.vehicles.trails[c.vehicle].pair_a[c.side];
    case SourceVehicleRingKind::pair_b:
        if (c.profile_address != 0x474e28)
            break;
        return w.vehicles.trails[c.vehicle].pair_b[c.side];
    }
    throw SourceAllocationError(0x41f2f0, "vehicle ring capture and profile identity disagree");
}
void present_source_vehicle_parts_414350(RaceSessionPlatform &p, SourceVehicleDrawStack &stack)
{
    admit(p, 0x414350);
    const auto words = body_4138a0(p, stack);
    wheels_4139f0(p, stack, words.forward);
    const W basis_yz = basis_413b60(p, stack, words.up, words.forward);
    segments_413e60(p, basis_yz);
    anchors_414130(p, basis_yz);
}
void append_source_vehicle_speed_trail_414cf0(RaceSessionPlatform &p, SourceVehicleRingRequired &r)
{
    admit(p, 0x414cf0);
    auto &w = p.state();
    if (w.impact_particles.rate_selector < 20)
        return;
    const auto cam = camera(w, 0x414cfd);
    const auto slot = current(w, 0x414d02);
    if (mounted(w, cam, slot))
        return;
    float alpha;
    if (!sgn_vdp_speed_alpha(&w.vehicles.physics[slot].speed, &alpha))
        return;
    const W cursor = w.vehicles.physics[slot].camera_effects.values[2],
            count = w.vehicles.physics[slot].camera_effects.values[3];
    ring(p, r, 0x414d6a, SourceVehicleRingKind::speed, slot, 0, 0x474c08, cursor, count, alpha, alpha);
}
void append_source_vehicle_effect_trails_414e50(RaceSessionPlatform &p, SourceVehicleRingRequired &r)
{
    admit(p, 0x414e50);
    auto &w = p.state();
    const auto slot = current(w, 0x414e54);
    float alpha = 0;
    if (w.vehicles.anchors[slot].effect_available[0] == 1 && w.vehicles.presentation[slot].field_33c8 == 1)
    {
        // FILD precedes count/cursor capture, which cannot mutate its source.
        const auto level = w.vehicles.physics[slot].launch_effect_level;
        const W count = w.vehicles.physics[slot].camera_effects.values[0],
                cursor = w.vehicles.model_effects[slot].cursor[0];
        alpha = as_float(sgn_vdp_launch_alpha(&level));
        ring(p, r, 0x414ea6, SourceVehicleRingKind::effect, slot, 0, 0x474af8, cursor, count, alpha, alpha);
    }
    if (w.vehicles.anchors[slot].effect_available[1] != 1 || w.vehicles.presentation[slot].field_3508 != 1)
        return;
    const W count = w.vehicles.physics[slot].camera_effects.values[1],
            cursor = w.vehicles.model_effects[slot].cursor[1];
    ring(p, r, 0x414ee2, SourceVehicleRingKind::effect, slot, 1, 0x474af8, cursor, count, alpha, alpha);
}
void append_source_vehicle_pair_a_trails_415040(RaceSessionPlatform &p, SourceVehicleRingRequired &r)
{
    admit(p, 0x415040);
    auto &w = p.state();
    if (w.impact_particles.rate_selector < 60)
        return;
    const auto slot = current(w, 0x415049);
    if (w.vehicles.trail_inputs[slot].field_28a0 == 0)
        return;
    const auto cam = camera(w, 0x415058);
    if (mounted(w, cam, slot))
        return;
    const W cursor = w.vehicles.trails[slot].cursor_a[0], count = w.vehicles.physics[slot].camera_effects.values[4];
    ring(p, r, 0x415093, SourceVehicleRingKind::pair_a, slot, 0, 0x474d18, cursor, count, 1, 1);
    const W second_count = w.vehicles.physics[slot].camera_effects.values[5],
            second_cursor = w.vehicles.trails[slot].cursor_a[1];
    ring(p, r, 0x4150bb, SourceVehicleRingKind::pair_a, slot, 1, 0x474d18, second_cursor, second_count, 1, 1);
}
void append_source_vehicle_pair_b_trails_4153a0(RaceSessionPlatform &p, SourceVehicleRingRequired &r)
{
    admit(p, 0x4153a0);
    auto &w = p.state();
    if (w.impact_particles.rate_selector < 60)
        return;
    const auto cam = camera(w, 0x4153b4);
    const auto captured = current(w, 0x4153b9);
    if (mounted(w, cam, captured) || w.vehicles.trail_inputs[captured].field_28a0 == 0)
        return;
    const float alpha = as_float(
        sgn_vdp_distance_alpha(&w.vehicles.physics[captured].position, &w.controls.controllers[cam].camera.position));
    auto predicate_slot = captured;
    if (w.vehicles.trail_inputs[captured].pair_b_available[0] == 1)
    {
        const W count = w.vehicles.physics[captured].camera_effects.values[6],
                cursor = w.vehicles.trails[captured].cursor_b[0];
        ring(p, r, 0x415475, SourceVehicleRingKind::pair_b, captured, 0, 0x474e28, cursor, count, 1, alpha);
        predicate_slot = current(w, 0x41547a);
    }
    if (w.vehicles.trail_inputs[predicate_slot].pair_b_available[1] != 1)
        return;
    const W count = w.vehicles.physics[captured].camera_effects.values[7],
            cursor = w.vehicles.trails[captured].cursor_b[1];
    ring(p, r, 0x4154a8, SourceVehicleRingKind::pair_b, captured, 1, 0x474e28, cursor, count, 1, alpha);
}
void append_source_vehicle_glows_4154c0(RaceSessionPlatform &p, SourceVehicleDrawStack &stack)
{
    admit(p, 0x4154c0);
    auto &w = p.state();
    auto slot = current(w, 0x4154c6);
    if (w.vehicles.trail_inputs[slot].field_28a0 == 0)
        return;
    SourceSpriteRecord local;
    std::memcpy(&local, vdp_literals::sprite.data(), sizeof(local));
    local.texture = 6;
    local.flags = 0x60;
    if (w.vehicles.trail_inputs[slot].pair_b_available[0] == 0)
        return;
    auto &phases = w.vehicles.model_effects[slot].glow_phases;
    phases[0] = (phases[0] + 32u) & 8191u;
    const W second = phases[1];
    const auto cam = w.controls.current;
    phases[1] = (second + 2u) & 8191u;
    // 415521 captures a pointer only; the phase1 store precedes its first read.
    if (cam >= w.controls.controllers.size())
        throw SourceAllocationError(0x41553b, "glow camera outside source owners");
    const std::array<const void *, 5> in{&w.vehicles.physics[slot].position,
                                         &w.controls.controllers[cam].camera.position, &phases[0], &phases[1],
                                         vdp_literals::trig.data()};
    // 4155B4's _ftol leaves oldCW:truncCW at S-128. In this call context
    // 413B60 reads it as local-X.W; local-Y.W at S-144 remains a separate frontier.
    const W basis_xw = sgn_vdp_glow(&local, in.data());
    (void)basis_413b60(p, stack, T::known(basis_xw));
    slot = current(w, 0x4155eb);
    project(w, slot, w.vehicles.trail_inputs[slot].pair_b[0], local.position);
    sprite(p, local);
    slot = current(w, 0x415604);
    if (w.vehicles.trail_inputs[slot].pair_b_available[1] == 0)
        return;
    project(w, slot, w.vehicles.trail_inputs[slot].pair_b[1], local.position);
    sprite(p, local);
}
void SourceVehicleDrawPresentation::invoke(RaceSessionPlatform &p, W at, W child, std::optional<unsigned> captured)
{
    if (at == 0x4181be && child == 0x414350)
    {
        present_source_vehicle_parts_414350(p, stack_);
        return;
    }
    if (at == 0x4181c8 && child == 0x414cf0)
    {
        append_source_vehicle_speed_trail_414cf0(p, rings_);
        return;
    }
    if (at == 0x4181cd && child == 0x414e50)
    {
        append_source_vehicle_effect_trails_414e50(p, rings_);
        return;
    }
    if (at == 0x4181d2 && child == 0x415040)
    {
        append_source_vehicle_pair_a_trails_415040(p, rings_);
        return;
    }
    if (at == 0x4181d7 && child == 0x4153a0)
    {
        append_source_vehicle_pair_b_trails_4153a0(p, rings_);
        return;
    }
    if (at == 0x4181dc && child == 0x4154c0)
    {
        append_source_vehicle_glows_4154c0(p, stack_);
        return;
    }
    remaining_.invoke(p, at, child, captured);
}
void SourceVehicleDrawPresentation::leader_notice(RaceSessionPlatform &p, unsigned leader, std::string_view text)
{
    remaining_.leader_notice(p, leader, text);
}
} // namespace sgn::retail
