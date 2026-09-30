#include "sgn/source_world_spatial_emission.hpp"
#include "sgn/detail/world_polygon_emission.hpp"
#include "sgn/source_bank_read_memo.hpp"
#include "sgn/host_profiler.hpp"
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace sgn::retail
{
namespace
{
// NEW HOST performance (R30 perf-tick), not recovered behaviour. One Access lives for ONE synchronous
// 429EA0/42A090 invocation on the owner thread. That invocation reads PMD/transformed words, stores
// known DWORDs into the existing packet records and encodes address tokens; it never allocates or
// releases a bank allocation and never stores an opaque transport word. Therefore:
//  - reads go through SourceBankReadMemo (its documented contract, sgn/source_bank_read_memo.hpp): the
//    first access of an allocation performs the full bank admission, later reads return the same span;
//  - `encode` memoizes the LAST location encoded per lane. SourceAllocationBank::encode_address(A,o) is
//    admission (thread, initialized, live A, o <= size: none of which can change during the call) plus
//    find_token/append_token, and tokens are append-only and never erased, so once (A,o) returned t it
//    returns t again with no side effect. The memo returns that same t without repeating the lookup.
//    The attribute base and the transformed cursor are the same location for every record of a call.
struct Access
{
    SourceAllocationBank &bank;
    SourceBankReadMemo memo;
    struct Encoded
    {
        SourcePmdLocation location;
        std::uint32_t token{};
        bool valid{};
    };
    std::array<Encoded, 4> encoded{}; // one per packet lane kind: header, indices, vertices, attributes
    explicit Access(SourceAllocationBank &b) : bank(b), memo(b) {}
    Access(const Access &) = delete;
    Access &operator=(const Access &) = delete;
    std::uint32_t encode(unsigned lane, SourcePmdLocation location)
    {
        auto &e = encoded[lane];
        if (e.valid && e.location == location)
            return e.token;
        const auto token = bank.encode_address(location.allocation, location.offset);
        e = {location, token, true};
        return token;
    }
};
SourcePmdLocation offset(SourcePmdLocation base, std::uint64_t delta, std::uint32_t site)
{
    if (delta > std::numeric_limits<std::uint32_t>::max() - base.offset)
        throw SourceAllocationError(site, "source spatial emission offset leaves host address domain");
    base.offset += static_cast<std::uint32_t>(delta);
    return base;
}
// == bank.bytes(allocation) + this extent test (same site, message and value), through the memo.
template <class T> T read(Access &a, SourcePmdLocation location, std::uint32_t site)
{
    return a.memo.whole_read<T>(location.allocation, location.offset, site,
                                "reached source spatial emission field outside allocation");
}
// PMD pointer fields contain this allocation's normalized offsets, unlike
// bank address tokens in world packets/lists. Preserve a literal source NULL.
SourcePmdLocation pointed_word(Access &bank, SourcePmdLocation owner, std::uint32_t member, std::uint32_t site)
{
    const auto word = read<std::uint32_t>(bank, offset(owner, member, site), site);
    return word ? SourcePmdLocation{owner.allocation, word} : SourcePmdLocation{};
}
SourcePmdLocation transformed_pointer(Access &a, const WorldPolygonFrame &frame, std::uint32_t site)
{
    const auto &binding = frame.transformed.allocation.source_binding();
    if (binding.bank != &a.bank)
        throw SourceAllocationError(site, "transformed source cursor belongs to another bank");
    // Capture the current61C510, not a reset61C50C projection. No pointed read.
    return offset({binding.allocation, binding.byte_offset}, std::uint64_t(frame.transformed.count) * 32u, site);
}
std::shared_ptr<RaceSessionPlatform> admit_records(SourceAllocationBank &bank, WorldPolygonFrame &frame,
                                                   const WorldSpatialRecords &records, std::uint32_t site)
{
    frame.indexed.require_owner_thread();
    if (!records.source_bound() || records.source_binding().bank != &bank)
        throw SourceAllocationError(site, "spatial emission needs records from the same source bank");
    auto platform = records.source_binding().platform.lock();
    if (!platform)
        throw SourceAllocationError(site, "source spatial record platform expired");
    return platform; // invocation-only lifetime guard, no payload ownership copy
}
SourcePmdLocation polygon_list(Access &a, const WorldSpatialRecords &records, std::uint32_t row)
{
    const auto location = a.bank.decode_address(records.source_word(row, 0)); // lane1 is never read here
    return {location.allocation, location.offset};
}
bool facing(Access &bank, const WorldPolygonFrame &frame, SourcePmdLocation vertices, SourcePmdLocation normals,
            SourcePmdLocation header, std::uint16_t vertex, std::uint32_t flags, bool clipped)
{
    const auto point = offset(vertices, std::uint32_t(vertex) * 16u, clipped ? 0x429fa0u : 0x42a130u);
    // XYZ subtraction is reached even for a two-sided polygon. W is not read.
    const double dx =
        double(read<float>(bank, point, clipped ? 0x429fa6u : 0x42a136u)) - frame.model_view.local_camera.x;
    const double dy =
        double(read<float>(bank, offset(point, 4, clipped ? 0x429faeu : 0x42a13eu), clipped ? 0x429faeu : 0x42a13eu)) -
        frame.model_view.local_camera.y;
    const double dz =
        double(read<float>(bank, offset(point, 8, clipped ? 0x429fb7u : 0x42a147u), clipped ? 0x429fb7u : 0x42a147u)) -
        frame.model_view.local_camera.z;
    if ((flags & 0x80000000u) != 0)
        return true;
    const auto index =
        read<std::uint16_t>(bank, offset(header, 4, clipped ? 0x429fc7u : 0x42a157u), clipped ? 0x429fc7u : 0x42a157u);
    const auto normal = offset(normals, std::uint32_t(index) * 16u, clipped ? 0x429fceu : 0x42a15eu);
    // The source multiplies Z then Y, adds, then multiplies/adds X. Preserve
    // the existing strict PC53/double numeric policy and C0 unordered branch.
    const double z =
        dz * read<float>(bank, offset(normal, 8, clipped ? 0x429fd0u : 0x42a160u), clipped ? 0x429fd0u : 0x42a160u);
    const double y =
        dy * read<float>(bank, offset(normal, 4, clipped ? 0x429fd5u : 0x42a165u), clipped ? 0x429fd5u : 0x42a165u);
    const double zy = z + y;
    const double x = dx * read<float>(bank, normal, clipped ? 0x429fdcu : 0x42a16cu);
    return !(zy + x >= 0.0); // FCOMP46E1EC(+0), FNSTSW C0 includes unordered
}
struct CapturedFields
{
    SourcePmdLocation normals, indices, vertices, attributes;
    SourceTrackPartPointer part; // source register reloaded only after a nonempty batch
};
CapturedFields capture_clipped(Access &bank, const WorldPolygonFrame &frame)
{
    CapturedFields result;
    const auto descriptor = frame.active_track.source_descriptor().location(); // 429EA6, actual61C52C
    result.normals = pointed_word(bank, descriptor, 0x28, 0x429eb0);
    result.indices = pointed_word(bank, descriptor, 0x38, 0x429eb3);
    result.part = frame.active_track_base_part;             // 429EB9, independent61C530
    const auto part = result.part.source_part().location(); // host relocation/lifetime admission at first part read
    result.vertices = pointed_word(bank, part, 0x30, 0x429ec4);
    result.attributes = pointed_word(bank, part, 0x2c, 0x429ecd);
    return result;
}
CapturedFields capture_inside(Access &bank, const WorldPolygonFrame &frame)
{
    CapturedFields result;
    result.part = frame.active_track_base_part;                                // 42A0A1, before descriptor capture
    const auto descriptor = frame.active_track.source_descriptor().location(); // 42A0B0
    result.normals = pointed_word(bank, descriptor, 0x28, 0x42a0ba);
    result.indices = pointed_word(bank, descriptor, 0x38, 0x42a0bd);
    const auto part = result.part.source_part().location();
    result.vertices = pointed_word(bank, part, 0x30, 0x42a0c9);
    result.attributes = pointed_word(bank, part, 0x2c, 0x42a0d1);
    return result;
}
void emit_record(Access &bank, WorldPolygonFrame &frame, const std::shared_ptr<RaceSessionPlatform> &platform,
                 detail::PacketEmissionCursors &cursors, SourcePmdLocation header, SourcePmdLocation indices,
                 SourcePmdLocation attributes, std::uint32_t flags, std::uint32_t common, bool clipped)
{
    const bool alpha = (flags & 0x40000000u) != 0; // no model/part alpha override for either spatial leaf
    auto &current = alpha ? cursors.alpha : cursors.opaque;
    const std::array<std::uint32_t, 5> pc =
        clipped ? (alpha ? std::array<std::uint32_t, 5>{0x42a000, 0x42a005, 0x42a00e, 0x42a014, 0x42a017}
                         : std::array<std::uint32_t, 5>{0x42a025, 0x42a02a, 0x42a033, 0x42a039, 0x42a03c})
                : (alpha ? std::array<std::uint32_t, 5>{0x42a18b, 0x42a193, 0x42a196, 0x42a199, 0}
                         : std::array<std::uint32_t, 5>{0x42a1b1, 0x42a1b3, 0x42a1b6, 0x42a1b9, 0});
    const auto store = [&](unsigned lane, std::uint32_t word, unsigned instruction, WorldPacketWriteKind kind) {
        current.require_source_platform(platform); // same owner, reached complete20-byte record admission
        current.store_source_word(lane, word);
        cursors.observe(instruction, kind, alpha, current);
    };
    const auto address = [&](unsigned lane, SourcePmdLocation location) { return bank.encode(lane, location); };
    SourcePmdLocation transformed;
    if (!clipped && !alpha)
        transformed = transformed_pointer(bank, frame, 0x42a1ac); // before header store
    store(0, address(0, header), pc[0], WorldPacketWriteKind::header);
    if (!clipped && alpha)
        transformed = transformed_pointer(bank, frame, 0x42a18d); // after header, before indices
    store(1, address(1, indices), pc[1], WorldPacketWriteKind::indices);
    if (clipped)
        transformed = transformed_pointer(bank, frame, alpha ? 0x42a008u : 0x42a02du);
    store(3, address(2, transformed), pc[2], WorldPacketWriteKind::vertices);
    store(2, address(3, attributes), pc[3], WorldPacketWriteKind::attributes);
    if (clipped)
        store(4, common, pc[4], WorldPacketWriteKind::inside_mask);
    // Inside retains DWORD+10. Register advance never publishes a global cursor.
    current.advance();
}
void emit_clipped_polygon(Access &bank, WorldPolygonFrame &frame, const std::shared_ptr<RaceSessionPlatform> &platform,
                          detail::PacketEmissionCursors &cursors, const CapturedFields &fields,
                          SourcePmdLocation headers, std::uint16_t polygon)
{
    const auto header = offset(headers, std::uint32_t(polygon) * 8u, 0x429f23);
    const auto first = read<std::uint16_t>(bank, offset(header, 6, 0x429f2b), 0x429f2b);
    const auto indices = offset(fields.indices, std::uint32_t(first) * 2u, 0x429f2f);
    const auto count = read<std::uint32_t>(bank, header, 0x429f32) & 0xffu;
    if (count == 0)
        return; // no corner, transformed-mask, authored-vertex or normal reads
    std::uint32_t common = 0x3f, any = 0;
    auto corner = indices;
    SourcePmdLocation last_attribute;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const auto index = read<std::uint16_t>(bank, corner, 0x429f55);
        const auto transformed = transformed_pointer(bank, frame, 0x429f58); // fresh61C510 every corner
        last_attribute = offset(fields.attributes, std::uint32_t(index) * 16u, 0x429f61);
        corner = offset(corner, 2, 0x429f65);
        const auto vertex = read<std::uint16_t>(bank, offset(last_attribute, 8, 0x429f68), 0x429f68);
        const auto mask =
            read<std::uint16_t>(bank, offset(transformed, std::uint32_t(vertex) * 32u + 16u, 0x429f6f), 0x429f73);
        any |= mask;
        common &= mask;
    }
    if (any != 0x3f)
        return;
    // 429F96 rereads the last coordinate's vertex WORD; do not use the first
    // corner or silently retain the mask loop's already-read value.
    const auto vertex = read<std::uint16_t>(bank, offset(last_attribute, 8, 0x429f96), 0x429f96);
    const auto flags = read<std::uint32_t>(bank, header, 0x429fa2);
    if (!facing(bank, frame, fields.vertices, fields.normals, header, vertex, flags, true))
        return;
    emit_record(bank, frame, platform, cursors, header, indices, fields.attributes, flags, common, true);
}
void emit_inside_polygon(Access &bank, WorldPolygonFrame &frame, const std::shared_ptr<RaceSessionPlatform> &platform,
                         detail::PacketEmissionCursors &cursors, const CapturedFields &fields,
                         SourcePmdLocation headers, std::uint16_t polygon)
{
    const auto header = offset(headers, std::uint32_t(polygon) * 8u, 0x42a110);
    const auto first = read<std::uint16_t>(bank, offset(header, 6, 0x42a115), 0x42a115);
    const auto indices = offset(fields.indices, std::uint32_t(first) * 2u, 0x42a119);
    // Inside unconditionally follows the first corner, even if header byte0==0.
    const auto coordinate = read<std::uint16_t>(bank, indices, 0x42a11e);
    const auto attribute = offset(fields.attributes, std::uint32_t(coordinate) * 16u, 0x42a124);
    const auto vertex = read<std::uint16_t>(bank, offset(attribute, 8, 0x42a126), 0x42a126);
    const auto flags = read<std::uint32_t>(bank, header, 0x42a132);
    if (!facing(bank, frame, fields.vertices, fields.normals, header, vertex, flags, false))
        return;
    emit_record(bank, frame, platform, cursors, header, indices, fields.attributes, flags, 0, false);
}
template <bool clipped>
void emit_batches(SourceAllocationBank &source_bank, WorldPolygonFrame &frame, const WorldSpatialRecords &records,
                  std::uint32_t count, WorldPacketEmissionObserver *observer)
{
    SGN_PROFILE_SCOPE(clipped ? "emit.clipped_429ea0" : "emit.inside_42a090"); // NEW HOST (R30) marker
    const auto platform = admit_records(source_bank, frame, records, clipped ? 0x429ea0u : 0x42a090u);
    Access bank(source_bank); // NEW HOST (R30): after the unchanged admission, before the first bank read
    constexpr unsigned pair = clipped ? 0u : 2u;
    // Read-only capture of the existing cursors. Their shared helper publishes
    // alpha then opaque for429EA0, opaque then alpha for42A090 (also count0).
    detail::PacketEmissionCursors cursors(frame.packets[pair], frame.packets[pair + 1], observer);
    CapturedFields fields;
    std::uint32_t old_count;
    if constexpr (clipped)
    {
        fields = capture_clipped(bank, frame);
        old_count = frame.spatial_batch_count; // 429ED3
    }
    else
    {
        old_count = frame.spatial_batch_count;
        fields = capture_inside(bank, frame); // 42A09A, before PMD reads
    }
    frame.spatial_batch_count = old_count + count; // 429EEA /42A0D9, wrapping; also count0
    for (std::uint32_t row = 0; row < count; ++row)
    {
        auto list = polygon_list(bank, records, row); // one raw lane0 and one decode per batch
        auto polygon = read<std::uint16_t>(bank, list, clipped ? 0x429f01u : 0x42a0f0u);
        if (polygon == 0xffffu)
            continue; // no header+34 read and no61C530 reload
        const auto part = fields.part.source_part().location();
        const auto headers = pointed_word(bank, part, 0x34, clipped ? 0x429f0eu : 0x42a0fdu);
        do
        {
            if constexpr (clipped)
                emit_clipped_polygon(bank, frame, platform, cursors, fields, headers, polygon);
            else
                emit_inside_polygon(bank, frame, platform, cursors, fields, headers, polygon);
            list = offset(list, 2, clipped ? 0x42a048u : 0x42a1cau);
            polygon = read<std::uint16_t>(bank, list, clipped ? 0x42a04eu : 0x42a1d0u);
        } while (polygon != 0xffffu);
        // Only the held part register is refreshed. Initial normals/indices,
        // authored vertices and coordinates stay captured across all batches.
        fields.part = frame.active_track_base_part; // 42A05B /42A1DD
    }
    cursors.publish(clipped ? detail::PacketEmissionPath::spatial_clipped : detail::PacketEmissionPath::spatial_inside,
                    count == 0);
}
} // namespace
void emit_source_world_spatial_clipped_429ea0(SourceAllocationBank &bank, WorldPolygonFrame &frame,
                                              const WorldSpatialRecords &records, std::uint32_t count,
                                              WorldPacketEmissionObserver *observer)
{
    emit_batches<true>(bank, frame, records, count, observer);
}
void emit_source_world_spatial_inside_42a090(SourceAllocationBank &bank, WorldPolygonFrame &frame,
                                             const WorldSpatialRecords &records, std::uint32_t count,
                                             WorldPacketEmissionObserver *observer)
{
    emit_batches<false>(bank, frame, records, count, observer);
}
void emit_source_world_spatial_polygons_42a220(SourceAllocationBank &bank, WorldPolygonFrame &frame,
                                               WorldPacketEmissionObserver *observer)
{
    frame.indexed.require_owner_thread();
    const auto clipped_count = frame.spatial[0].count;
    const auto clipped_base = frame.spatial[0].records; // metadata-only capture of61C518
    emit_source_world_spatial_clipped_429ea0(bank, frame, clipped_base, clipped_count, observer);
    const auto inside_count = frame.spatial[1].count; // fresh42A23B after first complete call
    const auto inside_base = frame.spatial[1].records;
    emit_source_world_spatial_inside_42a090(bank, frame, inside_base, inside_count, observer);
}
} // namespace sgn::retail
