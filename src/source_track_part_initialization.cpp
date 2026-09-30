#include "sgn/source_track_part_initialization.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/vehicle_tables.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using Location = SourcePmdLocation;
float f32(double value)
{
    return static_cast<float>(value);
}
using sgn::bit_reinterpret::as_float;
Location add(Location at, std::uint64_t amount, W site)
{
    if (amount > std::numeric_limits<W>::max() - at.offset)
        throw SourceAllocationError(site, "track preparation pointer overflows its actual allocation identity");
    at.offset += static_cast<W>(amount);
    return at;
}
float finite(float value, W site)
{
    if ((std::bit_cast<W>(value) & 0x7f800000u) == 0x7f800000u)
        throw SourceAllocationError(site,
                                    "track preparation reached nonfinite input outside its admitted numerical domain");
    return value;
}
struct Memory
{
    SourceAllocationBank &bank;
    template <class T> T read(Location at, W offset, W site) const
    {
        at = add(at, offset, site);
        const auto bytes = bank.bytes(at.allocation);
        if (at.offset > bytes.size() || sizeof(T) > bytes.size() - at.offset)
            throw SourceAllocationError(site, "reached track preparation read leaves actual owner");
        T value;
        std::memcpy(&value, bytes.data() + at.offset, sizeof(T));
        return value;
    }
    template <class T> void write(Location at, W offset, T value, W site) const
    {
        at = add(at, offset, site);
        auto bytes = bank.bytes(at.allocation);
        if (at.offset > bytes.size() || sizeof(T) > bytes.size() - at.offset)
            throw SourceAllocationError(site, "reached track preparation store leaves actual owner");
        std::memcpy(bytes.data() + at.offset, &value, sizeof(T));
    }
    Location pointer(Location at, W offset, W site) const
    {
        const auto word = read<W>(at, offset, site);
        return word ? Location{at.allocation, word} : Location{};
    }
    float number(Location at, W offset, W site) const
    {
        return finite(as_float(read<W>(at, offset, site)), site);
    }
    void copy_word(Location to, W target, Location from, W source, W site) const
    {
        const auto word = read<W>(from, source, site);
        write(to, target, word, site);
    }
};
// Scalar scratch access preserves raw copied W/UV bits. No vector snapshot or
// new scratch bank: these128 bytes are the existing session's actual owner.
float &lane(TrackPartPreparationScratch &s, W index, W component)
{
    auto &v = s.values[index];
    switch (component)
    {
    case 0:
        return v.x;
    case 1:
        return v.y;
    case 2:
        return v.z;
    default:
        return v.w;
    }
}
float number(TrackPartPreparationScratch &s, W index, W component, W site)
{
    return finite(lane(s, index, component), site);
}
void initialize_425ae0(TrackPartPreparationScratch &s)
{
    for (W i = 0; i < 32; ++i)
        lane(s, i / 4u, i % 4u) = 0; // 425AED32 ascendingDWORD stores
    lane(s, 0, 0) = 10000;
    lane(s, 2, 0) = -10000;
    lane(s, 4, 0) = 10000;
    lane(s, 6, 0) = -10000;
    lane(s, 0, 1) = 10000;
    lane(s, 2, 1) = 10000;
    lane(s, 4, 1) = -10000;
    lane(s, 6, 1) = -10000;
}
void publish_corner(Memory m, TrackPartPreparationScratch &s, W corner, Location vertex, Location uv, bool front,
                    W site)
{
    if (front)
    {
        for (W component = 0; component < 4; ++component)
            lane(s, corner, component) = as_float(m.read<W>(vertex, component * 4u, site));
        lane(s, corner + 1u, 0) = as_float(m.read<W>(uv, 0, site));
        lane(s, corner + 1u, 1) = as_float(m.read<W>(uv, 4, site));
    }
    else
    {
        lane(s, corner + 1u, 2) = as_float(m.read<W>(uv, 0, site));
        lane(s, corner + 1u, 3) = as_float(m.read<W>(uv, 4, site));
    }
}
void scan_425b50(Memory m, Location part, W selected, TrackPartPreparationScratch &s)
{
    const auto vertices = m.pointer(part, 0x30, 0x425b5c);
    const auto coordinates = m.pointer(part, 0x2c, 0x425b5f);
    initialize_425ae0(s); // before low WORD vertex count
    const W count = m.read<std::uint16_t>(part, 0x28, 0x425b69);
    for (W i = 0; i < count; ++i)
    {
        const auto vertex = add(vertices, std::uint64_t(i) * 16u, 0x425b7d);
        const auto uv = add(coordinates, std::uint64_t(i) * 16u, 0x425b89);
        const bool first_x = m.number(vertex, 0, 0x425b7d) <= number(s, 0, 0, 0x425b7f);
        const bool front = m.read<std::uint16_t>(uv, 10, 0x425b89) == static_cast<std::uint16_t>(selected);
        // First FCOM precedes the normal-word read. Other operands are read
        // only when their actual branch is reached, including repeated XY.
        if (first_x &&
            m.number(vertex, 4, front ? 0x425b9bu : 0x425cf5u) <= number(s, 0, 1, front ? 0x425b9eu : 0x425cf8u))
            publish_corner(m, s, 0, vertex, uv, front, front ? 0x425badu : 0x425d05u);
        else if (m.number(vertex, 0, front ? 0x425be2u : 0x425d18u) >= number(s, 2, 0, front ? 0x425be4u : 0x425d1au) &&
                 m.number(vertex, 4, front ? 0x425bf1u : 0x425d27u) <= number(s, 2, 1, front ? 0x425bf4u : 0x425d2au))
            publish_corner(m, s, 2, vertex, uv, front, front ? 0x425c03u : 0x425d37u);
        if (m.number(vertex, 0, front ? 0x425c36u : 0x425d48u) <= number(s, 4, 0, front ? 0x425c38u : 0x425d4au) &&
            m.number(vertex, 4, front ? 0x425c45u : 0x425d57u) >= number(s, 4, 1, front ? 0x425c48u : 0x425d5au))
            publish_corner(m, s, 4, vertex, uv, front, front ? 0x425c57u : 0x425d67u);
        else if (m.number(vertex, 0, front ? 0x425c8fu : 0x425d7au) >= number(s, 6, 0, front ? 0x425c91u : 0x425d7cu) &&
                 m.number(vertex, 4, front ? 0x425ca2u : 0x425d89u) >= number(s, 6, 1, front ? 0x425ca5u : 0x425d8cu))
            publish_corner(m, s, 6, vertex, uv, front, front ? 0x425cb8u : 0x425d99u);
    }
    lane(s, 0, 2) = 0;
    lane(s, 2, 2) = 0;
    lane(s, 4, 2) = 0;
    lane(s, 6, 2) = 0; // 425DB6..425DD4
}
void store_record_vector(Memory m, Location source, W offset, VehicleVector &to, W site)
{
    to.x = as_float(m.read<W>(source, offset, site));
    to.y = as_float(m.read<W>(source, offset + 4u, site));
    to.z = as_float(m.read<W>(source, offset + 8u, site));
    to.w = as_float(m.read<W>(source, offset + 12u, site));
}
} // namespace
void prepare_source_animated_track_part_425df0(SourceAllocationBank &bank, const SourcePmdLoadingState &globals,
                                               SourcePmdByteView descriptor, Location part, W kind,
                                               TrackPartPreparationScratch &s)
{
    Memory m{bank};
    const auto headers = m.pointer(part, 0x34, 0x425e00);
    const auto vertices = m.pointer(part, 0x30, 0x425e03);
    const auto coordinates = m.pointer(part, 0x2c, 0x425e06);
    const W polygon_count = m.read<std::uint16_t>(part, 0x24, 0x425e09);
    const W vertex_count = m.read<std::uint16_t>(part, 0x28, 0x425e12);
    W selected = 0xffffffffu;
    for (W i = 0; i < vertex_count; ++i)
    {
        const auto uv = add(coordinates, std::uint64_t(i) * 16u, 0x425e35);
        const auto normal = m.read<std::uint16_t>(uv, 10, 0x425e35);
        const auto normal_base = globals.track.normals; // 425E38 fresh62448C
        const auto at = add(normal_base, W(normal) * 16u, 0x425e42);
        if (std::abs(m.number(at, 0, 0x425e44)) < as_float(0x3727c5ac) &&
            std::abs(m.number(at, 4, 0x425e55)) < as_float(0x3727c5ac) &&
            m.number(at, 8, 0x425e67) < as_float(0xbf7fff58))
            selected = normal;
    }
    scan_425b50(m, part, selected, s); // reloadpart+30/+2C/+28, not captured mirrors
    const auto half = polygon_count >> 1;
    const auto side = half * 2u + 2u;
    // The original divides by half here, after scanning and its scratch stores.
    // Finite authored preparation requires a nonzero divisor. No fake no-op.
    if (half == 0)
        throw SourceAllocationError(0x425eb2, "track preparation reached zero polygon-half divisor");
    const float step = f32(1.0 / double(half));
    double t = 0;
    const auto subtract = [&](W a, W ac, W b, W bc, W site) {
        const auto first = number(s, a, ac, site);
        const auto second = number(s, b, bc, site);
        return double(first) - second;
    };
    // 425EC1..425EF7 remain live x87 values. Subsequent deltas spill tofloat.
    const double ax = subtract(2, 0, 0, 0, 0x425ec1), ay = subtract(2, 1, 0, 1, 0x425ecd),
                 az = subtract(2, 2, 0, 2, 0x425ed9);
    const double au = subtract(3, 0, 1, 0, 0x425ee5), av = subtract(3, 1, 1, 1, 0x425ef1);
    const float cx = f32(subtract(6, 0, 4, 0, 0x425efd)), cy = f32(subtract(6, 1, 4, 1, 0x425f1c)),
                cz = f32(subtract(6, 2, 4, 2, 0x425f3d));
    const float cu = f32(subtract(7, 0, 5, 0, 0x425f58)), cv = f32(subtract(7, 1, 5, 1, 0x425f67));
    const float back_au = f32(subtract(3, 2, 1, 2, 0x425f76)), back_av = f32(subtract(3, 3, 1, 3, 0x425f85));
    const float back_cu = f32(subtract(7, 2, 5, 2, 0x425f94)), back_cv = f32(subtract(7, 3, 5, 3, 0x425fa3));
    for (W i = 0, wave = 0; i < side; i += 2u, wave += 0x2000u)
    {
        const auto va = add(vertices, std::uint64_t(i) * 16u, 0x425fe5), vc = add(va, 16, 0x42604f);
        const auto qa = add(coordinates, std::uint64_t(i) * 16u, 0x425fc1), qc = add(qa, 16, 0x426039);
        const auto ba = add(vertices, std::uint64_t(side + i) * 16u, 0x4260cb), bc = add(ba, 16, 0x4260f9);
        const auto bqa = add(coordinates, std::uint64_t(side + i) * 16u, 0x42611b), bqc = add(bqa, 16, 0x426136);
        const float ay_spill = f32(ay), az_spill = f32(az);
        m.write(qa, 8, static_cast<std::uint16_t>(i), 0x425fc1);
        const double ax_scaled = ax * t;
        const float ay_scaled = f32(double(ay_spill) * t), az_scaled = f32(double(az_spill) * t);
        m.write(va, 0, f32(double(number(s, 0, 0, 0x425fdd)) + ax_scaled), 0x425fe5);
        m.write(va, 4, f32(double(number(s, 0, 1, 0x425fea)) + ay_scaled), 0x425ff9);
        m.write(va, 8, f32(double(number(s, 0, 2, 0x425ffc)) + az_scaled), 0x42600b);
        m.write(qa, 0, f32(au * t + number(s, 1, 0, 0x426012)), 0x426018);
        m.write(qa, 4, f32(av * t + number(s, 1, 1, 0x42601f)), 0x426025);
        const double cx_scaled = double(cx) * t;
        const double cy_live = double(cy) * t;
        m.write(qa, 12, W{0xffffff}, 0x426032);
        m.write(qc, 8, static_cast<std::uint16_t>(i + 1u), 0x426039);
        const auto cy_scaled = f32(cy_live), cz_scaled = f32(double(cz) * t);
        m.write(vc, 0, f32(double(number(s, 4, 0, 0x426047)) + cx_scaled), 0x42604f);
        m.write(vc, 4, f32(double(number(s, 4, 1, 0x426054)) + cy_scaled), 0x42605d);
        m.write(vc, 8, f32(double(number(s, 4, 2, 0x42605f)) + cz_scaled), 0x426068);
        m.write(qc, 0, f32(double(cu) * t + number(s, 5, 0, 0x426074)), 0x42607a);
        m.write(qc, 4, f32(double(cv) * t + number(s, 5, 1, 0x426082)), 0x426088);
        m.write(qc, 12, W{0xffffff}, 0x42608b);
        if (kind == 1)
        {
            const auto phase = wave / side; // unsignedDIV, no phase mask
            if (phase >= vehicle_tables::sine_bits.size())
                throw SourceAllocationError(0x42609b, "preparation sine index exceeds literal table");
            const double height = double(as_float(vehicle_tables::sine_bits[phase])) * -256.0;
            m.write(va, 4, f32(double(number(s, 0, 1, 0x4260a8)) + height), 0x4260b3);
            m.write(vc, 4, f32(double(number(s, 4, 1, 0x4260b6)) + height), 0x4260be);
        }
        // Source copies four DWORDs per vertex/coordinate, including opaque W
        // and retained upper-coordinate words. Do not snapshot the whole pair:
        // overlapping source/destination ranges must see earlier writes.
        m.copy_word(ba, 0, va, 0, 0x4260cb);
        const double back_u_product = double(back_au) * t; // 4260C8/D0 retains until4260F0
        m.copy_word(ba, 4, va, 4, 0x4260d5);
        m.copy_word(ba, 8, va, 8, 0x4260de);
        m.copy_word(ba, 12, va, 12, 0x4260ea);
        const double back_u = back_u_product + number(s, 1, 2, 0x4260f0);
        m.copy_word(bc, 0, vc, 0, 0x4260f9);
        m.copy_word(bc, 4, vc, 4, 0x426100);
        m.copy_word(bc, 8, vc, 8, 0x426109);
        m.copy_word(bc, 12, vc, 12, 0x426112);
        m.copy_word(bqa, 0, qa, 0, 0x42611b);
        m.copy_word(bqa, 4, qa, 4, 0x426121);
        m.copy_word(bqa, 8, qa, 8, 0x426127);
        m.copy_word(bqa, 12, qa, 12, 0x42612d);
        m.copy_word(bqc, 0, qc, 0, 0x426136);
        m.copy_word(bqc, 4, qc, 4, 0x42613c);
        m.copy_word(bqc, 8, qc, 8, 0x426141);
        m.copy_word(bqc, 12, qc, 12, 0x426147);
        m.write(bqa, 8, static_cast<std::uint16_t>(side + i), 0x426151);
        m.write(bqc, 8, static_cast<std::uint16_t>(side + i + 1u), 0x426155);
        m.write(bqa, 0, f32(back_u), 0x42615c);
        m.write(bqa, 4, f32(double(back_av) * t + number(s, 1, 3, 0x426174)), 0x426187);
        m.write(bqc, 0, f32(double(back_cu) * t + number(s, 5, 2, 0x42618f)), 0x426195);
        m.write(bqc, 4, f32(double(back_cv) * t + number(s, 5, 3, 0x42619d)), 0x4261a3);
        t += step; // 4261AC: accumulated PC53 value, never spilled tofloat
    }
    // Descriptor+38 is first reached only after all geometry writes.
    const auto corners = m.pointer(descriptor.location(), 0x38, 0x4261de);
    for (W i = 0; i < half; ++i)
    {
        const auto header = add(headers, std::uint64_t(i) * 8u, 0x4261ea);
        const auto flags = m.read<std::uint8_t>(header, 3, 0x4261ea);
        m.write(header, 3, static_cast<std::uint8_t>(flags & 0x7fu), 0x4261f1);
        const auto corner = m.read<std::uint16_t>(header, 6, 0x4261f3);
        const auto at = add(corners, W(corner) * 2u, 0x4261fd);
        const auto front = i * 2u;
        m.write(at, 2, static_cast<std::uint16_t>(front + 1u), 0x426203);
        m.write(at, 4, static_cast<std::uint16_t>(front + 3u), 0x42620a);
        m.write(at, 6, static_cast<std::uint16_t>(front + 2u), 0x426211);
        m.write(at, 0, static_cast<std::uint16_t>(front), 0x426218);
        m.write(at, half * 8u, static_cast<std::uint16_t>(front + side), 0x42621b);
        m.write(at, half * 8u + 2u, static_cast<std::uint16_t>(front + side + 1u), 0x426223);
        m.write(at, half * 8u + 4u, static_cast<std::uint16_t>(front + side + 3u), 0x42622c);
        m.write(at, half * 8u + 6u, static_cast<std::uint16_t>(front + side + 2u), 0x426235);
    }
}
void initialize_source_track_parts_426260(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    if (world.scene_mode == 2u)
        return; // 426266/26F: do not ask for scene resources
    auto &scene = platform.source_scene_resources();
    const auto handle = scene.model_handle(3); // 426275 actual636448 publication
    if (!handle)
        return; // 426282: neither bank nor records are accessed
    const auto descriptor = scene.models().source_at(*handle).descriptor(); // capture once
    auto count = descriptor.field<W>(0x24).value();                         // 426288
    if (count <= 1u)
        return;                                              // no descriptor+48 or optional-bank access
    const auto raw_part = descriptor.field<W>(0x48).value(); // 426294
    auto part = raw_part ? Location{descriptor.location().allocation, raw_part} : Location{};
    auto &bank = platform.source_allocations();
    Memory m{bank};
    const auto &globals = scene.models().source_loading_state();
    W index = 0, record_offset = 0;
    do
    {
        const auto before = add(globals.track.texture_indices, record_offset,
                                0x4262ac); // actual624484, not a PMD-header reconstruction
        const auto kind = m.read<W>(before, 0x30, 0x4262b1);
        if (kind >= 1u && kind <= 3u)
            prepare_source_animated_track_part_425df0(bank, globals, descriptor, part, kind,
                                                      world.track_part_preparation);
        const auto authored = add(globals.track.texture_indices, record_offset, 0x4262ca); // fresh after child
        const auto reloaded_kind = m.read<W>(authored, 0x30, 0x4262d0);
        // NEW host materialization of the existing optional32-slot static
        // owner. All2304 PE bytes are proven zero before any source producer.
        // No authored read failure is replaced by zeros; existing banks retain
        // their previous +3C/+40 and all unreached/unused fields.
        if (!world.track_part_animations)
            world.track_part_animations.emplace();
        auto records = world.track_part_animations->records();
        if (index >= records.size())
            throw SourceAllocationError(0x4262d7, "reached animation record leaves sole32-slot576CD0 owner");
        auto &record = records[index];
        record.kind = reloaded_kind; // 4262D7, before all vector reads
        record_offset += 64u;        // 4262DF; authored remains captured
        store_record_vector(m, authored, 0, record.angles, 0x4262e2);
        store_record_vector(m, authored, 0x10, record.angular_velocity, 0x4262fd);
        store_record_vector(m, authored, 0x20, record.parameters, 0x42631c);
        const auto parameter34 = m.read<W>(authored, 0x34, 0x426332);
        const auto parameter44 = m.read<W>(authored, 0x38, 0x426335);
        record.parameter_34 = parameter34;         // 42633B
        record.parameter_44 = parameter44;         // 426341
        count = descriptor.field<W>(0x24).value(); // 426347, before phase reset
        record.phase_u = 0;                        // 42634A only; +3C/+40 remain their actual retained values
        part = add(part, 64, 0x426351);
        ++index;
    } while (index < count);
}
} // namespace sgn::retail
