#include "sgn/source_vehicle_particles.hpp"
#include "sgn/race_session.hpp"
#include "sgn/contact_load_particles.hpp"
#include "sgn/contact_particle_allocator.hpp"
#include "sgn/retail_effect_template.hpp"
#include "sgn/source_pmd_model.hpp"
#include "sgn/vehicle_stunt_awards.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

// R17 source-owner translations of the six vehicle particle children of
// 418830/418970. NEW source C++ translation (not x87-exact): arithmetic follows
// the original operand order with double intermediates where the x87 stack keeps
// a value and float rounding exactly where the original FSTs to a float. Branches
// reproduce the FCOMP/FNSTSW polarity including unordered (NaN) operands, and the
// integer conversions reproduce 462860 (__ftol: FISTP qword under chop, EAX = low
// DWORD, invalid -> 8000000000000000h). Reused verified leaves: 42F1C0
// (VehicleRandom::signed_value), 42F1F0 (vehicle_random_integer), 45E110
// (allocate_effect_particle), 4169E0 (initialize_engine_particle_staging), 417B50
// (emit_load_particle), 4864E8 REP MOVSD (copy_effect_template).
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
// clang-format off: keep the original using-declaration order (SortUsingDeclarations would reorder it)
using contact::effects::EffectParticle;
using contact::effects::allocate_effect_particle;
using contact::effects::copy_effect_template;
// clang-format on
using sgn::bit_reinterpret::as_float;
float f32(double value)
{
    return static_cast<float>(value);
}
void admit(RaceSessionPlatform &p, W site)
{
    p.require_owner_thread();
    if (!p.source_construction_only_entry())
        throw SourceAllocationError(site, "vehicle particles require the actual source platform");
}
unsigned current(RaceSessionState &w, W site)
{
    const auto slot = w.current_slot; // 4D5BB8
    if (slot >= VehicleStorage::physical_slots)
        throw SourceAllocationError(site, "particle current pointer leaves physical vehicle owners");
    return slot;
}
// FCOMP + FNSTSW + TEST AH,1: C0 is set for "below" and for unordered operands.
bool c0(double a, double b)
{
    return !(a >= b);
}
// FCOMP + FNSTSW + TEST AH,41h nonzero: below, equal or unordered.
bool c0_or_c3(double a, double b)
{
    return !(a > b);
}
// 462860: FSTCW/OR AH,0C/FLDCW/FISTP qword/FLDCW, returns EAX (low DWORD).
W ftol_462860(double value)
{
    if (!(value >= -9223372036854775808.0 && value < 9223372036854775808.0))
        return 0; // integer indefinite low DWORD
    return static_cast<W>(static_cast<std::uint64_t>(static_cast<std::int64_t>(value)));
}
using sgn::bit_reinterpret::as_signed;
SourcePmdModelView vehicle_model(RaceSessionPlatform &p, RaceSessionState &w, unsigned slot, W site)
{
    const auto &handle = w.vehicles.models[slot].model; // vehicle+26B0
    if (!handle)
        throw SourceAllocationError(site, "particle child reached a null vehicle model pointer");
    return p.source_scene_resources().models().source_at(*handle);
}
std::array<float, 3> part_position(SourcePmdModelView model, SourcePmdPartView part)
{
    // 416B83/416D3A: MOV EAX,[part+8]; reads the first three floats of that transform.
    const auto bytes = model.bytes().subview(part.transform().location().offset, 12);
    return {bytes.field<float>(0).value(), bytes.field<float>(4).value(), bytes.field<float>(8).value()};
}

