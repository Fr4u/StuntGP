#include "sgn/retail_track_geometry_animation.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <cstring>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_float;
float round32(double value)
{
    return static_cast<float>(value);
}
bool finite(const Vec3 &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
std::uint32_t truncate_word(double value)
{ // 462860, EAX of the signed64 result
    constexpr double limit = 9223372036854775808.0;
    if (!std::isfinite(value) || value < -limit || value >= limit)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
void normal_xyz(VehicleVector &destination, VehicleVector value)
{
    value = vehicle_normalize(value);
    destination.x = value.x;
    destination.y = value.y;
    destination.z = value.z;
}
void normal_index(std::array<std::uint8_t, 8> &polygon, std::uint32_t index)
{
    polygon[4] = static_cast<std::uint8_t>(index);
    polygon[5] = static_cast<std::uint8_t>(index >> 8);
}
void coordinate_normal(std::array<std::uint32_t, 4> &coordinate, std::uint32_t index)
{
    coordinate[2] = (coordinate[2] & 0xffffu) | (index << 16);
}
void rebuild_normals(TrackGeometryAnimationPart p, TrackNormalView view)
{ // 426370
    const auto count = static_cast<std::uint32_t>(p.polygons.size()), half = count / 2;
    const auto base = *view.allocation.cursor;
    for (std::uint32_t i = 0; i < half; ++i)
    {
        const auto &a = p.vertices[i * 2];
        const auto &b = p.vertices[i * 2 + 1];
        const auto &c = p.vertices[i * 2 + 2];
        const VehicleVector edge{round32(double(b.x) - a.x), round32(double(b.y) - a.y), round32(double(b.z) - a.z), 0};
        const double x = double(a.x) - c.x, y = double(a.y) - c.y, z = double(a.z) - c.z;
        VehicleVector cross{round32(y * edge.z - z * edge.y), round32(z * edge.x - x * edge.z),
                            round32(x * edge.y - y * edge.x), 0};
        normal_xyz(view.normals[base + i], cross);
        const auto &n = view.normals[base + i];
        auto &back = view.normals[base + half + i];
        back.x = -n.x;
        back.y = -n.y;
        back.z = -n.z;
        normal_index(p.polygons[i], base + i);
        normal_index(p.polygons[half + i], base + half + i);
    }
    const auto coordinate_back = p.vertices.size() / 2 + 2;
    for (std::uint32_t i = 0; i + 1 < half; ++i)
    {
        const auto &a = view.normals[base + i];
        const auto &b = view.normals[base + i + 1];
        auto &n = view.normals[base + count + i];
        normal_xyz(n, {round32(double(a.x) + b.x), round32(double(a.y) + b.y), round32(double(a.z) + b.z), 0});
        auto &back = view.normals[base + count + half + i];
        back.x = -n.x;
        back.y = -n.y;
        back.z = -n.z;
        coordinate_normal(p.coordinates[2 + i * 2], base + count + i);
        coordinate_normal(p.coordinates[3 + i * 2], base + count + i);
        coordinate_normal(p.coordinates[coordinate_back + i * 2], base + count + half + i);
        coordinate_normal(p.coordinates[coordinate_back + 1 + i * 2], base + count + half + i);
    }
    // Source advances by3*polygon_count-2, including its untouched reserve.
    view.allocation.cursor = base + count * 3 - 2;
}
void deform(const TrackPartAnimation &animation, TrackGeometryAnimationPart p, TrackNormalView normals)
{ // 426610/4266E0
    const auto kind = animation.kind;
    const auto half = static_cast<std::uint32_t>(p.polygons.size() / 2);
    const auto count = half + (kind == 2 ? 1u : 0u), stride = kind == 1 ? 64u : 32u;
    auto phase = animation.phase_u;
    const auto back = p.vertices.size() / 2;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        double amplitude = 256;
        if (kind == 1)
        {
            if (i * 64 < 256)
                amplitude = i * 64;
            else if (i >= half - 4)
                amplitude = (half - i) * 64;
        }
        else if (kind == 2)
        {
            if (i * 32 < 256)
                amplitude = i * 32;
        }
        else if (i >= count - 8)
            amplitude = (count - i) * 32;
        const auto height = round32(amplitude * vehicle_tables::sine(phase));
        p.vertices[i * 2].z = height;
        p.vertices[i * 2 + 1].z = height;
        p.vertices[back + i * 2].z = height;
        p.vertices[back + i * 2 + 1].z = height;
        phase = (phase + (stride == 64 ? 512u : 1024u)) & 8191;
    }
    rebuild_normals(p, normals);
}
void rotate(const TrackPartAnimation &a, ModelTransform &pose, TrackGeometryAnimationAudit audit)
{ // 4267D0
    const CameraVector angles{a.angles.x, a.angles.y, a.angles.z, a.angles.w};
    const CameraVector parameters{a.parameters.x, a.parameters.y, a.parameters.z, a.parameters.w};
    auto forward = camera_rotate_degrees(angles, {0, 0, 1, 0}, audit.rotation_scratch);
    auto up = camera_rotate_degrees(angles, {0, 1, 0, 0}, audit.rotation_scratch);
    forward = camera_rotate_degrees(parameters, forward, audit.rotation_scratch);
    up = camera_rotate_degrees(parameters, up, audit.rotation_scratch);
    pose.forward = {forward.x, forward.y, forward.z};
    pose.up = {up.x, up.y, up.z};
}
void bob(const TrackPartAnimation &a, ModelTransform &pose, bool dual)
{ // 426880/4268F0
    const auto phase = dual ? a.phase_v : a.phase_u;
    const double height = double(vehicle_tables::sine(a.phase_u)) * (dual ? 128 : 32) - 100;
    pose.position.y = round32(dual ? height + double(vehicle_tables::sine(a.phase_v)) * 128 : height);
    const auto n = vehicle_normalize({0, vehicle_tables::sine((phase * 2) & 8191), 128, 0});
    pose.forward = {n.x, n.y, n.z};
}
void emit(TrackPartAnimation &a, TrackGeometryAnimationPart part, TrackGeometryAnimationEffects effects,
          TrackGeometryAnimationAudit audit)
{ // 426A00/426970
    if (part.flags_02 & 0x10)
        return;
    ++a.retained_40;
    if (effects.source_rate <= 80 && a.retained_40 < 2)
        return;
    a.retained_40 = 0;
    const auto index = [](float angle) {
        return std::bit_cast<std::int32_t>(truncate_word(double(angle) * as_float(0x41b60b61)));
    };
    const auto m = camera_rotation(index(a.angles.x), index(a.angles.y), index(a.angles.z));
    contact::effects::EffectParticle p;
    static_assert(sizeof(ModelTransform) == 80);
    p.position.opaque = effects.template_pose.position_w;
    std::memcpy(p.frame_payload.data(), reinterpret_cast<const std::byte *>(&effects.template_pose) + 16, 64);
    const auto pivot = *part.pivot;
    const double x = -double(pivot.x), y = -double(pivot.y), z = -(double(pivot.z) + 2000);
    p.position.x = round32((double(m[4]) * y + double(m[0]) * x) + double(m[8]) * z);
    p.position.y = round32((double(m[5]) * y + double(m[1]) * x) + double(m[9]) * z);
    p.position.z = round32((double(m[6]) * y + double(m[2]) * x) + double(m[10]) * z);
    p.position.x = round32(double(round32(double(p.position.x) + part.transform->position.x)) + pivot.x);
    p.position.y = round32(double(round32(double(p.position.y) + part.transform->position.y)) + pivot.y);
    p.position.z = round32((double(p.position.z) + part.transform->position.z) + pivot.z);
    p.position.y = round32(double(p.position.y) - 400);
    p.flags = 7;
    p.field_68 = as_float(0x3c23d70a);
    p.gravity_scale = round32(double(effects.gravity.inverse_factor) * as_float(0xbd4ccccd));
    p.lifetime_mode = 2;
    p.render_enabled = 1;
    p.animation_binding = 0;
    p.base_scale = 10;
    p.scale_fade_in = 3;
    p.scale_fade_out = 12;
    p.base_alpha = 128;
    p.alpha_fade_in = 0;
    p.alpha_fade_out = 18;
    p.base_color = 0x406090;
    p.texture = 7;
    p.render_flags = 0x60;
    p.lifetime = truncate_word(std::abs(effects.random.signed_value(18)) + 45);
    const double velocity = effects.random.signed_value(100);
    double offset = velocity * 8;
    if (std::abs(offset) < 800)
        offset = offset < 0 ? -800 : 800;
    p.position.x = round32(double(p.position.x) + round32(offset * m[0]));
    p.position.y = round32(double(p.position.y) + round32(offset * m[1]));
    p.position.z = round32(double(p.position.z) + offset * m[2]);
    p.velocity.x = round32(velocity * m[0]);
    p.velocity.z = round32(velocity * m[2]);
    p.velocity.y = round32(std::abs(effects.random.signed_value(50)));
    p.angular_velocity = round32(effects.random.signed_value(22.5f));
    p.source_undefined = {true, true, true, false, true, true, true};
    contact::effects::allocate_effect_particle(p, effects.particles, {}, audit.particle_frames);
}
void color(const TrackPartAnimation &a, TrackGeometryAnimationPart part, std::uint32_t intensity)
{ // 426CF0
    std::uint32_t rgb = 0;
    for (unsigned shift = 0; shift < 24; shift += 8)
        rgb |= (((((a.parameter_44 >> shift) & 255u) * intensity) >> 8) & 255u) << shift;
    for (auto &coordinate : part.coordinates)
        coordinate[3] = (coordinate[3] & 0xff000000u) | rgb;
}
void orient_forward(ModelTransform &pose, float x, float y, float z)
{
    const auto n = vehicle_normalize({x, y, z, 0});
    pose.forward = {n.x, n.y, n.z};
}
void orient_up(ModelTransform &pose, float x, float y, float z)
{
    const auto n = vehicle_normalize({x, y, z, 0});
    pose.up = {n.x, n.y, n.z};
}
void validate(std::span<TrackGeometryAnimationPart> parts, std::span<TrackPartAnimation> records,
              TrackNormalView normals, TrackGeometryAnimationEffects effects)
{
    if (parts.size() > 32 || records.size() < parts.size() ||
        normals.allocation.first_generated > normals.normals.size())
        throw FormatError("track animation owner extent invalid");
    std::size_t generated = normals.allocation.first_generated;
    for (std::size_t i = 1; i < parts.size(); ++i)
    {
        const auto &a = records[i];
        const auto &p = parts[i];
        if (a.kind == 0 || a.kind >= 17 || a.kind == 15)
            continue;
        if (!p.transform || !p.pivot)
            throw FormatError("track animation missing canonical pose");
        if (a.kind != 4 && (a.phase_u >= 8192 || (a.kind == 16 && a.phase_v >= 8192)))
            throw FormatError("track animation phase outside source sine table");
        if (a.kind >= 1 && a.kind <= 3)
        {
            const auto count = p.polygons.size(), half = count / 2, rows = half + (a.kind == 2 ? 1u : 0u);
            if (count < 2 || count > 65535 || (count & 1) || p.vertices.size() > 65535 || (p.vertices.size() & 1) ||
                p.vertices.size() < rows * 4 || p.vertices.size() < count + 2 ||
                p.coordinates.size() < p.vertices.size())
                throw FormatError("track deformation outside authored owner extents");
            const auto needed = count * 3 - 2;
            if (needed > normals.normals.size() - generated)
                throw FormatError("track generated normals leave reserved owner");
            generated += needed;
            for (const auto &v : p.vertices)
                if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
                    throw FormatError("track deformation nonfinite geometry");
        }
        if (a.kind == 4 || a.kind == 16)
        {
            for (const auto &vector : {a.angles, a.parameters})
                for (float angle : {vector.x, vector.y, vector.z})
                {
                    const double converted = double(angle) * as_float(0x41b60b61);
                    if (!std::isfinite(converted) || converted < -2147483648.0 || converted > 2147483647.0)
                        throw FormatError("track rotation outside finite camera math domain");
                }
        }
        if (a.kind == 16 && (!(p.flags_02 & 0x10)) &&
            (!finite(*p.pivot) || !finite(p.transform->position) || !std::isfinite(effects.gravity.inverse_factor)))
            throw FormatError("track emitter nonfinite pose/gravity");
    }
}
} // namespace
void animate_track_geometry(std::span<TrackGeometryAnimationPart> parts, std::span<TrackPartAnimation> records,
                            TrackNormalView normals, TrackGeometryAnimationEffects effects,
                            TrackGeometryAnimationAudit audit)
{
    validate(parts, records, normals, effects);
    normals.allocation.cursor = normals.allocation.first_generated;
    for (std::size_t i = 1; i < parts.size(); ++i)
    {
        auto &a = records[i];
        auto p = parts[i];
        if (a.kind == 0 || a.kind >= 17 || a.kind == 15)
            continue;
        auto &pose = *p.transform;
        if (a.kind <= 3)
        {
            deform(a, p, normals);
            continue;
        }
        if (a.kind == 4)
        {
            rotate(a, pose, audit);
            continue;
        }
        const float sine = vehicle_tables::sine(a.phase_u), twice = vehicle_tables::sine((a.phase_u * 2) & 8191);
        switch (a.kind)
        {
        case 5:
            bob(a, pose, false);
            break;
        case 6:
        case 7:
            orient_forward(pose, sine, 0, 16);
            break;
        case 8:
            color(a, p, truncate_word(double(sine) * 96 + 159));
            break;
        case 9:
            color(a, p, truncate_word(double(sine) * 128));
            break;
        case 10:
            pose.scale.x = pose.scale.z = round32(double(sine) * as_float(0x3c23d70a) + 1);
            pose.scale.y = round32(double(twice) * as_float(0x3c75c28f) + 1);
            break;
        case 11:
            pose.scale.x = pose.scale.z = round32(double(sine) * as_float(0x3ba3d70a) + 1);
            pose.scale.y = round32(double(twice) * as_float(0x3c23d70a) + 1);
            break;
        case 12:
        case 13:
            orient_forward(pose, sine, 0, 2);
            orient_up(pose, sine, 16, twice);
            pose.scale.x = round32(double(sine) * as_float(0x3c23d70a) + 1);
            pose.scale.y = round32(double(twice) * as_float(0x3ca3d70a) + 1);
            pose.scale.z = round32(double(twice) * as_float(0x3c23d70a) + 1);
            break;
        case 14:
            orient_up(pose, sine, 64, 0);
            orient_forward(pose, sine, 0, 128);
            break;
        case 16:
            bob(a, pose, true);
            rotate(a, pose, audit);
            emit(a, p, effects, audit);
            break;
        default:
            break;
        }
    }
}
void animate_track_geometry(ModelResource &model, std::span<TrackPartAnimation> records, TrackNormalView normals,
                            TrackGeometryAnimationEffects effects, TrackGeometryAnimationAudit audit)
{
    if (normals.normals.data() != model.source_normals().data() ||
        normals.normals.size() != model.source_normals().size())
        throw FormatError("track animation requires canonical normal owner");
    const auto lod = model.source_lod_index(0);
    const auto count = model.part_count(lod);
    if (count > 32)
        throw FormatError("track animation exceeds original32 records");
    std::array<TrackGeometryAnimationPart, 32> parts{};
    for (unsigned i = 0; i < count; ++i)
    {
        const auto binding = model.part(lod, i);
        parts[i] = {&binding.transform,
                    &binding.pivot,
                    model.source_part_flags_02(lod, i),
                    model.mutable_source_vertices(lod, i),
                    model.source_coordinates(lod, i),
                    model.source_attributes(lod, i)};
    }
    animate_track_geometry(std::span{parts}.first(count), records, normals, effects, audit);
}
} // namespace sgn::retail
