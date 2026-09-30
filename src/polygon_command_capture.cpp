#include "sgn/polygon_command_capture.hpp"
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace sgn
{
namespace
{
// NEW HOST (R30 perf-tick) storage recycling, not recovered behaviour. A sealed frame used to own
// freshly allocated storage: its command vector grew by reallocation every frame and every draw
// allocated its own vertex and index vectors (hundreds of heap allocations per 60 Hz pass, freed
// again when the frame retired). The capture now takes EMPTY vectors whose capacity survived an
// earlier retired frame and copies the same elements into them; a retired frame gives its vectors
// back (cleared, capacity kept). Only capacities and heap traffic change: every sealed frame holds
// exactly the same commands, vertices, indices, payload count and GPU insertions as before.
// Frames can retire on any thread, so the pool is mutex-protected (uncontended in practice).
using Vertices = std::vector<retail::PolygonSubmissionVertex>;
using Indices = std::vector<std::uint16_t>;
using Commands = std::vector<CapturedPolygonCommand>;
using Insertions = std::vector<PolygonGpuInsertion>;
struct StoragePool
{
    // Bounds: a pathological frame cannot pin unbounded memory in the pool (at most max_bytes of idle
    // capacity; a real frame is limited to 20000 commands and 32 MiB of payload by its capture budget).
    static constexpr std::size_t max_frames = 4, max_spares = 16384, max_vertex_capacity = 65536,
                                 max_index_capacity = 196608, max_command_capacity = 65536,
                                 max_bytes = 96u * 1024u * 1024u;
    std::mutex mutex;
    std::size_t bytes{}; // idle capacity held in all four lists
    std::vector<Commands> commands;
    std::vector<Insertions> insertions;
    std::vector<Vertices> vertices; // LIFO: back() is the next draw's buffer
    std::vector<Indices> indices;
};
StoragePool &pool()
{
    static auto *storage = new StoragePool; // leaked on purpose: frames may retire during static destruction
    return *storage;
}
template <class Vector> std::size_t capacity_bytes(const Vector &v) noexcept
{
    return v.capacity() * sizeof(typename Vector::value_type);
}
// Both run with p.mutex held.
template <class Vector> Vector take(StoragePool &p, std::vector<Vector> &spares)
{
    Vector result;
    if (!spares.empty())
    {
        result = std::move(spares.back());
        spares.pop_back();
        p.bytes -= capacity_bytes(result);
    }
    return result; // empty; any capacity is reused by the caller's assign()
}
template <class Vector>
void keep(StoragePool &p, std::vector<Vector> &spares, Vector &v, std::size_t max_capacity, std::size_t max_count)
{
    const auto bytes = capacity_bytes(v);
    if (bytes == 0 || v.capacity() > max_capacity || spares.size() >= max_count ||
        bytes > StoragePool::max_bytes - p.bytes)
        return; // freed by the frame as before
    v.clear();
    spares.push_back(std::move(v)); // may throw bad_alloc: v is untouched then and freed normally
    p.bytes += bytes;
}
void recycle(CapturedPolygonFrame *frame) noexcept
{
    std::unique_ptr<CapturedPolygonFrame> owner(frame);
    try
    {
        auto &p = pool();
        {
            const std::lock_guard lock(p.mutex);
            // Reverse order, so the next capture pops them in this frame's draw order (similar sizes).
            for (auto it = frame->commands.rbegin(); it != frame->commands.rend(); ++it)
                if (auto *draw = std::get_if<CapturedPolygonDraw>(&*it))
                {
                    keep(p, p.vertices, draw->vertices, StoragePool::max_vertex_capacity, StoragePool::max_spares);
                    keep(p, p.indices, draw->indices, StoragePool::max_index_capacity, StoragePool::max_spares);
                }
        }
        // Outside the lock: GPU insertion commands release their shared owners here, as before.
        frame->commands.clear();
        frame->gpu_insertions.clear();
        const std::lock_guard lock(p.mutex);
        keep(p, p.commands, frame->commands, StoragePool::max_command_capacity, StoragePool::max_frames);
        keep(p, p.insertions, frame->gpu_insertions, StoragePool::max_command_capacity, StoragePool::max_frames);
    }
    catch (...)
    {
    }
}
// A pool failure only means "no recycled buffer": the caller then allocates exactly as before.
template <class Vector> Vector take_spare(std::vector<Vector> StoragePool::*spares, std::size_t count) noexcept
{
    if (count == 0)
        return {}; // an empty payload never allocated; do not bind a spare to it
    try
    {
        auto &p = pool();
        const std::lock_guard lock(p.mutex);
        return take(p, p.*spares);
    }
    catch (...)
    {
        return {};
    }
}
} // namespace
PolygonCommandCapture::PolygonCommandCapture(std::uint64_t tick, std::uint64_t epoch, PolygonCaptureLimits limits)
    : limits_(limits)
{
    if (epoch == 0 || limits.commands == 0)
        throw FormatError("polygon capture requires a resource epoch and command budget");
    frame_.source_tick = tick;
    frame_.resource_epoch = epoch;
    // NEW HOST (R30): empty command/insertion vectors with the capacity of a retired frame.
    frame_.commands = take_spare(&StoragePool::commands, 1);
    frame_.gpu_insertions = take_spare(&StoragePool::insertions, 1);
}
void PolygonCommandCapture::check_capacity(std::size_t payload) const
{
    if (sealed_)
        throw FormatError("polygon capture is already sealed");
    if (frame_.commands.size() >= limits_.commands || frame_.payload_bytes > limits_.payload_bytes ||
        payload > limits_.payload_bytes - frame_.payload_bytes)
        throw FormatError("polygon capture exceeds the configured host memory budget");
}
std::uint32_t PolygonCommandCapture::set_render_state(std::uint32_t state, std::uint32_t value)
{
    check_capacity(0);
    frame_.commands.emplace_back(CapturedRenderState{state, value});
    return 0;
}
std::uint32_t PolygonCommandCapture::set_texture(std::uint32_t stage, std::uint32_t token)
{
    check_capacity(0);
    frame_.commands.emplace_back(CapturedTexture{stage, token});
    return 0;
}
std::uint32_t PolygonCommandCapture::set_texture_stage_state(std::uint32_t stage, std::uint32_t state,
                                                             std::uint32_t value)
{
    check_capacity(0);
    frame_.commands.emplace_back(CapturedTextureStageState{stage, state, value});
    return 0;
}
std::uint32_t PolygonCommandCapture::append_draw(std::uint32_t type, std::uint32_t fvf,
                                                 std::span<const retail::PolygonSubmissionVertex> vertices,
                                                 std::span<const std::uint16_t> indices, bool indexed,
                                                 std::uint32_t flags, const retail::ViewportState &viewport,
                                                 bool indices_checked)
{
    if (vertices.size() > std::numeric_limits<std::size_t>::max() / sizeof(retail::PolygonSubmissionVertex) ||
        indices.size() > std::numeric_limits<std::size_t>::max() / sizeof(std::uint16_t))
        throw FormatError("polygon capture payload extent overflow");
    const auto vertex_bytes = vertices.size_bytes(), index_bytes = indices.size_bytes();
    if (vertex_bytes > std::numeric_limits<std::size_t>::max() - index_bytes)
        throw FormatError("polygon capture payload sum overflow");
    const auto payload = vertex_bytes + index_bytes;
    check_capacity(payload);
    if (!indices_checked)
        for (const auto index : indices)
            if (index >= vertices.size())
                throw FormatError("polygon capture index outside submitted vertices");
    // Preserve all integer and IEEE payload bits, including unused UV/color
    // fields. Format/topology translation belongs to the actual GPU adapter.
    // NEW HOST (R30): the same element copies, into recycled vectors (capacity from a retired frame).
    CapturedPolygonDraw command{type,
                                fvf,
                                flags,
                                indexed,
                                viewport,
                                take_spare(&StoragePool::vertices, vertices.size()),
                                take_spare(&StoragePool::indices, indices.size())};
    command.vertices.assign(vertices.begin(), vertices.end());
    command.indices.assign(indices.begin(), indices.end());
    frame_.commands.emplace_back(std::move(command));
    frame_.payload_bytes += payload;
    return 0;
}
std::uint32_t PolygonCommandCapture::draw_indexed(std::uint32_t type, std::uint32_t fvf,
                                                  std::span<const retail::PolygonSubmissionVertex> vertices,
                                                  std::span<const std::uint16_t> indices, std::uint32_t flags,
                                                  const retail::ViewportState &viewport)
{
    return append_draw(type, fvf, vertices, indices, true, flags, viewport);
}
std::uint32_t PolygonCommandCapture::draw(std::uint32_t type, std::uint32_t fvf,
                                          std::span<const retail::PolygonSubmissionVertex> vertices,
                                          std::uint32_t flags, const retail::ViewportState &viewport)
{
    return append_draw(type, fvf, vertices, {}, false, flags, viewport);
}
std::uint32_t PolygonCommandCapture::append_prevalidated_draw(std::uint32_t type, std::uint32_t fvf,
                                                              std::span<const retail::PolygonSubmissionVertex> vertices,
                                                              std::span<const std::uint16_t> indices, bool indexed,
                                                              std::uint32_t flags,
                                                              const retail::ViewportState &viewport)
{
    return append_draw(type, fvf, vertices, indices, indexed, flags, viewport, true);
}
void PolygonCommandCapture::insert_gpu_command(std::shared_ptr<const PolygonGpuCommand> command,
                                               std::optional<PolygonGpuDrawRequest> draw)
{
    if (sealed_)
        throw FormatError("polygon capture is already sealed");
    if (!command)
        throw FormatError("polygon GPU insertion requires a command");
    frame_.gpu_insertions.push_back({frame_.commands.size(), std::move(command), std::move(draw)});
}
ImmutablePolygonFrame PolygonCommandCapture::seal()
{
    if (sealed_)
        throw FormatError("polygon capture is already sealed");
    // NEW HOST (R30): the sealed frame returns its storage to the pool when it retires. Both allocations
    // precede the move, so a failed seal still leaves the capture (and its accepted prefix) intact.
    auto *storage = new CapturedPolygonFrame;
    ImmutablePolygonFrame result(storage, recycle); // on bad_alloc the deleter already disposed of storage
    *storage = std::move(frame_);
    sealed_ = true;
    return result;
}
std::shared_ptr<CapturedPolygonFrame> copy_polygon_frame_recycled(const CapturedPolygonFrame &source)
{
    // Same member order and values as the implicit copy constructor of CapturedPolygonFrame
    // {source_tick, resource_epoch, commands, payload_bytes, gpu_insertions}; draws copy
    // {type, fvf, flags, indexed, viewport, vertices, indices}. Keep in step with that struct.
    auto *storage = new CapturedPolygonFrame;
    std::shared_ptr<CapturedPolygonFrame> result(storage, recycle); // bad_alloc: storage already disposed of
    storage->source_tick = source.source_tick;
    storage->resource_epoch = source.resource_epoch;
    storage->payload_bytes = source.payload_bytes;
    storage->commands = take_spare(&StoragePool::commands, 1);
    storage->commands.reserve(source.commands.size());
    for (const auto &command : source.commands)
    {
        const auto *draw = std::get_if<CapturedPolygonDraw>(&command);
        if (!draw)
        {
            storage->commands.push_back(command);
            continue;
        }
        CapturedPolygonDraw copy{draw->type,
                                 draw->fvf,
                                 draw->flags,
                                 draw->indexed,
                                 draw->viewport,
                                 take_spare(&StoragePool::vertices, draw->vertices.size()),
                                 take_spare(&StoragePool::indices, draw->indices.size())};
        copy.vertices.assign(draw->vertices.begin(), draw->vertices.end());
        copy.indices.assign(draw->indices.begin(), draw->indices.end());
        storage->commands.emplace_back(std::move(copy));
    }
    storage->gpu_insertions = take_spare(&StoragePool::insertions, source.gpu_insertions.size());
    storage->gpu_insertions.assign(source.gpu_insertions.begin(), source.gpu_insertions.end());
    return result;
}
PolygonReplayError::PolygonReplayError(std::size_t index, std::uint32_t hr)
    : std::runtime_error("polygon backend failed at command " + std::to_string(index) + " with HRESULT " +
                         std::to_string(hr)),
      command(index), result(hr)
{
}
void replay_polygon_commands(const CapturedPolygonFrame &frame, std::uint64_t epoch,
                             retail::PolygonSubmissionSink &sink)
{
    if (epoch == 0 || epoch != frame.resource_epoch)
        throw FormatError("polygon presentation resource epoch is no longer bound");
    for (std::size_t i = 0; i < frame.commands.size(); ++i)
    {
        const auto result = std::visit(
            [&](const auto &command) -> std::uint32_t {
                using T = std::decay_t<decltype(command)>;
                if constexpr (std::is_same_v<T, CapturedRenderState>)
                    return sink.set_render_state(command.state, command.value);
                else if constexpr (std::is_same_v<T, CapturedTexture>)
                    return sink.set_texture(command.stage, command.token);
                else if constexpr (std::is_same_v<T, CapturedTextureStageState>)
                    return sink.set_texture_stage_state(command.stage, command.state, command.value);
                else if (command.indexed)
                    return sink.draw_indexed(command.type, command.fvf, command.vertices, command.indices,
                                             command.flags, command.viewport);
                else
                    return sink.draw(command.type, command.fvf, command.vertices, command.flags, command.viewport);
            },
            frame.commands[i]);
        if ((result & 0x80000000u) != 0)
            throw PolygonReplayError(i, result);
    }
}
} // namespace sgn