// 416B83..416C49 (part17) and 416D3A..416DFE (part18).
void engine_position(EffectParticle &s, const VehicleState &car, const std::array<float, 3> &local)
{
    const double x = double(local[0]) * car.basis_x.x;             // 416B86 (kept on ST)
    const float y = f32(double(local[0]) * car.basis_x.y);         // 416B96
    const float z = f32(double(local[0]) * car.basis_x.z);         // 416BA1
    const double height = double(as_float(0x428c0000)) - local[1]; // 416BA4 70-y (kept on ST)
    const float ux = f32(height * car.up.x), uy = f32(height * car.up.y), uz = f32(height * car.up.z); // 416BB5..416BC9
    const float fx = f32(double(local[2]) * car.basis_z.x);                                            // 416BD5
    const float fy = f32(double(local[2]) * car.basis_z.y);                                            // 416BE1
    const float fz = f32(double(local[2]) * car.basis_z.z);                                            // 416BF4
    const double sx = (x + ux) + fx;                                                                   // 416BF7,416C0C
    const float sy1 = f32(double(uy) + y), sz1 = f32(double(uz) + z);                                  // 416C00,416C09
    const float sy = f32(double(fy) + sy1), sz = f32(double(fz) + sz1);                                // 416C15,416C1E
    s.position.x = f32(double(car.position.x) - sx);                                                   // 416C21..416C29
    s.position.y = f32(double(car.position.y) - sy);                                                   // 416C31..416C3A
    s.position.z = f32(double(car.position.z) - sz);                                                   // 416C40..416C49
}
// 416C7A..416D01 / 416E04..416E81.
void engine_velocity(EffectParticle &s, const VehicleState &car, VehicleRandom &random)
{
    s.velocity.x = f32(double(car.velocity.x) * 0.5);
    s.velocity.y = f32(double(car.velocity.y) * 0.5);
    s.velocity.z = f32(double(car.velocity.z) * 0.5);
    const float jx = f32(random.signed_value(1.0f));      // 416CBA
    const double jz = random.signed_value(1.0f);          // 416CC7 (kept on ST)
    s.velocity.x = f32(double(s.velocity.x) + jx);        // 416CCC..416CE6
    s.velocity.z = f32(double(s.velocity.z) + jz);        // 416CEC..416CF4
    s.angular_velocity = f32(random.signed_value(22.5f)); // 416CFC..416D01
}

// 4157F0 on the stack staging.
void initialize_tire_4157f0(EffectParticle &s, const WorldGravity &g)
{
    s.gravity_scale = f32(double(g.inverse_factor) * as_float(0xbe99999a));
    s.render_flags = 0x60;
    s.flags = 7;
    s.texture = 7;
    s.lifetime_mode = 2;
    s.base_color = 0x406090;
    s.lifetime = 30;
    s.field_68 = as_float(0x3c23d70a);
    s.render_enabled = 1;
    s.animation_binding = 0;
    s.scale_fade_in = 1.5f;
    s.scale_fade_out = 6.0f;
    s.base_scale = 1.0f;
    s.alpha_fade_in = 0.0f;
    s.alpha_fade_out = 21.0f;
    s.base_alpha = 255.0f;
}
// 416EA0 on the stack staging.
void initialize_landing_416ea0(EffectParticle &s, const WorldGravity &g)
{
    s.gravity_scale = f32(double(g.inverse_factor) * as_float(0xbe19999a));
    s.flags = 3;
    s.texture = 0;
    s.animation_binding = 0;
    s.render_flags = 0x60;
    s.lifetime_mode = 2;
    s.lifetime = 30;
    s.field_68 = as_float(0x3c23d70a);
    s.render_enabled = 1;
    s.base_scale = 0.5f;
    s.scale_fade_in = 6.0f;
    s.scale_fade_out = 12.0f;
    s.base_alpha = 128.0f;
    s.alpha_fade_in = as_float(0x3f199999);
    s.alpha_fade_out = 12.0f;
    s.base_color = 0x182010;
}
// 4174E0 (material2), 4175E0 (material4) and 4176F0 (material3): each first
// copies 4864E8 into staging+8, then writes its literal set.
struct SurfaceLiterals
{
    W velocity_scale, base_color, lifetime, field_68, scale_fade_in, scale_fade_out, gravity, slow_scale, fast_scale,
        alpha_fade_in, alpha_fade_out;
};
constexpr SurfaceLiterals surface_4174e0{0x3f000000, 0x402000,   30,         0x3c23d70a, 0x40c00000, 0x40c00000,
                                         0xbf19999a, 0x3dcccccd, 0x3f800000, 0x40c00000, 0x41a80000};
