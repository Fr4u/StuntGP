#include "sgn/vehicle_trail_render.hpp"
#include "sgn/retail_camera_tables.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "vehicle_presentation_frame_provider.hpp"
#include <bit>
#include <cmath>

namespace sgn::retail
{
VehicleTrailModel VehicleTrailBasisSource::read() const
{
    if (const auto *value = std::get_if<std::reference_wrapper<const VehicleTrailModel>>(&value_))
        return value->get();
    const auto &source = std::get<Registry>(value_);
    if (!*source.handle)
        throw FormatError("vehicle glow basis requires the current model binding");
    const auto handle = **source.handle;
    if (source.models->source_backed(handle))
    {
        const auto model = source.models->source_at(handle);
        const auto part = model.part(model.source_lod_index(0), 0);
        const auto at = part.transform().location().offset;
        const auto bytes = model.bytes();
        const auto forward = bytes.field<VehicleVector>(at + 0x20).value();
        const VehicleVector up{bytes.field<float>(at + 0x30).value(), bytes.field<float>(at + 0x34).value(),
                               bytes.field<float>(at + 0x38).value(), 0};
        return {forward, up, 0}; // upW and part23Z are not consumed by413B60.
    }
    auto &model = source.models->at(handle);
    const auto &t = model.part(model.source_lod_index(0), 0).transform;
    return {{t.forward.x, t.forward.y, t.forward.z, std::bit_cast<float>(t.forward_w)}, {t.up.x, t.up.y, t.up.z, 0}, 0};
}
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
bool hidden(const VehicleTrailRenderBindings &b)
{
    return b.current_vehicle_slot == b.camera_vehicle_slot && b.render.camera.camera.mode < 10;
}
void current(const VehicleTrailRenderBindings &b)
{
    if (&b.render.current_vehicle_orientation != &b.vehicle.orientation)
        throw FormatError("vehicle trail current owner mismatch");
}
std::uint16_t alpha(double value)
{
    if (!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        throw FormatError("vehicle glow alpha range");
    return static_cast<std::uint16_t>(static_cast<std::int64_t>(value));
}
double distance_squared(const VehicleTrailRenderBindings &b)
{
    const auto &p = b.vehicle.position;
    const auto &c = b.render.camera.camera.position;
    const double x = double(p.x) - c.x, y = double(p.y) - c.y, z = double(p.z) - c.z;
    return (z * z + y * y) + x * x;
}
SourceSpriteRecord sprite_template()
{
    constexpr std::array<std::uint32_t, 26> words{
        32, 8388609, 16777215, 16777215,   16777215, 16777215, 0,          0, 0, 0, 1065353216, 1065353216, 1065353216,
        0,  0,       0,        3212836864, 0,        0,        1065353216, 0, 0, 1, 0,          0,          2};
    return std::bit_cast<SourceSpriteRecord>(words);
}
} // namespace
void render_vehicle_speed_trail(VehicleTrailRenderBindings b)
{
    current(b);
    if (b.source_tick < 20 || hidden(b))
        return;
    const double speed = double(b.vehicle.speed) - as_float(0x42855555);
    if (speed < 0)
        return;
    const double scaled = speed * as_float(0x3cc49ba6);
    const float intensity = scaled > 1 ? 1 : f32(scaled);
    append_retail_trail(b.render, b.render.profiles.profiles[1], b.trails.speed, b.vehicle.camera_effects.values[2],
                        b.vehicle.camera_effects.values[3], intensity, intensity,
                        b.ring_audit ? std::span<const bool>(b.ring_audit->speed) : std::span<const bool>{});
}
void render_vehicle_effect_trails(VehicleTrailRenderBindings b)
{
    current(b);
    float intensity = 0;
    if (b.anchors.effect_available[0] == 1 && b.presentation.field_33c8 == 1)
    {
        intensity = f32(double(b.vehicle.launch_effect_level) * as_float(0x3b808081));
        append_retail_trail(b.render, b.render.profiles.profiles[0], b.effects.trails[0], b.effects.cursor[0],
                            b.vehicle.camera_effects.values[0], intensity, intensity,
                            b.ring_audit ? std::span<const bool>(b.ring_audit->effect[0]) : std::span<const bool>{});
    }
    if (b.anchors.effect_available[1] == 1 && b.presentation.field_3508 == 1)
        append_retail_trail(b.render, b.render.profiles.profiles[0], b.effects.trails[1], b.effects.cursor[1],
                            b.vehicle.camera_effects.values[1], intensity, intensity,
                            b.ring_audit ? std::span<const bool>(b.ring_audit->effect[1]) : std::span<const bool>{});
}
void render_vehicle_pair_a_trails(VehicleTrailRenderBindings b)
{
    current(b);
    if (b.source_tick < 60 || b.inputs.field_28a0 == 0 || hidden(b))
        return;
    for (unsigned j = 0; j < 2; ++j)
        append_retail_trail(b.render, b.render.profiles.profiles[2], b.trails.pair_a[j], b.trails.cursor_a[j],
                            b.vehicle.camera_effects.values[4 + j], 1, 1,
                            b.ring_audit ? std::span<const bool>(b.ring_audit->pair_a[j]) : std::span<const bool>{});
}
void render_vehicle_pair_b_trails(VehicleTrailRenderBindings b)
{
    current(b);
    if (b.source_tick < 60 || hidden(b) || b.inputs.field_28a0 == 0)
        return;
    float intensity = f32(distance_squared(b) * as_float(0x332bcc77));
    if (intensity < 0)
        intensity = 0;
    else if (intensity > 1)
        intensity = 1;
    for (unsigned j = 0; j < 2; ++j)
        if (b.inputs.pair_b_available[j] == 1)
            append_retail_trail(b.render, b.render.profiles.profiles[3], b.trails.pair_b[j], b.trails.cursor_b[j],
                                b.vehicle.camera_effects.values[6 + j], 1, intensity,
                                b.ring_audit ? std::span<const bool>(b.ring_audit->pair_b[j])
                                             : std::span<const bool>{});
}
void transform_vehicle_pair_b_anchor(const VehicleState &s, const VehicleMatrix &m, VehicleVector p, CameraVector &out)
{
    const double x = (double(p.y) * m[4] + double(p.z) * m[8]) + double(m[0]) * p.x;
    const float y = f32((double(p.y) * m[5] + double(p.z) * m[9]) + double(m[1]) * p.x),
                z = f32((double(p.y) * m[6] + double(p.z) * m[10]) + double(m[2]) * p.x);
    const double side = -x, lift = (70.0 - y) - s.body_heave;
    const float sx = f32(side * s.basis_x.x), sy = f32(side * s.basis_x.y);
    const double sz = side * s.basis_x.z;
    const float ux = f32(lift * s.up.x), uy = f32(lift * s.up.y);
    const double uz = lift * s.up.z;
    const float fx = f32(double(z) * s.basis_z.x), fy = f32(double(z) * s.basis_z.y), fz = f32(double(z) * s.basis_z.z);
    const float xy_x = f32(double(ux) + sx), xy_y = f32(double(uy) + sy), total_x = f32(double(fx) + xy_x),
                total_y = f32(double(fy) + xy_y);
    out.x = f32(double(s.position.x) - total_x);
    out.y = f32(double(s.position.y) - total_y);
    out.z = f32(double(s.position.z) - ((sz + uz) + fz));
}
void detail::render_vehicle_pair_b_glows_observed(VehicleTrailRenderBindings b, PresentationCallAudit *audit)
{
    current(b);
    if (b.inputs.field_28a0 == 0)
        return;
    auto sprite = sprite_template();
    sprite.texture = 6;
    sprite.flags = 0x60;
    if (b.inputs.pair_b_available[0] == 0)
        return;
    b.phases[0] = (b.phases[0] + 32u) & 8191u;
    b.phases[1] = (b.phases[1] + 2u) & 8191u;
    double intensity = std::sqrt(distance_squared(b)) * as_float(0x3983126f) + as_float(0x3dcccccd);
    if (intensity > 1)
        intensity = 1;
    const double size =
        double(as_float(camera_detail::trig_bits[b.phases[0]])) * as_float(0x3c23d70a) + (intensity + intensity);
    sprite.alpha = alpha(intensity * 192);
    const auto angle = (0u - b.phases[1]) & 8191u;
    sprite.up.x = as_float(camera_detail::trig_bits[angle + 2048]);
    sprite.up.y = as_float(camera_detail::trig_bits[angle]);
    sprite.up.z = 0;
    sprite.scale = {f32(size), f32(size), f32(size)};
    if (audit && audit->boundary)
        audit->boundary(audit->context, {0x4155e2, 0x413b60, b.current_vehicle_slot, false});
    prepare_vehicle_model_basis(b.model.read(), b.model_basis);
    if (audit && audit->boundary)
        audit->boundary(audit->context, {0x4155e2, 0x413b60, b.current_vehicle_slot, true});
    if (b.basis_audit && b.basis_audit->retained_word)
    {
        b.basis_audit->retained_word(b.basis_audit->context, b.model_basis[3], VehicleTrailOpaqueLane::basis_x);
        b.basis_audit->retained_word(b.basis_audit->context, b.model_basis[7], VehicleTrailOpaqueLane::basis_y);
    }
    transform_vehicle_pair_b_anchor(b.vehicle, b.model_basis, b.inputs.pair_b[0], sprite.position);
    append_sprite_packet(b.render.sprites, sprite, b.render.camera);
    if (b.inputs.pair_b_available[1] != 0)
    {
        transform_vehicle_pair_b_anchor(b.vehicle, b.model_basis, b.inputs.pair_b[1], sprite.position);
        append_sprite_packet(b.render.sprites, sprite, b.render.camera);
    }
}
void render_vehicle_pair_b_glows(VehicleTrailRenderBindings b)
{
    detail::render_vehicle_pair_b_glows_observed(b, nullptr);
}
} // namespace sgn::retail
