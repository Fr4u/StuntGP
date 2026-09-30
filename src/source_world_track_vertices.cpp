#include "sgn/source_world_track_view.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/source_bank_read_memo.hpp"
#include "sgn/host_profiler.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

#include "sgn/detail/world_vertex_kernels.hpp"
#include "sgn/detail/pass_parallel.hpp"
#include <vector>

#ifdef SGN_WORLD_X87_REFERENCE // NEW diagnostic build: original x87 leaves
extern "C" void sgn_track_vertex_inside_42979a(const float *, const void *, const std::uint32_t *, void *) noexcept;
extern "C" void sgn_track_vertex_clipped_4298f1(const float *, const void *, const std::uint32_t *, void *) noexcept;
#endif

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
// NEW host performance (perf_world R19): offset-only forms of offset/read/
// reached over a SourceBankReadMemo. Same overflow/extent checks, sites and
// messages as the former SourcePmdLocation helpers; no per-field weak copies.
W advance(W at, W amount, W site)
{
    if (amount > std::numeric_limits<W>::max() - at)
        throw SourceAllocationError(site, "source track vertex pointer leaves the admitted offset domain");
    return at + amount;
}
std::span<const std::uint8_t> reached(SourceBankReadMemo &memo, const SourceAllocationRef &allocation, W at, W count,
                                      W site)
{
    const auto bytes = memo.whole(allocation); //== bank.bytes(allocation)
    if (at > bytes.size() || count > bytes.size() - at)
        throw SourceAllocationError(site, "source track vertex read leaves its actual allocation");
    return bytes.subspan(at, count);
}
template <class T> T read(SourceBankReadMemo &memo, const SourceAllocationRef &allocation, W at, W site)
{
    const auto bytes = reached(memo, allocation, at, static_cast<W>(sizeof(T)), site);
    T value;
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
}
template <bool clipped>
void transform(SourceAllocationBank &bank, WorldPolygonFrame &frame, const ProjectionParameters &projection)
{
    frame.indexed.require_owner_thread();
    // 429886 reads current-camera+198 even when the clipped list is empty.
    const W clip_near = clipped ? std::bit_cast<W>(projection.clip_near) : 0u;
    auto &list = frame.spatial[clipped ? 0u : 1u];
    if (!list.records.source_bound() || list.records.source_binding().bank != &bank)
        throw SourceAllocationError(clipped ? 0x429880 : 0x429730,
                                    "source spatial consumer requires its actual allocation bank");
    if (list.count == 0)
        return; // no part, stamp, indexed pointer or payload read
    SourceTrackPartPointer retained_part;
    WorldIndexedVertexPointer current;
    W stamp;
    if constexpr (clipped)
    {
        current = frame.indexed.current();
        stamp = frame.frame_stamp; // 4298AC/B3
    }
    else
    {
        retained_part = frame.active_track_base_part; // 42974D, captured without dereference
        stamp = frame.frame_stamp;
        current = frame.indexed.current(); // 429754/75B
    }
    // NEW host performance state for this synchronous traversal (perf_world
    // R19). Nothing below allocates/releases bank storage or stores opaque
    // transport words: the kernels, indexed appends and stamp stores are raw
    // payload stores. Each memo performs its full admission lazily at exactly
    // the first point the uncached code reached it, so failures are unchanged.
    SourceBankReadMemo memo(bank);
    WorldIndexedVertexPointer::SourceAppendRun appender(current);
    std::span<WorldPolygonVertex> output_vertices; // checked view of the61C50C allocation
    std::span<std::uint8_t> written;               // NEW repair (PORT-FIX-014): host marks of that allocation
    bool outputs_admitted = false;
    // 61C530's relocated part location is host metadata (relocation history and
    // allocation identity), invariant here: neither61C530 nor the retained copy
    // changes during the traversal. Its part+30 payload DWORD is still re-read
    // for every changed vertex, as before.
    std::optional<SourcePmdLocation> part_location;
    // Invariant kernel inputs: the caller's projection is not written here.
    const detail::WorldVertexProjection kernel{projection.x, projection.y, projection.z, projection.offset_z,
                                               std::bit_cast<float>(clip_near)};
#ifdef SGN_WORLD_X87_REFERENCE
    const std::array<W, 6> inputs{std::bit_cast<W>(projection.x),
                                  std::bit_cast<W>(projection.y),
                                  std::bit_cast<W>(projection.z),
                                  std::bit_cast<W>(projection.offset_z),
                                  clip_near,
                                  stamp};
#endif
#ifndef SGN_WORLD_X87_REFERENCE
    // NEW HOST (R33 perf-pass-parallel): the 4298F1 / 42979A kernel calls are deferred and then run on the
    // host pool (detail/pass_parallel.hpp). Everything else (admissions, stamp tests, PORT-FIX marks, indexed
    // appends, PMD reads) still runs here, serially and in the original order. Why this is exact:
    //  - a kernel reads only frame.model_view, `kernel` and its 12 PMD XYZ bytes, and writes only bytes
    //    0..31 of its own output vertex; each destination is transformed at most once per call (the stamp /
    //    written[] test below skips it afterwards), so the deferred kernels write disjoint vertices;
    //  - the only output bytes this loop reads back are the stamp WORD at +0x12 of later destinations. The
    //    inside path stores it here already (4297A6); for the clipped path the kernel's own stamp store
    //    (4299E5, the same value) is made here first, so every later stamp test reads what it read before;
    //  - the item lists, the part and the XYZ are read from PMD allocations; the output vertices are the
    //    transformed allocation (61C50C) and the indexed words (61C520) are their own 42E8D0 allocation
    //    (source_polygon_indexed.cpp), so no read here can observe a pending kernel write. If a PMD
    //    allocation ever IS the transformed allocation, the remaining vertices run immediately, in order;
    //  - every deferred kernel has run before the stores that follow the loop (publish_current), also when
    //    an admission throws (the kernels of the vertices before it run, then the exception continues).
    struct Deferred
    {
        const std::uint8_t *xyz;
        WorldPolygonVertex *output;
    };
    thread_local std::vector<Deferred> deferred; // capacity kept across passes; cleared at entry
    deferred.clear();
    bool defer = true;
    const auto run_kernel = [&](const std::uint8_t *xyz, WorldPolygonVertex *output) noexcept {
        if constexpr (clipped)
            detail::world_track_vertex_clipped_4298f1(frame.model_view.model_view.data(), xyz, kernel, stamp, output);
        else
            detail::world_track_vertex_inside_42979a(frame.model_view.model_view.data(), xyz, kernel, output);
    };
    const auto run_deferred = [&]() noexcept {
        const auto count = static_cast<std::uint32_t>(deferred.size());
        if (count == 0)
            return;
        SGN_PROFILE_SCOPE(clipped ? "track.transform_clipped_429880_parallel"
                                  : "track.transform_inside_429730_parallel");
        if (sgn::host_profile::enabled())
            sgn::host_profile::sample(
                clipped ? "track.transform_clipped_429880_elements" : "track.transform_inside_429730_elements", count);
        const auto *jobs = deferred.data();
        detail::pass_parallel_for(count, [&](std::uint32_t first, std::uint32_t last) noexcept {
            for (auto i = first; i < last; ++i)
                run_kernel(jobs[i].xyz, jobs[i].output);
        });
        deferred.clear();
    };
    try
    {
#endif
        W row = 0;
        do
        {
            const auto decoded = bank.decode_address(list.records.source_word(row, 1));
            const auto &item_allocation = decoded.allocation;
            W item = decoded.offset;
            auto index = read<std::uint16_t>(memo, item_allocation, item, clipped ? 0x4298bc : 0x429764);
            while (index != 0xffffu)
            {
                if (!outputs_admitted)
                {
                    const auto &vertices = frame.transformed.allocation;
                    if (!vertices.source_bound() || vertices.source_binding().bank != &bank)
                        throw SourceAllocationError(clipped ? 0x4298ca : 0x429772,
                                                    "track transform requires actual source transformed storage");
                    output_vertices = vertices.view();
                    written =
                        frame.track_vertex_writes.marks(vertices.source_binding().allocation, output_vertices.size());
                    outputs_admitted = true;
                }
                // 61C510 is independent of allocation61C50C and is read per vertex.
                const auto destination = std::size_t(frame.transformed.count) + index;
                if (destination >= output_vertices.size()) //== SourceWorldSpan::operator[]
                    throw SourceAllocationError(0x42e430, "source world buffer index outside owner");
                auto &output = output_vertices[destination];
                std::uint16_t previous;
                std::memcpy(&previous, reinterpret_cast<const std::uint8_t *>(&output) + 0x12, 2);
                // NEW repair (PORT-FIX-014): a slot never written since the 42E8D0
                // zero fill is not "already transformed this frame" when its zero
                // stamp equals 61C508. Every written slot keeps the source test.
                if (W(previous) != stamp || written[destination] == 0)
                { // whole32-bit frame comparison, not truncated
                    if (W(previous) == stamp && sgn::host_profile::enabled())
                        sgn::host_profile::add("world.portfix014_unwritten_track_vertex", 0); // NEW HOST counter
                    // Clipped:4298E9 fresh61C530 per changed vertex (read in place,
                    // the owner is unchanged); inside: the42974D retained copy.
                    const SourceTrackPartPointer &part = clipped ? frame.active_track_base_part : retained_part;
                    if (!current.is_null() && !current.source_bound())
                        throw SourceAllocationError(clipped ? 0x4298f7 : 0x429792,
                                                    "source indexed cursor cannot borrow native storage");
                    appender.append(&output); // actual DWORD store; global61C520 still retained
                    const auto source_index =
                        read<std::uint16_t>(memo, item_allocation, item, clipped ? 0x4298f9 : 0x429794);
                    // Source part+30 is a relocated PMD-relative offset, NOT an
                    // allocator transport token. No vertex-count/header/W read.
                    if (!part_location)
                        part_location = part.source_part().location();
                    const auto &part_allocation = part_location->allocation;
                    const auto vertex_base = read<W>(
                        memo, part_allocation, advance(part_location->offset, 0x30, clipped ? 0x429902 : 0x4297a3),
                        clipped ? 0x429902 : 0x4297a3);
                    if (vertex_base == 0)
                        throw SourceAllocationError(clipped ? 0x429902 : 0x4297a3,
                                                    "null relocated PMD vertex pointer is outside the host domain");
                    const auto point = advance(vertex_base, W(source_index) * 16u, clipped ? 0x429902 : 0x4297a3);
                    if constexpr (!clipped)
                    {
                        const auto low = static_cast<std::uint16_t>(stamp);
                        std::memcpy(reinterpret_cast<std::uint8_t *>(&output) + 0x12, &low, 2); // 4297A6
                    }
                    // NEW host admission for a synchronous12-byte XYZ borrow.
                    // The original's possible x87 partial-fault stack is not emulated.
                    const auto xyz = reached(memo, part_allocation, point, 12, clipped ? 0x429905 : 0x4297aa);
#ifdef SGN_WORLD_X87_REFERENCE
                    if constexpr (clipped)
                        sgn_track_vertex_clipped_4298f1(frame.model_view.model_view.data(), xyz.data(), inputs.data(),
                                                        &output);
                    else
                        sgn_track_vertex_inside_42979a(frame.model_view.model_view.data(), xyz.data(), inputs.data(),
                                                       &output);
#else
                // Plain C++ of4298F1 /42979A (see detail/world_vertex_kernels.hpp), deferred (NEW HOST R33).
                if (defer && (item_allocation == frame.transformed.allocation.source_binding().allocation ||
                              part_allocation == frame.transformed.allocation.source_binding().allocation))
                {
                    run_deferred(); // a PMD read may alias the outputs: keep strict order from here on
                    defer = false;
                }
                bool queued = false;
                if (defer)
                {
                    try
                    {
                        deferred.push_back({xyz.data(), &output});
                        queued = true;
                    }
                    catch (...)
                    {
                        run_deferred(); // no host memory for the queue: continue in strict order
                        defer = false;
                    }
                }
                if (queued)
                {
                    if constexpr (clipped)
                        detail::world_store_u16(&output, 18, static_cast<std::uint16_t>(stamp)); // 4299E5 value
                }
                else
                    run_kernel(xyz.data(), &output);
#endif
                    written[destination] = 1; // stamp WORD now stored (4297A6 / 4299E5)
                }
                const auto next = advance(item, 2, clipped ? 0x429a0a : 0x42983f);
                index = read<std::uint16_t>(memo, item_allocation, next, clipped ? 0x429a0a : 0x42983f);
                item = next;
            }
            ++row;
        } while (row < list.count); // 429854/429A1C reread end cursor after each row
#ifndef SGN_WORLD_X87_REFERENCE
    }
    catch (...)
    {
        run_deferred(); // the vertices admitted before the failure are transformed, as before
        throw;
    }
    run_deferred();
#endif
    frame.indexed.publish_current(std::move(current)); // 429868/429A30 only on full return
}
} // namespace
void SourceWorldTrackContext::transform_spatial_vertices_clipped_429880(const ProjectionParameters &actual_projection,
                                                                        WorldPolygonFrame &frame) const
{
    transform<true>(spatial_bank(), frame, actual_projection);
}
void SourceWorldTrackContext::transform_spatial_vertices_inside_429730(const ProjectionParameters &actual_projection,
                                                                       WorldPolygonFrame &frame) const
{
    transform<false>(spatial_bank(), frame, actual_projection);
}
} // namespace sgn::retail