constexpr SurfaceLiterals surface_4175e0{0x3f400000, 0x101800,   18,         0x3d4ccccd, 0x40400000, 0x41100000,
                                         0xbe99999a, 0x3dcccccd, 0x3f400000, 0x40400000, 0x40c00000};
constexpr SurfaceLiterals surface_4176f0{0x3f000000, 0x401000,   18,         0x3dcccccd, 0x40400000, 0x41400000,
                                         0xbe99999a, 0x3d99999a, 0x3f800000, 0x40400000, 0x40c00000};
void initialize_surface(EffectParticle &s, const SurfaceLiterals &k, const VehicleState &car, const WorldGravity &g,
                        const ModelTransform &pose)
{
    copy_effect_template(s, pose);
    s.render_flags = 0x60;
    s.flags = 3;
    s.lifetime_mode = 2;
    s.base_color = k.base_color;
    s.texture = 9;
    s.lifetime = k.lifetime;
    s.field_68 = as_float(k.field_68);
    s.render_enabled = 1;
    s.animation_binding = 0;
    s.velocity.x = f32(double(car.velocity.x) * as_float(k.velocity_scale));
    s.velocity.y = f32(double(car.velocity.y) * as_float(k.velocity_scale));
    s.velocity.z = f32(double(car.velocity.z) * as_float(k.velocity_scale));
    s.scale_fade_in = as_float(k.scale_fade_in);
    s.scale_fade_out = as_float(k.scale_fade_out);
    s.gravity_scale = f32(double(g.inverse_factor) * as_float(k.gravity));
    if (c0(car.speed, 10.0))
        s.base_scale = f32(double(car.speed) * as_float(k.slow_scale)); // 417591..4175B0
    else
        s.base_scale = as_float(k.fast_scale);
    s.alpha_fade_in = as_float(k.alpha_fade_in);
    s.alpha_fade_out = as_float(k.alpha_fade_out);
    s.base_alpha = 255.0f;
}
// 417350 on the stack staging (no 4864E8 copy).
void initialize_launch_417350(EffectParticle &s)
{
    s.flags = 3;
    s.texture = 0;
    s.animation_binding = 0;
    s.render_flags = 0x60;
    s.lifetime_mode = 2;
    s.lifetime = 41;
    s.field_68 = as_float(0x3c23d70a);
    s.gravity_scale = 0.25f;
    s.render_enabled = 1;
    s.base_scale = 1.0f;
    s.scale_fade_in = as_float(0x3f199999);
    s.scale_fade_out = 24.0f;
    s.base_alpha = 224.0f;
    s.alpha_fade_in = as_float(0x3f199999);
    s.alpha_fade_out = 18.0f;
    s.base_color = 0x182010;
}
} // namespace

