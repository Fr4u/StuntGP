#include "sgn/source_track_geometry_animation.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_bank_read_memo.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using Location = SourcePmdLocation;
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
VehicleVector normalized(VehicleVector v)
{ // 44ED40: unordered FCOM follows zero branch
    const double squared = (double(v.x) * v.x + double(v.y) * v.y) + double(v.z) * v.z;
    if (squared == 0 || std::isnan(squared))
        return {0, 0, 0, v.w};
    const double inverse = 1 / std::sqrt(squared);
    return {f32(inverse * v.x), f32(inverse * v.y), f32(inverse * v.z), v.w};
}
Location add(Location base, std::uint32_t offset, std::uint32_t site)
{
    if (offset > std::numeric_limits<std::uint32_t>::max() - base.offset)
        throw SourceAllocationError(site, "track animation pointer arithmetic wraps its allocation");
    base.offset += offset;
    return base;
}
// NEW host performance (perf_world R19): `views` is the 426ED0 invocation's
// typed view of the actual allocations (sgn/source_bank_read_memo.hpp). It
// returns exactly bank.bytes(allocation): this synchronous animation only
// stores raw payload bytes and never allocates, releases or stores opaque
// transport words in the bank (effect particles live in their own pool).
struct Memory
{
    SourceAllocationBank &bank;
    SourceBankReadMemo *views;
    template <class T> T read(Location base, std::uint32_t offset, std::uint32_t site) const
    {
        const auto at = add(base, offset, site);
        const auto bytes = views->whole(at.allocation);
        if (at.offset > bytes.size() || sizeof(T) > bytes.size() - at.offset)
            throw SourceAllocationError(site, "reached track animation read leaves actual owner");
        T value;
        std::memcpy(&value, bytes.data() + at.offset, sizeof(T));
        return value;
    }
    template <class T> void write(Location base, std::uint32_t offset, T value, std::uint32_t site) const
    {
        const auto at = add(base, offset, site);
        auto bytes = views->whole(at.allocation);
        if (at.offset > bytes.size() || sizeof(T) > bytes.size() - at.offset)
            throw SourceAllocationError(site, "reached track animation store leaves actual owner");
        std::memcpy(bytes.data() + at.offset, &value, sizeof(T));
    }
    Location pointer(Location record, std::uint32_t offset, std::uint32_t site) const
    {
        const auto value = read<std::uint32_t>(record, offset, site);
        // Relocated PMD pointers are allocation offsets, not bank tokens.
        return value ? Location{record.allocation, value} : Location{};
    }
    void xyz(Location destination, VehicleVector value, std::uint32_t site) const
    {
        write(destination, 0, value.x, site);
        write(destination, 4, value.y, site);
        write(destination, 8, value.z, site);
        // Source stack W was never initialized. Existing explicit host policy:
        // retain the actual destination lane; do not read or invent a W value.
    }
    VehicleVector xyz(Location source, std::uint32_t site) const
    {
        const auto x = read<float>(source, 0, site);
        const auto y = read<float>(source, 4, site);
        const auto z = read<float>(source, 8, site);
        return {x, y, z, 0};
    }
    void normalize(Location destination, std::uint32_t site) const
    {
        // 44ED40 reads Z/Y/X first; its zero/unordered arm stores Z/Y/X.
        const auto z = read<float>(destination, 8, site), y = read<float>(destination, 4, site),
                   x = read<float>(destination, 0, site);
        const double squared = (double(x) * x + double(y) * y) + double(z) * z;
        if (squared == 0 || std::isnan(squared))
        {
            write(destination, 8, 0.0f, site);
            write(destination, 4, 0.0f, site);
            write(destination, 0, 0.0f, site);
        }
        else
            xyz(destination, normalized({x, y, z, 0}), site);
    }
};
float sine(std::uint32_t index, std::uint32_t site)
{
    // Initial phases are NOT masked by these leaves. The10240-word retail
    // table includes its cosine tail; only the source's later phases wrap.
    if (index >= vehicle_tables::sine_bits.size())
        throw SourceAllocationError(site, "reached phase leaves the pinned source sine table");
    return as_float(vehicle_tables::sine_bits[index]);
}
std::uint32_t ftol(double value)
{ // 462860 EAX, signed64 truncation/indefinite
    constexpr double limit = 9223372036854775808.0;
    if (!std::isfinite(value) || value < -limit || value >= limit)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
}
std::uint32_t normal_index(Location value, Location base, std::uint32_t site)
{
    if (value.allocation != base.allocation)
        throw SourceAllocationError(site, "normal index subtraction crosses allocation identities");
    const auto delta = value.offset - base.offset; // source32-bit SUB then signed SAR
    return std::bit_cast<std::uint32_t>(std::bit_cast<std::int32_t>(delta) >> 4);
}
void normals_426370(Memory m, const SourcePmdLoadingState &globals, Location part, Location &cursor)
{
    const auto base = cursor; // 426377, captured before all part fields
    const auto headers = m.pointer(part, 0x34, 0x42637e);
    const std::uint32_t count = m.read<std::uint16_t>(part, 0x24, 0x426384);
    const auto vertices = m.pointer(part, 0x30, 0x42638b);
    const auto half = count >> 1;
    const auto smoothed = add(base, count * 16u, 0x42639a);
    for (std::uint32_t i = 0; i < half; ++i)
    {
        const auto normal = add(base, i * 16u, 0x4263e3);
        const auto normal_base = globals.track.normals; // 4263D2, fresh each iteration
        const auto a = add(vertices, i * 32u, 0x4263d8), b = add(a, 16, 0x4263d8), c = add(a, 32, 0x426405);
        const double bx = m.read<float>(b, 0, 0x4263d8), ax = m.read<float>(a, 0, 0x4263db);
        m.write(headers, i * 8u + 4u, static_cast<std::uint16_t>(normal_index(normal, normal_base, 0x4263e5)),
                0x4263e8);
        const float edge_x = f32(bx - ax);
        const double by = m.read<float>(b, 4, 0x4263f1), ay = m.read<float>(a, 4, 0x4263f3);
        const float edge_y = f32(by - ay);
        const double bz = m.read<float>(b, 8, 0x4263f9), az = m.read<float>(a, 8, 0x4263fc);
        const float edge_z = f32(bz - az);
        const double ax2 = m.read<float>(a, 0, 0x426402), cx = m.read<float>(c, 0, 0x426405);
        const double ay2 = m.read<float>(a, 4, 0x426408), cy = m.read<float>(c, 4, 0x42640b);
        const double az2 = m.read<float>(a, 8, 0x42640e), cz = m.read<float>(c, 8, 0x426411);
        const double x = ax2 - cx, y = ay2 - cy, z = az2 - cz;
        const auto n =
            normalized({f32(y * edge_z - z * edge_y), f32(z * edge_x - x * edge_z), f32(x * edge_y - y * edge_x), 0});
        m.xyz(normal, n, 0x42644f);
        const auto back = add(base, (half + i) * 16u, 0x42647b);
        m.write(headers, (half + i) * 8u + 4u,
                static_cast<std::uint16_t>(normal_index(back, globals.track.normals, 0x426475)), 0x426478);
        m.xyz(back, {-n.x, -n.y, -n.z, 0}, 0x426499);
    }
    const auto coordinates = m.pointer(part, 0x2c, 0x4264e7);
    const auto front_index = normal_index(smoothed, globals.track.normals, 0x4264f5);
    const auto back_index = front_index + half;
    const std::uint32_t vertex_count = m.read<std::uint16_t>(part, 0x28, 0x426502);
    const auto back_coordinate = (vertex_count >> 1) + 2u;
    // Intentional unsigned half-1. Malformed count0/1 reaches the source's
    // reads and then the bank bounds guard; it is not silently made a no-op.
    for (std::uint32_t i = 0; i < half - 1u; ++i)
    {
        const auto a = add(base, i * 16u, 0x426534), b = add(a, 16u, 0x426537);
        const double ax = m.read<float>(a, 0, 0x426534), bx = m.read<float>(b, 0, 0x426537);
        const auto x = f32(ax + bx);
        const double ay = m.read<float>(a, 4, 0x426540), by = m.read<float>(b, 4, 0x426543);
        const auto y = f32(ay + by);
        const double az = m.read<float>(a, 8, 0x426548), bz = m.read<float>(b, 8, 0x42654b);
        const auto n = normalized({x, y, f32(az + bz), 0});
        m.xyz(add(smoothed, i * 16u, 0x42655c), n, 0x426567);
        m.xyz(add(smoothed, (half + i) * 16u, 0x426587), {-n.x, -n.y, -n.z, 0}, 0x4265a8);
        m.write(coordinates, (2u + i * 2u) * 16u + 10u, static_cast<std::uint16_t>(front_index + i), 0x4265c5);
        m.write(coordinates, (back_coordinate + i * 2u) * 16u + 10u, static_cast<std::uint16_t>(back_index + i),
                0x4265cb);
        m.write(coordinates, (3u + i * 2u) * 16u + 10u, static_cast<std::uint16_t>(front_index + i), 0x4265cf);
        m.write(coordinates, (back_coordinate + 1u + i * 2u) * 16u + 10u, static_cast<std::uint16_t>(back_index + i),
                0x4265d5);
    }
    // 4265F4 reloads the global cursor. It is not the local advanced normal.
    const std::int64_t delta = std::int64_t(count) * 48 - 32;
    if (delta < 0)
    {
        if (cursor.offset < static_cast<std::uint64_t>(-delta))
            throw SourceAllocationError(0x426602, "normal cursor subtraction leaves owner");
        cursor.offset -= static_cast<std::uint32_t>(-delta);
    }
    else
        cursor = add(cursor, static_cast<std::uint32_t>(delta), 0x426607);
}
void deform(Memory m, const SourcePmdLoadingState &globals, TrackPartAnimation &animation, Location part,
            Location &cursor, std::uint32_t kind)
{
    auto phase = animation.phase_u;
    Location vertices;
    std::uint32_t count;
    if (kind == 1)
    {
        vertices = m.pointer(part, 0x30, 0x426622);
        count = m.read<std::uint16_t>(part, 0x24, 0x426627);
    }
    else
    {
        count = m.read<std::uint16_t>(part, 0x24, 0x4266f7);
        vertices = m.pointer(part, 0x30, 0x4266fb);
    }
    const std::uint32_t vertex_count = m.read<std::uint16_t>(part, 0x28, kind == 1 ? 0x42662du : 0x426700u);
    const auto rows = (count >> 1) + (kind == 2 ? 1u : 0u), back = vertex_count >> 1;
    const auto step = kind == 1 ? 64u : 32u;
    for (std::uint32_t i = 0, up = 0, down = rows * step; i < rows; ++i, up += step, down -= step)
    {
        double amplitude = 256;
        if (kind == 1)
        {
            if (up < 256)
                amplitude = up;
            else if (i >= rows - 4u)
                amplitude = down;
        }
        else if (kind == 2)
        {
            if (up < 256)
                amplitude = up;
        }
        else if (i >= rows - 8u)
            amplitude = down;
        const auto value = f32(amplitude * sine(phase, kind == 1 ? 0x426688u : 0x42676au));
        phase = (phase + (kind == 1 ? 512u : 1024u)) & 8191u;
        const auto site = kind == 1 ? 0x4266a7u : 0x426789u;
        m.write(vertices, i * 32u + 8u, value, site);
        m.write(vertices, (back + i * 2u) * 16u + 8u, value, site + 3u);
        m.write(vertices, i * 32u + 24u, value, site + 6u);
        m.write(vertices, (back + i * 2u + 1u) * 16u + 8u, value, site + 8u);
    }
    normals_426370(m, globals, part, cursor);
}
void rotate_4267d0(Memory m, const TrackPartAnimation &a, Location part, TrackGeometryAnimationAudit audit)
{
    const auto rotate = [&](const VehicleVector &angles, CameraVector value) {
        // 44EC00 converts Z/Y/X via462860 before publishing61C3A4.
        const auto iz = std::bit_cast<std::int32_t>(ftol(double(angles.z) * as_float(0x41b60b61)));
        const auto iy = std::bit_cast<std::int32_t>(ftol(double(angles.y) * as_float(0x41b60b61)));
        const auto ix = std::bit_cast<std::int32_t>(ftol(double(angles.x) * as_float(0x41b60b61)));
        const auto matrix = camera_rotation(ix, iy, iz);
        if (audit.rotation_scratch)
            *audit.rotation_scratch = matrix;
        return CameraVector{
            f32((double(matrix[8]) * value.z + double(matrix[4]) * value.y) + double(matrix[0]) * value.x),
            f32((double(matrix[9]) * value.z + double(matrix[5]) * value.y) + double(matrix[1]) * value.x),
            f32((double(matrix[10]) * value.z + double(matrix[6]) * value.y) + double(matrix[2]) * value.x), value.w};
    };
    auto forward = rotate(a.angles, {0, 0, 1, 0});
    auto up = rotate(a.angles, {0, 1, 0, 0});
    forward = rotate(a.parameters, forward);
    up = rotate(a.parameters, up);
    const auto pose = m.pointer(part, 8, 0x426835); // only after all four44EC00 calls
    m.xyz(add(pose, 0x20, 0x42683d), {forward.x, forward.y, forward.z, 0}, 0x426844);
    m.xyz(add(pose, 0x30, 0x426840), {up.x, up.y, up.z, 0}, 0x42685b);
}
void bob(Memory m, const TrackPartAnimation &a, Location part, bool dual)
{
    const auto phase_v = dual ? a.phase_v : 0u;
    const auto phase = a.phase_u;
    const auto pose = m.pointer(part, 8, dual ? 0x4268fdu : 0x42688au);
    const double height = double(sine(phase, dual ? 0x426900u : 0x426890u)) * (dual ? 128 : 32) - 100;
    const auto result = dual ? height + double(sine(phase_v, 0x42691f)) * 128 : height;
    m.write(pose, 4, f32(result), dual ? 0x42692eu : 0x4268aau);
    const auto y = sine(((dual ? phase_v : phase) * 2u) & 8191u, dual ? 0x426931u : 0x4268adu);
    m.xyz(add(pose, 0x20, dual ? 0x426951u : 0x4268cdu), normalized({0, y, 128, 0}), dual ? 0x426954u : 0x4268d0u);
}
void orient(Memory m, const TrackPartAnimation &a, Location part, std::uint32_t kind)
{
    const auto pose = m.pointer(part, 8, kind == 14 ? 0x426ca1u : 0x426c70u);
    const auto phase = a.phase_u;
    if (kind == 14)
    {
        const auto up = add(pose, 0x30, 0x426ca8);
        m.write(up, 0, sine(phase, 0x426cab), 0x426cb2);
        m.write(up, 4, 64.0f, 0x426cb4);
        m.write(up, 8, 0.0f, 0x426cbb);
        m.normalize(up, 0x426cc2);
    }
    const auto forward = add(pose, 0x20, kind == 14 ? 0x426cceu : 0x426c76u);
    m.write(forward, 0, sine(phase, kind == 14 ? 0x426cc7u : 0x426c7du), kind == 14 ? 0x426cd2u : 0x426c84u);
    m.write(forward, 4, 0.0f, kind == 14 ? 0x426cd4u : 0x426c86u);
    m.write(forward, 8, kind == 14 ? 128.0f : 16.0f, kind == 14 ? 0x426cdbu : 0x426c8du);
    m.normalize(forward, kind == 14 ? 0x426ce3u : 0x426c94u);
}
void color(Memory m, const TrackPartAnimation &a, Location part, bool additive)
{
    const auto phase = a.phase_u;
    const auto intensity =
        ftol(double(sine(phase, additive ? 0x426d59u : 0x426d89u)) * (additive ? 96 : 128) + (additive ? 159 : 0));
    // IMUL retains low32 bits before SHR. Byte stores preserve coordinate alpha.
    const auto red = static_cast<std::uint8_t>((((a.parameter_44 >> 16) & 255u) * intensity) >> 8);
    const auto green = static_cast<std::uint8_t>((((a.parameter_44 >> 8) & 255u) * intensity) >> 8);
    const auto blue = static_cast<std::uint8_t>(((a.parameter_44 & 255u) * intensity) >> 8);
    const auto coordinates = m.pointer(part, 0x2c, 0x426d15);
    const std::uint32_t count = m.read<std::uint16_t>(part, 0x26, 0x426d1a);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        m.write(coordinates, i * 16u + 14u, red, 0x426d2e);
        m.write(coordinates, i * 16u + 13u, green, 0x426d31);
        m.write(coordinates, i * 16u + 12u, blue, 0x426d33);
    }
}
void scale(Memory m, const TrackPartAnimation &a, Location part, bool gentle)
{
    const auto phase = a.phase_u;
    const auto pose = m.pointer(part, 8, gentle ? 0x426df3u : 0x426db3u);
    const auto x =
        f32(double(sine(phase, gentle ? 0x426df6u : 0x426db6u)) * as_float(gentle ? 0x3ba3d70au : 0x3c23d70au) + 1);
    m.write(pose, 0x10, x, gentle ? 0x426e11u : 0x426dd1u);
    m.write(pose, 0x18, x, gentle ? 0x426e14u : 0x426dd4u);
    const auto y = f32(double(sine((phase * 2u) & 8191u, gentle ? 0x426e17u : 0x426dd7u)) *
                           as_float(gentle ? 0x3c23d70au : 0x3c75c28fu) +
                       1);
    m.write(pose, 0x14, y, gentle ? 0x426e2au : 0x426deau);
}
void sway_426e30(Memory m, const TrackPartAnimation &a, Location part)
{
    const auto phase = a.phase_u;
    const auto pose = m.pointer(part, 8, 0x426e35);
    const auto initial = sine(phase, 0x426e38);
    const auto twice = (phase * 2u) & 8191u;
    const auto forward = add(pose, 0x20, 0x426e40), up = add(pose, 0x30, 0x426e6f);
    m.write(forward, 0, initial, 0x426e4c);
    m.write(forward, 4, 0.0f, 0x426e4e);
    m.write(forward, 8, 2.0f, 0x426e55);
    m.normalize(forward, 0x426e5c);
    const auto x = sine(phase, 0x426e61), z = sine(twice, 0x426e68);
    m.write(up, 0, x, 0x426e72);
    m.write(up, 4, 16.0f, 0x426e74);
    m.write(up, 8, z, 0x426e7b);
    m.normalize(up, 0x426e7e);
    m.write(pose, 0x10, f32(double(sine(phase, 0x426e83)) * as_float(0x3c23d70a) + 1), 0x426e96);
    m.write(pose, 0x14, f32(double(sine(twice, 0x426e99)) * as_float(0x3ca3d70a) + 1), 0x426eac);
    m.write(pose, 0x18, f32(double(sine(twice, 0x426eaf)) * as_float(0x3c23d70a) + 1), 0x426ec3);
}
void emit_426a00(Memory m, TrackPartAnimation &a, Location part, SourceTrackAnimationEffects effects,
                 TrackGeometryAnimationAudit audit)
{
    if (m.read<std::uint8_t>(part, 2, 0x426a09) & 0x10u)
        return;
    const auto incremented = a.retained_40 + 1u;
    const auto rate = effects.source_rate;
    a.retained_40 = incremented; // 426A27, even when the rate gate returns
    if (rate <= 80u && incremented < 2u)
        return;
    const double z_angle = double(a.angles.z) * as_float(0x41b60b61);
    a.retained_40 = 0; // 426A52, before the template copy/angle conversions
    contact::effects::EffectParticle particle;
    static_assert(sizeof(ModelTransform) == 80);
    particle.position.opaque = effects.template_pose.position_w;
    std::memcpy(particle.frame_payload.data(), reinterpret_cast<const std::byte *>(&effects.template_pose) + 16, 64);
    const auto z_index = std::bit_cast<std::int32_t>(ftol(z_angle));
    const auto y_index = std::bit_cast<std::int32_t>(ftol(double(a.angles.y) * as_float(0x41b60b61)));
    const auto x_index = std::bit_cast<std::int32_t>(ftol(double(a.angles.x) * as_float(0x41b60b61)));
    const auto matrix = camera_rotation(x_index, y_index, z_index); // 44FB00 into stack, not61C3A4
    const auto pivot_x = m.read<float>(part, 0x10, 0x426a90);
    const auto pivot_y = m.read<float>(part, 0x14, 0x426a95);
    const auto pivot_z = m.read<float>(part, 0x18, 0x426a9b);
    const double x = -double(pivot_x), y = -double(pivot_y), z = -(double(pivot_z) + 2000);
    (void)m.read<std::uint32_t>(part, 0x1c, 0x426aba); // source copied opaque W, no arithmetic
    const auto pose = m.pointer(part, 8, 0x426ace);
    particle.position.x = f32((double(matrix[4]) * y + double(matrix[0]) * x) + double(matrix[8]) * z);
    particle.position.y = f32((double(matrix[5]) * y + double(matrix[1]) * x) + double(matrix[9]) * z);
    particle.position.z = f32((double(matrix[6]) * y + double(matrix[2]) * x) + double(matrix[10]) * z);
    particle.position.x = f32(double(particle.position.x) + m.read<float>(pose, 0, 0x426b1e));
    particle.position.y = f32(double(particle.position.y) + m.read<float>(pose, 4, 0x426b2c));
    const double position_z = double(particle.position.z) + m.read<float>(pose, 8, 0x426b3b);
    particle.position.x = f32(double(particle.position.x) + m.read<float>(part, 0x10, 0x426b44));
    particle.position.y = f32(double(particle.position.y) + m.read<float>(part, 0x14, 0x426b52));
    particle.position.z = f32(position_z + m.read<float>(part, 0x18, 0x426b5b));
    particle.position.y = f32(double(particle.position.y) - 400);
    // 426970, on this actual stack staging object; unchanged45E110 consumes it.
    particle.flags = 7;
    particle.field_68 = as_float(0x3c23d70a);
    particle.lifetime_mode = 2;
    particle.render_enabled = 1;
    particle.gravity_scale = f32(double(effects.gravity.inverse_factor) * as_float(0xbd4ccccd));
    particle.animation_binding = 0;
    particle.base_scale = 10;
    particle.scale_fade_in = 3;
    particle.scale_fade_out = 12;
    particle.base_alpha = 128;
    particle.alpha_fade_in = 0;
    particle.alpha_fade_out = 18;
    particle.base_color = 0x406090;
    particle.texture = 7;
    particle.render_flags = 0x60;
    particle.lifetime = ftol(std::abs(effects.random.signed_value(18)) + 45);
    const double velocity = effects.random.signed_value(100);
    double offset = velocity * 8;
    if (std::abs(offset) < 800)
        offset = offset < 0 ? -800 : 800;
    particle.position.x = f32(double(particle.position.x) + f32(offset * matrix[0]));
    particle.position.y = f32(double(particle.position.y) + f32(offset * matrix[1]));
    particle.position.z = f32(double(particle.position.z) + offset * matrix[2]);
    particle.velocity.x = f32(velocity * matrix[0]);
    particle.velocity.y = f32(velocity * matrix[1]);
    particle.velocity.z = f32(velocity * matrix[2]);
    particle.velocity.y = f32(std::abs(effects.random.signed_value(50)));
    particle.angular_velocity = f32(effects.random.signed_value(22.5f));
    particle.source_undefined = {true, true, true, false, true, true, true};
    (void)contact::effects::allocate_effect_particle(particle, effects.particles, {}, audit.particle_frames);
}
} // namespace
SourceTrackAnimationOwners borrow_source_track_animation_owners(RaceSessionState &world,
                                                                TrackGeometryAnimationAudit audit)
{
    return {world.track_part_animations,
            world.source_track_normal_cursor,
            {world.random, world.effect_particles, world.gravity, world.impact_particles.rate_selector,
             world.model_draws.template_pose},
            audit};
}
void animate_source_track_geometry_426ed0(SourceAllocationBank &bank, const SourcePmdLoadingState &globals,
                                          SourceTrackModelPointer incoming, SourceTrackAnimationOwners &owners)
{
    owners.generated_cursor = globals.track.generated_normals; // 426ED4/DC, before descriptor admission
    const auto descriptor = incoming.source_descriptor();
    auto count = descriptor.field<std::uint32_t>(0x24).value();                                            // 426EE7
    auto part = Location{descriptor.location().allocation, descriptor.field<std::uint32_t>(0x48).value()}; // 426EEB
    if (part.offset == 0)
        part = {}; // nullable captured head; no part read for count<=1
    SourceBankReadMemo views(bank);
    Memory m{bank, &views};
    std::uint32_t index = 1;
    if (count <= index)
        return;
    do
    {
        if (!owners.records || index >= owners.records->records().size())
            throw SourceAllocationError(0x426efe, "reached576CD0 record lacks actual initialized session owner");
        auto &record = owners.records->records()[index];
        const auto kind = record.kind;  // 426EFE
        part = add(part, 64, 0x426f04); // raw+48, not selected/base+44
        switch (kind)
        {
        case 1:
        case 2:
        case 3:
            deform(m, globals, record, part, owners.generated_cursor, kind);
            break;
        case 4:
            rotate_4267d0(m, record, part, owners.audit);
            break;
        case 5:
            bob(m, record, part, false);
            break;
        case 6:
        case 7:
        case 14:
            orient(m, record, part, kind);
            break;
        case 8:
            color(m, record, part, true);
            break;
        case 9:
            color(m, record, part, false);
            break;
        case 10:
            scale(m, record, part, false);
            break;
        case 11:
            scale(m, record, part, true);
            break;
        case 12:
        case 13:
            sway_426e30(m, record, part);
            break;
        case 16:
            bob(m, record, part, true);
            rotate_4267d0(m, record, part, owners.audit);
            emit_426a00(m, record, part, owners.effects, owners.audit);
            break;
        default:
            break; // 15 and unsigned(kind-1)>15 touch no part payload
        }
        count = descriptor.field<std::uint32_t>(0x24).value(); // 426FB7, fresh after child
        ++index;                                               // 426FBA, after count read
    } while (index < count);
}
} // namespace sgn::retail
