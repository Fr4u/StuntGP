#include "sgn/vehicle_model_effects.hpp"
#include "sgn/vehicle_trail_append.hpp"
#include "sgn/bit_reinterpret.hpp"
#include "vehicle_launch_provider.hpp"
#include <bit>
#include <limits>
namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
void write_alpha(std::span<std::array<std::uint8_t, 8>> attributes, std::uint8_t alpha)
{
    for (auto &a : attributes)
    {
        a[2] = alpha;
        a[3] = 0x60;
    }
}
std::uint8_t truncated_alpha(float value)
{
    const double number = value;
    if (!std::isfinite(number) || number < -9223372036854775808.0 || number >= 9223372036854775808.0)
        throw FormatError("brake alpha outside source finite int64 domain");
    return static_cast<std::uint8_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(number)) & 255u);
}
} // namespace
void update_vehicle_brake_lights(VehicleState &s, const VehicleTrailInputs &trails, VehicleModelEffects &effects,
                                 VehicleModelEffectResource &model)
{
    if (s.brake_signal != 0)
    {
        s.brake_signal = as_signed(std::bit_cast<std::uint32_t>(s.brake_signal) - 32u);
        if (s.brake_signal < 0)
            s.brake_signal = 0;
    }
    if (trails.field_28a0 == 1 && s.brake_signal < 120)
        s.brake_signal = 120;
    const auto lod = model.selected_lod();
    auto p = model.part(lod, 5);
    p.flags = static_cast<std::uint16_t>(s.brake_signal != 0);
    const auto current = std::bit_cast<std::uint32_t>(s.brake_signal);
    if (current != effects.cached_brake)
    {
        effects.cached_brake = current;
        write_alpha(model.attributes(lod, 5), static_cast<std::uint8_t>(current & 255u));
    }
}
void update_vehicle_route_lights(VehicleState &s, const VehicleRaceState &race, VehicleTrailInputs &trails,
                                 VehicleModelEffects &effects, VehicleModelEffectResource &model,
                                 VehicleRoutePresentation route)
{
    if (model.part_count() <= 10)
        return;
    if (route.point_count == 0)
        throw FormatError("route lights require nonzero first spline point count");
    const auto position = as_signed(race.spline_index);
    const std::uint32_t modulo = position < 0 ? route.point_count - ((0u - race.spline_index) % route.point_count)
                                              : race.spline_index % route.point_count;
    const auto index = (modulo << 7) / route.point_count; // source SHL wraps before unsigned DIV
    if (index > 128)
        throw FormatError("route lights index outside source table boundary");
    const float light = index == 128 ? std::bit_cast<float>(route.ambience[0]) : route.lights[index];
    if (!std::isnan(light) && light != 0)
    {
        if (trails.field_28a0 != 1)
        {
            trails.field_28a0 = 1;
            s.camera_effects.values[4] = 0;
            s.camera_effects.values[5] = 0;
            s.camera_effects.values[6] = 0;
            s.camera_effects.values[7] = 0;
            for (unsigned lod = 0; lod < 4; ++lod)
            {
                model.part(lod, 10).flags = 1;
                model.part(lod, 5).flags = 1;
            }
        }
    }
    else if (trails.field_28a0 != 0)
    {
        trails.field_28a0 = 0;
        for (unsigned lod = 0; lod < 4; ++lod)
        {
            model.part(lod, 10).flags = 0;
            model.part(lod, 5).flags = 0;
        }
    }
    effects.route_envelope = index == 128 ? route.ambience_adjacent_word : route.ambience[index];
}
namespace
{
struct LegacyBrakeWheel
{
    VehicleModelEffectPart destination, source;
    VehicleModelEffectResource &model;
    unsigned lod, index;
    void copy_forward()
    {
        destination.transform.forward = source.transform.forward;
        destination.transform.forward_w = source.transform.forward_w;
    }
    std::uint32_t up_first() const
    {
        return std::bit_cast<std::uint32_t>(source.transform.up.x);
    }
    void copy_up(std::uint32_t first)
    {
        destination.transform.up.x = std::bit_cast<float>(first);
        destination.transform.up.y = source.transform.up.y;
        const auto z = source.transform.up.z;
        const auto w = source.transform.up_w;
        destination.transform.up.z = z;
        destination.transform.up_w = w;
    }
    void flag(std::uint16_t value)
    {
        destination.flags = value;
    }
    void alpha(float strength)
    {
        const auto attributes = model.attributes(lod, index);
        if (!attributes.empty())
            write_alpha(attributes, truncated_alpha(strength));
    }
};
struct LegacyBrakeProvider
{
    VehicleModelEffectResource &model;
    void clear(unsigned raw)
    {
        model.part(raw, 6).flags = 0;
    }
    unsigned selected_lod() const
    {
        return model.selected_lod();
    }
    LegacyBrakeWheel wheel(unsigned lod, unsigned i)
    {
        return {model.part(lod, 6 + i), model.part(lod, 1 + i), model, lod, 6 + i};
    }
};
struct SourceBrakeWheel
{
    SourcePmdByteView bytes;
    SourcePmdPartView destination;
    std::uint32_t out, in;
    std::uint32_t read(unsigned at) const
    {
        return bytes.field<std::uint32_t>(in + at).value();
    }
    void write(unsigned at, std::uint32_t value)
    {
        bytes.field<std::uint32_t>(out + at).set(value);
    }
    void copy_forward()
    {
        for (unsigned at = 0x20; at < 0x30; at += 4)
            write(at, read(at));
    } // 414BA7..414BBA, DWORD copies including W
    std::uint32_t up_first() const
    {
        return read(0x30);
    } // 414BC3 before strength store
    void copy_up(std::uint32_t first)
    {
        write(0x30, first);
        write(0x34, read(0x34));
        const auto z = read(0x38), w = read(0x3c);
        write(0x38, z);
        write(0x3c, w); // 414BD8..414BE1
    }
    void flag(std::uint16_t value)
    {
        destination.flags().set(value);
    }
    void alpha(float strength)
    {
        const auto count = destination.attribute_count().value(); // 414C01, low WORD only
        if (count == 0)
            return;                                   // neither conversion nor pointer34 access
        const auto value = truncated_alpha(strength); // 414C09 before pointer34
        for (unsigned i = 0; i < count; ++i)
        {
            destination.attribute_byte(i, 2).set(value);
            destination.attribute_byte(i, 3).set(0x60);
        }
    }
};
struct SourceBrakeProvider
{
    SourcePmdModelView model;
    void clear(unsigned raw)
    {
        model.part(model.source_lod_index(raw), 6).flags().set(0);
    }
    unsigned selected_lod() const
    {
        return model.source_lod_index(model.current_detail_level().value());
    }
    SourceBrakeWheel wheel(unsigned lod, unsigned i)
    {
        const auto destination = model.part(lod, 6 + i);
        const auto out = destination.transform().location().offset; // 414B94 destination before414B9A source
        const auto in = model.part(lod, 1 + i).transform().location().offset;
        return {model.bytes(), destination, out, in};
    }
};
// One existing414B30 arithmetic path. Providers change only model access.
template <class Provider> void brake_effects(VehicleState &s, Provider model)
{
    for (unsigned lod = 0; lod < 3; ++lod)
        model.clear(lod);
    const auto lod = model.selected_lod();
    for (unsigned i = 0; i < 4; ++i)
    {
        auto &strength = s.wheels[i].braking_signal;
        if (!(strength < 0 || strength > 0))
            continue;
        auto part = model.wheel(lod, i);
        const double decayed = double(strength) - 2.0;
        part.copy_forward();
        const auto up = part.up_first();
        strength = vehicle_f32(decayed);
        part.copy_up(up);
        if (decayed <= 0)
        {
            strength = 0;
            part.flag(0);
        }
        else
            part.flag(1);
        part.alpha(strength);
    }
}
} // namespace
void update_vehicle_brake_effects(VehicleState &s, VehicleModelEffectResource &model)
{
    brake_effects(s, LegacyBrakeProvider{model});
}
void update_vehicle_brake_effects(VehicleState &s, SourcePmdModelView model)
{
    brake_effects(s, SourceBrakeProvider{model});
}
void update_vehicle_brake_effects(VehicleState &s, ModelResources &models, std::size_t handle)
{
    if (models.source_backed(handle))
        update_vehicle_brake_effects(s, models.source_at(handle));
    else
    {
        VehicleModelEffectResourceView model(models.at(handle), models.current_detail_level(handle));
        update_vehicle_brake_effects(s, model);
    }
}
void animate_vehicle_effect_coordinates(VehicleModelEffectResource &model, unsigned index)
{
    for (auto &coordinate : model.coordinates(0, index))
    {
        const float advanced =
            vehicle_f32(double(std::bit_cast<float>(coordinate[1])) + std::bit_cast<float>(std::uint32_t{0x3ca3d70a}));
        const float value = advanced > 1 ? vehicle_f32(double(advanced) - 1.0) : advanced;
        coordinate[1] = std::bit_cast<std::uint32_t>(value);
    }
}
namespace
{
contact::Vector4 launch_position(const VehicleState &s, const VehicleMatrix &m, Vec3 p)
{ // 416420
    const double transformed_x = (double(p.z) * m[8] + double(p.y) * m[4]) + double(p.x) * m[0];
    const float y = vehicle_f32((double(p.z) * m[9] + double(p.y) * m[5]) + double(p.x) * m[1]);
    const float z = vehicle_f32((double(p.z) * m[10] + double(p.y) * m[6]) + double(p.x) * m[2]);
    const float sx = vehicle_f32(-transformed_x * s.basis_x.x), sy = vehicle_f32(-transformed_x * s.basis_x.y);
    const double sz = -transformed_x * s.basis_x.z;
    const double lift = (70.0 - double(y)) - s.body_heave;
    const double ux = lift * s.up.x;
    const float uy = vehicle_f32(lift * s.up.y), uz = vehicle_f32(lift * s.up.z);
    const float fx = vehicle_f32(double(z) * s.basis_z.x), fy = vehicle_f32(double(z) * s.basis_z.y),
                fz = vehicle_f32(double(z) * s.basis_z.z);
    const float xy = vehicle_f32(double(sx) + ux), yy = vehicle_f32(double(sy) + uy);
    const float x = vehicle_f32(double(fx) + xy), y_total = vehicle_f32(double(fy) + yy);
    const double z_total = (sz + uz) + fz;
    return {vehicle_f32(double(s.position.x) - x), vehicle_f32(double(s.position.y) - y_total),
            vehicle_f32(double(s.position.z) - z_total), 0};
}
void launch_particle(contact::effects::ImpactParticles &bank, VehicleRandom &random, contact::Vector4 position,
                     const VehicleState &s)
{ // 416590
    if (bank.cursor >= 32)
        bank.cursor = 0;
    auto &p = bank.particles[bank.cursor];
    // 416420 never writes localW. Retain the destination particle lane, including
    //  on slot reuse; compare all definedXYZ and the copied physical velocityW.
    p.position = {position.x, position.y, position.z, p.position.opaque};
    p.type = 8;
    random.state = random.state * 0x343fdu + 0x269ec3u;
    const auto sample = static_cast<std::int32_t>((random.state >> 16) & 0x7fff) - 0x3fff;
    p.lifetime = (sample * 15) / 32767 + 30;
    p.decay = std::bit_cast<float>(std::uint32_t{0x3f733333});
    p.alpha = .25f;
    p.initial_lifetime = p.lifetime;
    p.field_430 = 0;
    p.field_434 = 0;
    p.scale = 2.5f;
    p.velocity = {s.velocity.x, s.velocity.y, s.velocity.z, std::bit_cast<std::uint32_t>(s.velocity_w)};
    ++bank.cursor;
    ++bank.emitted;
}
} // namespace
namespace
{
struct LegacyLaunchProvider
{
    VehicleModelEffectResource &model;
    std::uint32_t selected_head() const
    {
        return model.selected_lod();
    }
    Vec3 point(std::uint32_t lod, unsigned i) const
    {
        return model.part(lod, i).transform.position;
    }
    void animate(unsigned i) const
    {
        animate_vehicle_effect_coordinates(model, i);
    }
    void checkpoint(std::uint32_t, std::uint32_t) const noexcept {}
    void append(VehicleTrailBindings b, VehicleVector p, std::span<VehicleVector> ring, std::uint32_t &cursor,
                std::uint32_t &count) const
    {
        append_vehicle_trail_point(b, p, ring, cursor, count, {7, false});
    }
};
template <class Binding, class Provider>
void launch_effect(Binding b, VehicleModelEffects &effects, const VehicleAnchorState &anchor, Provider provider,
                   contact::effects::ImpactParticles &particles, VehicleRandom &random)
{
    if (anchor.effect_available[0] != 1)
        return;
    auto &s = b.vehicle;
    if ((s.turbo_rpm == 0 || std::isnan(s.turbo_rpm)) && s.launch_effect_level == 0)
    {
        effects.enabled = 0;
        return;
    }
    effects.enabled = 1;
    provider.checkpoint(0x416848, 1);
    if (b.source_tick >= 90)
        for (unsigned i = 0; i < 2; ++i)
            if (anchor.effect_available[i] == 1)
            {
                auto point = anchor.effect_position[i];
                point.z = vehicle_f32(double(point.z) + 25.0);
                provider.append(b, point, effects.trails[i], effects.cursor[i], s.camera_effects.values[i]);
            }
    const auto lod = provider.selected_head();
    if (s.launch_effect_timer != 0)
    {
        --s.launch_effect_timer;
        provider.checkpoint(0x416677, s.launch_effect_timer);
        if ((s.launch_effect_timer & 1u) == 0 && anchor.effect_available[0] == 1)
        {
            launch_particle(particles, random, launch_position(s, b.model_basis, provider.point(lod, 19)), s);
            provider.checkpoint(0x4166a6, particles.emitted);
            if (anchor.effect_available[1] == 1)
            {
                launch_particle(particles, random, launch_position(s, b.model_basis, provider.point(lod, 20)), s);
                provider.checkpoint(0x4166d7, particles.emitted);
            }
        }
    }
    const float decay = std::bit_cast<float>(std::uint32_t{0x3ca3d70a});
    if (s.launch_effect_strength > decay)
        s.launch_effect_strength = vehicle_f32(double(s.launch_effect_strength) - decay);
    s.launch_effect_level =
        as_signed(std::bit_cast<std::uint32_t>(s.launch_effect_level) + (s.turbo_rpm > 10 ? 8u : 0xfffffffeu));
    if (s.launch_effect_level > 255)
        s.launch_effect_level = 255;
    else if (s.launch_effect_level < 0)
        s.launch_effect_level = 0;
    effects.phase = (effects.phase + 512u) & 8191u;
    const double raw_intensity = double(s.launch_effect_level) * std::bit_cast<float>(std::uint32_t{0x3b808081});
    const float intensity = raw_intensity > 1 ? 1 : vehicle_f32(raw_intensity);
    const double wave = double(intensity) * vehicle_tables::sine(effects.phase);
    const float wobble_xy = vehicle_f32(wave * std::bit_cast<float>(std::uint32_t{0x3dcccccd})),
                wobble_z = vehicle_f32(wave * .25);
    auto scale = [&](unsigned i) {
        auto &out = effects.current_scale[i];
        const auto base = anchor.effect_base_scale[i];
        out.x = vehicle_f32((double(intensity) * base.x + s.launch_effect_strength) + wobble_xy);
        out.y = vehicle_f32((double(intensity) * base.y + s.launch_effect_strength) + wobble_xy);
        const double z = double(intensity) * base.z;
        out.z = vehicle_f32(((z + z) + s.launch_effect_strength) + wobble_z);
        provider.checkpoint(i ? 0x4169d0u : 0x416968u, 19 + i);
        provider.animate(19 + i);
    };
    scale(0);
    if (anchor.effect_available[1] != 0)
        scale(1);
}
} // namespace
void update_vehicle_launch_effect(VehicleTrailBindings b, VehicleModelEffects &effects,
                                  const VehicleAnchorState &anchor, VehicleModelEffectResource &model,
                                  contact::effects::ImpactParticles &particles, VehicleRandom &random)
{
    launch_effect(b, effects, anchor, LegacyLaunchProvider{model}, particles, random);
}
void detail::run_launch(VehicleLaunchBindings b, VehicleModelEffects &effects, const VehicleAnchorState &anchor,
                        detail::LaunchProvider provider, contact::effects::ImpactParticles &particles,
                        VehicleRandom &random)
{
    launch_effect(b, effects, anchor, provider, particles, random);
}
VehiclePresentationEffects vehicle_presentation_effects(const VehicleModelEffects &effects,
                                                        const VehicleAnchorState &anchors)
{
    return {effects.enabled,
            anchors.effect_available[0],
            anchors.effect_available[1],
            {effects.current_scale[0].x, effects.current_scale[0].y, effects.current_scale[0].z},
            {effects.current_scale[1].x, effects.current_scale[1].y, effects.current_scale[1].z}};
}
} // namespace sgn::retail