void emit_source_engine_particles_416a80(RaceSessionPlatform &p)
{
    admit(p, 0x416a80);
    auto &w = p.state();
    const auto slot = current(w, 0x416a87);
    const auto &car = w.vehicles.physics[slot];
    const auto &parameters = w.vehicles.parameters[slot];
    auto &state = w.vehicles.particles[slot];
    auto &global = w.effect_staging;
    auto &staging = global.particle;
    const auto model = vehicle_model(p, w, slot, 0x416a8e);
    // 416A8E..416AD1: (count>17 && turbo<=1 && speed<=30) || clutch<1, x87 polarity.
    const bool admitted =
        (model.part_count() > 0x11 && c0_or_c3(car.turbo_rpm, 1.0) && c0_or_c3(car.speed, 30.0)) || c0(car.clutch, 1.0);
    if (!admitted)
        return;
    // 416AD7..416AEF: ftol(rpm/rpm_max*64+7).
    auto interval = as_signed(ftol_462860((double(car.rpm) / parameters.rpm_max) * 64.0 + 7.0));
    if (c0(car.clutch, 0.25))
        interval = vehicle_random_integer(w.random, 4) + 4; // 416AF4..416B1B
    ++state.field_7628;
    if (as_signed(state.field_7628) < interval)
        return;                                                                               // 416B1E..416B2F
    const float remaining = f32((double(parameters.rpm_max) - car.rpm) / parameters.rpm_max); // 416B35..416B7A
    state.field_7628 = 0;                                                                     // 416B57
    copy_effect_template(staging, w.model_draws.template_pose);                               // 416B42..416B67
    const auto lod = model.current_detail_level().value();                                    // 416B69 selector
    const auto first = model.part(model.source_lod_index(lod), 17);                           // 416B6D..416B71 +440
    if (first.flags_02().value() & 4)
    { // 416B76
        engine_position(staging, car, part_position(model, first));
        if (global.initialized == 0)
        { // 416BED..416C5C
            contact::effects::initialize_engine_particle_staging(global, w.gravity);
            global.initialized = 1;
        }
        staging.lifetime = ftol_462860(double(remaining) * 30.0 + 36.0); // 416C66..416C8B
        engine_velocity(staging, car, w.random);
        (void)allocate_effect_particle(staging, w.effect_particles); // 416D07..416D0C
    }
    if (model.part_count() <= 0x12)
        return;                                                      // 416D17..416D21
    const auto second = model.part(model.source_lod_index(lod), 18); // 416D27..416D2B +480
    if (!(second.flags_02().value() & 4))
        return; // 416D30
    engine_position(staging, car, part_position(model, second));
    engine_velocity(staging, car, w.random);
    (void)allocate_effect_particle(staging, w.effect_particles); // 416E87..416E8C
}

void emit_source_tire_particles_415880(RaceSessionPlatform &p)
{
    admit(p, 0x415880);
    auto &w = p.state();
    const auto slot = current(w, 0x415889);
    const auto &car = w.vehicles.physics[slot];
    const auto &presentation = w.vehicles.presentation[slot];
    auto &state = w.vehicles.particles[slot];
    // 415892..4158CC: up * -30.000002.
    const float shift = as_float(0xc1f00001);
    const float ox = f32(double(car.up.x) * shift), oy = f32(double(car.up.y) * shift),
                oz = f32(double(car.up.z) * shift);
    bool first = true;
    EffectParticle staging; // stack EBP-0FC: 64,90,94,C4,C8,CC are never written; C0 is written below.
    staging.source_undefined.fill(true);
    staging.source_undefined[3] = false;
    for (unsigned j = 0; j < 4; ++j)
    {
        if (!c0(car.speed, 100.0))
            continue; // 4158CF..4158E0
        const auto &wheel = car.wheels[j];
        auto &accumulated = state.field_1ff8[j];
        if (c0(wheel.grip_usage, 3.5))
        { // 4158E6..4158F8
            const double decreased = double(accumulated) - 2.0;
            accumulated = f32(decreased); // 415938..415940 FST
            if (c0(decreased, 20.0))
                accumulated = 20.0f; // 415942..41594F
        }
        else
        {
            double usage = std::fabs(double(wheel.grip_usage) * 0.25); // 4158FA..415907
            if (!c0_or_c3(usage, 1.0))
                usage = 1.0; // 415909..415918
            accumulated =
                f32(((usage * wheel.surface_grip) * 20.0) / wheel.surface_rolling + accumulated); // 41591E..415934
        }
        if (c0_or_c3(accumulated, 200.0))
            continue;                                                   // 415955..415962
        const double scaled = double(car.speed) * as_float(0x3a83126f); // 415968..41596E
        float amount = f32(scaled);                                     // 415974 FST
        if (c0(scaled, as_float(0x3c23d70a)))
            amount = as_float(0x3c23d70a); // 415977..415984
        accumulated = 175.0f;              // 41598E
        if (first)
        {
            initialize_tire_4157f0(staging, w.gravity);
            copy_effect_template(staging, w.model_draws.template_pose);
            first = false;
        } // 415996..4159C5
        const auto &center = presentation.wheel_centers[j];
        staging.position.x = f32(double(ox) + center.x);
        staging.position.y = f32(double(oy) + center.y);
        staging.position.z = f32(double(oz) + center.z); // 4159CC..4159F4
        staging.velocity.x = f32(double(amount) * car.velocity.x);
        staging.velocity.y = f32(double(amount) * car.velocity.y);
        staging.velocity.z = f32(double(amount) * car.velocity.z); // 4159F7..415A1E
        const float jx = f32(w.random.signed_value(5.0f));         // 415A24..415A29
        const double jz = w.random.signed_value(5.0f);             // 415A31
        staging.velocity.x = f32(double(jx) + staging.velocity.x); // 415A36..415A51
        staging.velocity.z = f32(jz + staging.velocity.z);         // 415A57..415A5D
        if (j >= 2)
            staging.base_scale = as_float(0x3f333333);                                               // 415A45..415A65
        staging.angle = f32(w.random.signed_value(360.0f));                                          // 415A6C..415A76
        staging.angular_velocity = f32(w.random.signed_value(45.0f) - double(as_float(0x41b40000))); // 415A79..415A8F
        (void)allocate_effect_particle(staging, w.effect_particles);                                 // 415A89..415A92
    }
}

