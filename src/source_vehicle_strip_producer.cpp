#include "sgn/source_vehicle_strip_producer.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>

extern "C" std::uint32_t sgn_source_skid_add(std::uint32_t, std::uint32_t) noexcept;
extern "C" std::uint32_t sgn_source_skid_sub(std::uint32_t, std::uint32_t) noexcept;
extern "C" std::uint32_t sgn_source_skid_mul(std::uint32_t, std::uint32_t) noexcept;
extern "C" std::uint32_t sgn_source_skid_compare(std::uint32_t, std::uint32_t) noexcept;
extern "C" void sgn_source_skid_normalize_44eda0(void *, void *) noexcept;
extern "C" void sgn_source_skid_side_41cb99(const void *, const void *, void *) noexcept;
extern "C" std::uint32_t sgn_source_skid_join_41cc8c(const void *) noexcept;
extern "C" std::uint32_t sgn_source_skid_cumulative_41d06f(void *, std::uint32_t) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using H = std::uint16_t;
using B = std::uint8_t;
using sgn::bit_reinterpret::as_word;
constexpr W f30 = 0x41f00000, f85 = 0x41080000, f2 = 0x40000000, fm60 = 0xc2700000, f200 = 0x43480000,
            f800 = 0x44480000;
struct LocalVector
{
    W x{}, y{}, z{}, w{};
}; // original four stack DWORDs, not a world snapshot
static_assert(sizeof(LocalVector) == 16 && sizeof(VehicleVector) == 16);
static_assert(sizeof(VehicleSkidSegment) == 0x34 && offsetof(VehicleSkidSegment, first_vertex) == 0x30);
static_assert(offsetof(VehicleSkidSegment, end) == 0x10 && offsetof(VehicleSkidSegment, step) == 0x20);
W word(const VehicleVector &v, unsigned lane)
{
    switch (lane)
    {
    case 0:
        return as_word(v.x);
    case 1:
        return as_word(v.y);
    case 2:
        return as_word(v.z);
    default:
        return as_word(v.w);
    }
}
void store(VehicleVector &v, unsigned lane, W value)
{
    // Volatile scalar stores retain even source identity-valued MOV writes.
    float *at = nullptr;
    switch (lane)
    {
    case 0:
        at = &v.x;
        break;
    case 1:
        at = &v.y;
        break;
    case 2:
        at = &v.z;
        break;
    default:
        at = &v.w;
        break;
    }
    *static_cast<volatile float *>(at) = std::bit_cast<float>(value);
}
void store_vector(VehicleVector &destination, const LocalVector &source, bool defined_w)
{
    store(destination, 0, source.x);
    store(destination, 1, source.y);
    store(destination, 2, source.z);
    // NEW inherited undefined-stack-W policy: retain the reached destination
    // word, still issue its store. No guessed stack value enters XYZ arithmetic.
    store(destination, 3, defined_w ? source.w : word(destination, 3));
}
void mark(VehicleSkidWriteAudit *audit, unsigned wheel, W segment, unsigned lane, bool undefined)
{
    if (audit)
        audit->undefined_writes[wheel].set(std::size_t(segment) * 3u + lane, undefined);
}
unsigned current(RaceSessionState &world, W site)
{
    const auto slot = world.current_slot;
    if (slot >= world.vehicles.physical_slots)
        throw SourceAllocationError(site, "reached skid current vehicle outside six actual owners");
    return slot;
}
void admit(RaceSessionPlatform &platform, W site)
{
    platform.require_owner_thread();
    if (!platform.source_construction_only_entry())
        throw SourceAllocationError(site, "strip production requires actual source platform");
}
struct Pointer
{
    SourceAllocationRef allocation;
    W offset{}, token{};
    bool transported{};
    static Pointer raw(W value)
    {
        return {{}, 0, value, true};
    }
    static Pointer descriptor(const VehicleStripGeometry &g)
    {
        const auto &b = g.source.descriptor.source_binding();
        return {b.allocation, b.byte_offset, 0, false};
    }
};
struct Memory
{
    SourceAllocationBank &bank;
    std::span<B> bytes(const Pointer &pointer, W offset, W count, W site) const
    {
        try
        {
            auto at = pointer.transported ? bank.decode_address(pointer.token)
                                          : SourceAddressLocation{pointer.allocation, pointer.offset};
            const auto total = std::uint64_t(at.offset) + (pointer.transported ? pointer.offset : 0u) + offset;
            if (total > std::numeric_limits<W>::max())
                throw SourceAllocationError(site, "skid source offset leaves DWORD domain");
            const auto all = bank.bytes(at.allocation);
            if (total > all.size() || count > all.size() - total)
                throw SourceAllocationError(site, "reached skid field outside actual source allocation");
            return all.subspan(static_cast<std::size_t>(total), count);
        }
        catch (const SourceAllocationError &e)
        {
            throw SourceAllocationError(site, e.what());
        }
    }
    template <class T> T read(const Pointer &at, W offset, W site) const
    {
        const auto source = bytes(at, offset, static_cast<W>(sizeof(T)), site);
        T value;
        std::memcpy(&value, source.data(), sizeof(T));
        return value;
    }
    template <class T> void write(const Pointer &at, W offset, T value, W site) const
    {
        const auto target = bytes(at, offset, static_cast<W>(sizeof(T)), site);
        std::memcpy(target.data(), &value, sizeof(T));
    }
};
void advance(VehicleStripGeometry &g, VehicleStripHeader &h)
{
    const W next = (g.field_5712f0 + 1u) & 127u;
    const W count = g.field_5712f4;
    g.field_5712f0 = next;
    if (count < 128u)
        g.field_5712f4 = count + 1u;
    h.field_08 = next;
    h.field_04 = h.field_04 + 1u;
}
} // namespace
void emit_source_vehicle_strip_quad_41cb40(RaceSessionPlatform &platform, unsigned header_slot, unsigned wheel, W index,
                                           W surface_argument)
{
    admit(platform, 0x41cb40);
    auto &world = platform.state();
    (void)surface_argument; // PE never reads stack argument
    if (header_slot >= world.vehicles.physical_slots || wheel >= 4 || index >= 64)
        throw SourceAllocationError(0x41cb59, "captured skid ring outside actual vehicle record");
    auto &ring = world.vehicles.skid_rings[header_slot].items[wheel];
    auto &segment = ring.segments[index];
    const auto descriptor = Pointer::descriptor(world.strip);
    const Memory m{platform.source_allocations()};
    const auto part = Pointer::raw(m.read<W>(descriptor, 0x48, 0x41cb5d));
    const W first = segment.first_vertex; // 41CB63 before pointed part fields
    auto vertices = Pointer::raw(m.read<W>(part, 0x30, 0x41cb66));
    const W coordinates_token = m.read<W>(part, 0x2c, 0x41cb69);
    const W first_offset = first << 4u;
    auto coordinates = Pointer::raw(coordinates_token);
    coordinates.offset = first_offset;
    auto previous = vertices;
    previous.offset = ring.segments[(index - 1u) & 63u].first_vertex << 4u;
    vertices.offset = first_offset;
    const auto up_slot = current(world, 0x41cb94);
    LocalVector side;
    sgn_source_skid_side_41cb99(&world.vehicles.physics[up_slot].up, &segment.step, &side);
    // Reached scalar stores, no whole-quad bounds preflight or native geometry.
    m.write<W>(vertices, 0, sgn_source_skid_add(side.x, word(segment.start, 0)), 0x41cc0f);
    m.write<W>(vertices, 4, sgn_source_skid_add(side.y, word(segment.start, 1)), 0x41cc17);
    m.write<W>(vertices, 8, sgn_source_skid_add(side.z, word(segment.start, 2)), 0x41cc20);
    m.write<W>(vertices, 0x10, sgn_source_skid_sub(word(segment.start, 0), side.x), 0x41cc28);
    m.write<W>(vertices, 0x14, sgn_source_skid_sub(word(segment.start, 1), side.y), 0x41cc30);
    m.write<W>(vertices, 0x18, sgn_source_skid_sub(word(segment.start, 2), side.z), 0x41cc39);
    m.write<W>(vertices, 0x20, sgn_source_skid_add(side.x, word(segment.end, 0)), 0x41cc42);
    m.write<W>(vertices, 0x24, sgn_source_skid_add(side.y, word(segment.end, 1)), 0x41cc4b);
    const W third_z = sgn_source_skid_add(side.z, word(segment.end, 2));
    const W corner_pointer = m.bank.advance_address(coordinates_token, first_offset); // metadata only,41CC5A
    ring.last_corner = corner_pointer ? std::optional<W>(corner_pointer) : std::nullopt;
    m.write<W>(vertices, 0x28, third_z, 0x41cc60);
    const W fourth_x = sgn_source_skid_sub(word(segment.end, 0), side.x);
    const W header_index = m.read<H>(part, 0x24, 0x41cc6b);
    m.write<W>(vertices, 0x30, fourth_x, 0x41cc6f);
    m.write<W>(vertices, 0x34, sgn_source_skid_sub(word(segment.end, 1), side.y), 0x41cc78);
    const W fourth_z = sgn_source_skid_sub(word(segment.end, 2), side.z);
    const auto headers = Pointer::raw(m.read<W>(part, 0x34, 0x41cc81));
    m.write<W>(vertices, 0x38, fourth_z, 0x41cc84);
    m.write<B>(headers, header_index * 8u + 1u, 5, 0x41cc87);
    // Source differences remain extended until FCOMP. The six local input
    // words only carry the original ordered loads, with no intermediate spill.
    W pairs[6];
    pairs[0] = m.read<W>(vertices, 0, 0x41cc8c);
    pairs[1] = m.read<W>(previous, 0x20, 0x41cc8e);
    pairs[2] = m.read<W>(vertices, 4, 0x41cc91);
    pairs[3] = m.read<W>(previous, 0x24, 0x41cc94);
    pairs[4] = m.read<W>(vertices, 8, 0x41cc97);
    pairs[5] = m.read<W>(previous, 0x28, 0x41cc9a);
    if (sgn_source_skid_join_41cc8c(pairs) & 1u)
    {
        constexpr W loads[]{0x41ccc0, 0x41ccc8, 0x41ccce, 0x41ccd4, 0x41ccda, 0x41ccde, 0x41cce4, 0x41ccea};
        constexpr W stores[]{0x41ccc6, 0x41cccb, 0x41ccd1, 0x41ccd7, 0x41ccdc, 0x41cce1, 0x41cce7, 0x41cced};
        for (W lane = 0; lane < 8; ++lane)
        {
            const auto value = m.read<W>(previous, 0x20 + lane * 4u, loads[lane]);
            m.write<W>(vertices, lane * 4u, value, stores[lane]);
        }
    }
    else
    {
        m.write<B>(coordinates, 0xf, 0, 0x41ccf7);
        m.write<B>(coordinates, 0x1f, 0, 0x41ccfa);
    }
    W count = world.strip.field_5712f4;
    if (count >= 127u)
    {
        count = 127u;
        world.strip.field_5712f4 = count;
    }
    m.write<H>(part, 0x24, static_cast<H>(count), 0x41cd14);
    count <<= 2u;
    m.write<H>(part, 0x26, static_cast<H>(count), 0x41cd1b);
    m.write<H>(part, 0x28, static_cast<H>(count), 0x41cd1f);
}
void update_source_vehicle_wheel_strip_41cd30(RaceSessionPlatform &platform, unsigned wheel,
                                              VehicleSkidWriteAudit *audit)
{
    admit(platform, 0x41cd30);
    auto &world = platform.state();
    auto &g = world.strip;
    if (g.field_5712f8 == 0)
        return; // no current/wheel owner access
    const auto slot = current(world, 0x41cd46);
    auto &vehicle = world.vehicles.physics[slot];
    const auto above30 = sgn_source_skid_compare(as_word(vehicle.speed), f30);
    if (wheel >= 4)
        throw SourceAllocationError(0x41cd66, "wheel header leaves the four actual records");
    auto &header = world.vehicles.strips[slot].items[wheel];
    bool stop = false;
    if ((above30 & 0x41u) == 0)
        stop = (sgn_source_skid_compare(as_word(vehicle.wheels[wheel].grip_usage), f85) & 1u) != 0;
    if (!stop)
    {
        if (sgn_source_skid_compare(as_word(vehicle.speed), f30) & 1u)
            stop = (sgn_source_skid_compare(as_word(vehicle.wheels[wheel].grip_usage), f2) & 1u) != 0;
        if (!stop)
            stop = vehicle.contact_class != 0;
    }
    if (stop)
    {
        if (header.field_00 != 0)
            advance(g, header);
        header.field_00 = 0;
        return;
    }
    const W up_x = sgn_source_skid_mul(as_word(vehicle.up.x), fm60);
    const auto &center = world.vehicles.presentation[slot].wheel_centers[wheel];
    LocalVector at{word(center, 0), word(center, 1), word(center, 2), word(center, 3)};
    LocalVector step;
    step.x = up_x;
    step.y = sgn_source_skid_mul(as_word(vehicle.up.y), fm60);
    step.z = sgn_source_skid_mul(as_word(vehicle.up.z), fm60);
    const W at_x = sgn_source_skid_add(at.x, step.x);
    W index = header.field_04 & 63u;
    const W active = header.field_00;
    at.x = at_x;
    at.y = sgn_source_skid_add(at.y, step.y);
    at.z = sgn_source_skid_add(at.z, step.z);
    auto &ring = world.vehicles.skid_rings[slot].items[wheel];
    auto &initial = ring.segments[index];
    if (active == 0)
    {
        store_vector(initial.start, at, true);
        mark(audit, wheel, index, 0, false);
    }
    header.field_00 = 1;
    step.x = sgn_source_skid_sub(at.x, word(initial.start, 0));
    step.y = sgn_source_skid_sub(at.y, word(initial.start, 1));
    step.z = sgn_source_skid_sub(at.z, word(initial.start, 2));
    W distance;
    sgn_source_skid_normalize_44eda0(&step, &distance);
    if ((sgn_source_skid_compare(distance, f800) & 0x41u) == 0)
    {
        header.field_00 = 0;
        return;
    }
    step.x = sgn_source_skid_mul(step.x, f200);
    step.y = sgn_source_skid_mul(step.y, f200);
    step.z = sgn_source_skid_mul(step.z, f200);
    const LocalVector start{word(initial.start, 0), word(initial.start, 1), word(initial.start, 2),
                            word(initial.start, 3)};
    const bool short_end = (sgn_source_skid_compare(distance, f200) & 1u) != 0;
    LocalVector endpoint;
    if (short_end)
        endpoint = at;
    else
    {
        endpoint.x = sgn_source_skid_add(start.x, step.x);
        endpoint.y = sgn_source_skid_add(start.y, step.y);
        endpoint.z = sgn_source_skid_add(start.z, step.z);
    }
    const auto surface_slot = current(world, 0x41cf80);
    W cumulative = f200;
    const W surface = std::bit_cast<W>(
        world.vehicles.contact[surface_slot].wheels[wheel].type); // 41CF90, reached even though child ignores it
    store_vector(initial.start, start, true);                     // four original identity stores
    store_vector(initial.end, endpoint, short_end);
    mark(audit, wheel, index, 1, !short_end);
    store_vector(initial.step, step, false);
    mark(audit, wheel, index, 2, true);
    initial.first_vertex = header.field_08 << 2u;
    emit_source_vehicle_strip_quad_41cb40(platform, slot, wheel, index, surface);
    for (;;)
    {
        if (sgn_source_skid_compare(distance, f200) & 1u)
        {
            const auto pointer = Pointer::raw(ring.last_corner.value_or(0));
            const Memory m{platform.source_allocations()};
            m.write<B>(pointer, 0x2f, 0, 0x41d10a);
            m.write<B>(pointer, 0x3f, 0, 0x41d10e);
            return;
        }
        advance(g, header);
        const LocalVector previous = endpoint;
        index = (index + 1u) & 63u;
        endpoint.x = sgn_source_skid_add(endpoint.x, step.x);
        endpoint.y = sgn_source_skid_add(previous.y, step.y);
        endpoint.z = sgn_source_skid_add(previous.z, step.z);
        const W status = sgn_source_skid_cumulative_41d06f(&cumulative, distance); // FST then FCOMP of unspilled sum
        auto &next = ring.segments[index];
        store_vector(next.start, previous, false);
        mark(audit, wheel, index, 0, true);
        if ((status & 1u) == 0)
            return; // 41D119 final seed only, no end/step/index stores
        store_vector(next.end, endpoint, false);
        mark(audit, wheel, index, 1, true);
        store_vector(next.step, step, false);
        mark(audit, wheel, index, 2, true);
        next.first_vertex = header.field_08 << 2u;
        emit_source_vehicle_strip_quad_41cb40(platform, slot, wheel, index, surface);
    }
}
void update_source_vehicle_strip_41d140(RaceSessionPlatform &platform, VehicleSkidWriteAudit *audit)
{
    admit(platform, 0x41d140);
    auto &world = platform.state();
    if (world.impact_particles.rate_selector < 80u)
        return;
    const auto slot = current(world, 0x41d149);
    const auto recording = world.vehicles.race[slot].recording;
    if (recording && world.replays.status(*recording) == 1)
        return;
    update_source_vehicle_wheel_strip_41cd30(platform, 0, audit);
    update_source_vehicle_wheel_strip_41cd30(platform, 1, audit); // fresh current vehicle in second call
}
} // namespace sgn::retail