void emit_source_load_particles_417ca0(RaceSessionPlatform &p)
{
    admit(p, 0x417ca0);
    auto &w = p.state();
    const auto slot = current(w, 0x417ca8);
    const auto &car = w.vehicles.physics[slot];
    const auto &contact = w.vehicles.contact[slot];
    auto &state = w.vehicles.particles[slot];
    auto &audio = w.vehicles.audio[slot];
    auto &bank = w.impact_particles;
    if (car.contact_class == 1 || c0(car.speed, 10.0))
    {
        state.field_761c = 0;
        return;
    } // 417CB4..417CD0, 417DF4
    const float threshold = f32((double(w.gravity.magnitude) * car.effective_mass) * 3.0); // 417CD6..417CF5
    unsigned loaded = 0;
    double sum = 0.0; // 417CE2,417CF8
    for (unsigned j = 0; j < 4; ++j)
    { // 417CFE..417D19
        const auto &wheel = contact.wheels[j];
        if (wheel.type == 1 && !c0_or_c3(wheel.current_load, threshold))
            ++loaded;
        sum += wheel.current_load;
    }
    if (loaded >= 2)
    { // 417D1B..417D44
        auto increase = ftol_462860(sum * as_float(0x3b000000));
        if (increase > 8)
            increase = 8;
        if (as_signed(state.field_761c) < 15)
            state.field_761c += increase;
    }
    if (as_signed(state.field_761c) <= 0)
        return;                                                                         // 417D4A..417D52, 417E05
    const float strength = f32(sum * 0.25);                                             // 417D58,417D6A
    --state.field_761c;                                                                 // 417D5E..417D64
    const double ux = double(car.up.x) * -25.0;                                         // 417D6D..417D73
    const float uy = f32(double(car.up.y) * -25.0), uz = f32(double(car.up.z) * -25.0); // 417D79..417D97
    contact::Vector4 position{f32(ux + car.position.x), f32(double(uy) + car.position.y),
                              f32(double(uz) + car.position.z), 0}; // 417D9A..417DB8
    const contact::Vector4 velocity{car.velocity.x, car.velocity.y, car.velocity.z,
                                    std::bit_cast<std::uint32_t>(car.velocity_w)}; // EDX=vehicle+1C8C, 417B50 reads xyz
    for (unsigned j = 0; j < 2; ++j)
    { // 417DBD..417DD3
        // NEW policy for the never-written stack word EBP-18 (position W copied by
        // 417B80..417B92): retain the destination particle's current W.
        const auto destination = bank.cursor >= 32 ? 0u : bank.cursor;
        position.opaque = bank.particles[destination].position.opaque;
        (void)contact::effects::emit_load_particle(bank, w.random, car, position, velocity, strength); // 417DCD
    }
    audio.effect_pending = 1;
    audio.effect_strength = strength; // 417DDF..417DE9
}

void emit_source_landing_particles_416f30(RaceSessionPlatform &p)
{
    admit(p, 0x416f30);
    auto &w = p.state();
    const auto slot = current(w, 0x416f39);
    const auto &car = w.vehicles.physics[slot];
    const auto &presentation = w.vehicles.presentation[slot];
    if (c0(double(as_signed(car.stunt.landing_hold)), 30.0))
        return; // 416F3F..416F50 FILD/FCOMP double30
    const float threshold = f32((double(w.gravity.magnitude) * car.effective_mass) * 0.5); // 416F56..416F75
    bool first = true;
    EffectParticle staging; // stack EBP-110: 64,90,94,C0,C4,C8,CC are never written.
    staging.source_undefined.fill(true);
    float bx = 0, by = 0, bz = 0, lx = 0, ly = 0, lz = 0;
    for (unsigned j = 0; j < 4; ++j)
    {
        if (c0_or_c3(car.wheels[j].load, threshold))
            continue; // 416F78..416F87
        if (first)
        { // 416F8D..41705B
            copy_effect_template(staging, w.model_draws.template_pose);
            initialize_landing_416ea0(staging, w.gravity);
            const double vx = double(car.velocity.x) * 0.5;
            const float vy = f32(double(car.velocity.y) * 0.5), vz = f32(double(car.velocity.z) * 0.5);
            const double ux = double(car.up.x) * 1.5;
            const float uy = f32(double(car.up.y) * 1.5), uz = f32(double(car.up.z) * 1.5);
            bx = f32(ux + vx);
            by = f32(double(uy) + vy);
            bz = f32(double(uz) + vz); // 417018..41702E
            const float lift = as_float(0x42700001);
            lx = f32(double(car.up.x) * lift);
            ly = f32(double(car.up.y) * lift);
            lz = f32(double(car.up.z) * lift); // 417031..41705B
            first = false;
        }
        const auto &center = presentation.wheel_centers[j];
        staging.position.x = f32(double(center.x) - lx);
        staging.position.y = f32(double(center.y) - ly);
        staging.position.z = f32(double(center.z) - lz);                            // 41705E..417089
        const float ratio = f32(double(car.wheels[j].load) / threshold);            // 41708C..417096
        double side = std::fabs(w.random.signed_value(2.0f)) + double(ratio) * 2.0; // 417099..4170AE
        if (j & 1)
            side = -side;                                                           // 41709E..4170B2
        const float sx = f32(side * car.basis_x.x), sy = f32(side * car.basis_x.y); // 4170BF..4170D2
        const double sz = side * car.basis_x.z;                                     // 4170D5
        staging.velocity.x = f32(double(sx) + bx);
        staging.velocity.y = f32(double(sy) + by);
        staging.velocity.z = f32(sz + bz);                           // 4170DB..4170F6
        (void)allocate_effect_particle(staging, w.effect_particles); // 4170FC
    }
}

void emit_source_surface_particles_417800(RaceSessionPlatform &p)
{
    admit(p, 0x417800);
    auto &w = p.state();
    const auto slot = current(w, 0x41780a);
    const auto &car = w.vehicles.physics[slot];
    const auto &contact = w.vehicles.contact[slot];
    const auto &presentation = w.vehicles.presentation[slot];
    auto &state = w.vehicles.particles[slot];
    if (c0(car.speed, 2.0))
        return; // 417810..417821
    if (state.field_7620 != 0)
    {
        --state.field_7620;
        return;
    } // 417827..41783C
    std::int32_t last = 0;  // 41783D..417840
    EffectParticle staging; // stack EBP-0F4: 64,90,94,C0,C4,C8,CC are never written.
    staging.source_undefined.fill(true);
    float lx = 0, ly = 0, lz = 0;
    for (unsigned j = 0; j < 4; ++j)
    {
        const auto material = contact.wheels[j].type; // 417849
        const SurfaceLiterals *literals = nullptr;
        float amplitude = 0;
        if (material == 2)
        {
            literals = &surface_4174e0;
            amplitude = as_float(0x40f00001);
        }
        else if (material == 3)
        {
            literals = &surface_4176f0;
            amplitude = 1.0f;
        }
        else if (material == 4)
        {
            literals = &surface_4175e0;
            amplitude = 2.5f;
        }
        else
            continue; // 4179E8..4179EB
        if (car.wheels[j].contact_countdown <= 4)
            continue; // 417859/41792E/4179F1
        if (last != material)
        { // 417867/41793C/4179FE
            initialize_surface(staging, *literals, car, w.gravity, w.model_draws.template_pose);
            last = material;
            lx = f32(double(car.up.x) * 20.0);
            ly = f32(double(car.up.y) * 20.0);
            lz = f32(double(car.up.z) * 20.0);
        }
        const auto &center = presentation.wheel_centers[j];
        staging.position.x = f32(double(center.x) - lx);
        staging.position.y = f32(double(center.y) - ly);
        staging.position.z = f32(double(center.z) - lz);
        const float jx = f32(w.random.signed_value(amplitude));
        float jy = 0;
        if (material == 2)
            jy = f32(std::fabs(w.random.signed_value(2.5f))); // 4178E9..4178F5
        const double jz = w.random.signed_value(amplitude);
        staging.velocity.x = f32(double(staging.velocity.x) + jx);
        if (material == 2)
            staging.velocity.y = f32(double(staging.velocity.y) + jy); // 417911..41791A
        staging.velocity.z = f32(double(staging.velocity.z) + jz);     // 417AA5..417AB3
        (void)allocate_effect_particle(staging, w.effect_particles);   // 417ABB
    }
    if (last == 2 || last == 3)
    { // 417ADC..417B08, 417B0D..417B10
        const auto ticks = ftol_462860(100.0 / double(car.speed));
        state.field_7620 = ticks < 4 ? 4u : ticks;
    }
    else if (last == 4)
    { // 417B12..417B33
        const auto ticks = ftol_462860(25.0 / double(car.speed));
        state.field_7620 = ticks < 2 ? 2u : ticks;
    }
}

void emit_source_launch_wheel_particles_4173e0(RaceSessionPlatform &p)
{
    admit(p, 0x4173e0);
    auto &w = p.state();
    if (w.impact_particles.rate_selector < 0x50)
        return; // 4173E9..4173F0 (61297C)
    const auto slot = current(w, 0x4173f6);
    auto &car = w.vehicles.physics[slot];
    const auto &presentation = w.vehicles.presentation[slot];
    if (car.contact_counter != 0)
        return; // 4173FC..417404
    if (car.stunt.launch_state > 0x3c)
        return; // 41740A..417413
    ++car.stunt.launch_state;
    if (car.stunt.launch_state & 1)
        return;             // 417419..417422
    EffectParticle staging; // stack EBP-0E0: 64,90,94,C0,C4,C8,CC and frame 18..57 are never written.
    staging.source_undefined.fill(true);
    initialize_launch_417350(staging); // 417429..41742F
    std::array<bool, 16> undefined_frame;
    undefined_frame.fill(true);
    const float vx = f32(double(car.velocity.x) * 0.5), vy = f32(double(car.velocity.y) * 0.5),
                vz = f32(double(car.velocity.z) * 0.5); // 41743B..417465
    for (unsigned i = 0; i < 2; ++i)
    { // 41746A..4174D2
        const auto &center = presentation.wheel_centers[i];
        staging.position = {center.x, center.y, center.z, std::bit_cast<std::uint32_t>(center.w)}; // 41746F..417498
        staging.velocity.x = f32(w.random.signed_value(5.0f) + vx);                                // 417476..4174BB
        staging.velocity.y = vy;
        staging.velocity.z = vz;                                                      // 4174A6..4174B5
        (void)allocate_effect_particle(staging, w.effect_particles, undefined_frame); // 4174C1
    }
}
} // namespace sgn::retail
