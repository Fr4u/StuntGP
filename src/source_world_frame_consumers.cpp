#include "sgn/source_world_frame_consumers.hpp"
#include "sgn/host_presentation.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_wide_race_view.hpp"
#include "sgn/source_draw_pose.hpp"
#include "sgn/source_world_frame_orchestration.hpp"
#include "sgn/source_world_lighting.hpp"
#include "sgn/race_session.hpp"
#include "sgn/detail/world_polygon_emission.hpp"
#include "sgn/retail_world_model_visibility.hpp"
#include "sgn/source_bank_read_memo.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

#include "sgn/detail/world_vertex_kernels.hpp"
#include "sgn/detail/pass_parallel.hpp"
#include <vector>

#ifdef SGN_WORLD_X87_REFERENCE // NEW diagnostic build: original x87 leaves
extern "C" void sgn_source_part_vertex_clipped_429544(const float *, const void *, const float *, void *) noexcept;
extern "C" void sgn_source_part_vertex_inside_429673(const float *, const void *, const float *, void *) noexcept;
extern "C" std::uint32_t sgn_source_model_light_dot(const void *, const void *, std::uint32_t) noexcept;
extern "C" std::uint32_t sgn_source_model_light_ftol(std::uint32_t) noexcept;
extern "C" std::uint32_t sgn_source_model_light_point(const void *, const void *) noexcept;
#endif

extern "C" void sgn_source_model_light_basis_44efd0(float *, const float *, const float *) noexcept;
extern "C" void sgn_source_model_light_camera_42b406(const float *, float *) noexcept;
extern "C" void sgn_source_model_light_position_42b462(const void *, const void *, float *, std::uint32_t *) noexcept;
extern "C" void sgn_source_model_light_record_42b499(void *, void *, const float *, const float *,
                                                     std::uint32_t *) noexcept;
namespace
{
// Per-coordinate lighting leaves: plain C++ by default (detail header),
// original x87 leaves in the SGN_WORLD_X87_REFERENCE diagnostic build.
std::uint32_t light_dot(const void *prepared, const void *normal, std::uint32_t specular) noexcept
{
#ifdef SGN_WORLD_X87_REFERENCE
    return sgn_source_model_light_dot(prepared, normal, specular);
#else
    (void)specular; // operand order of the exact binary32 product X*X' only
    return sgn::retail::detail::world_light_dot(prepared, normal);
#endif
}
std::uint32_t light_ftol(std::uint32_t bits) noexcept
{
#ifdef SGN_WORLD_X87_REFERENCE
    return sgn_source_model_light_ftol(bits);
#else
    return sgn::retail::detail::world_light_ftol(bits);
#endif
}
std::uint32_t light_point(const void *light, const void *vertex) noexcept
{
#ifdef SGN_WORLD_X87_REFERENCE
    return sgn_source_model_light_point(light, vertex);
#else
    return sgn::retail::detail::world_light_point(light, vertex);
#endif
}
} // namespace

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
W add(W base, std::uint64_t delta)
{
    if (delta > std::numeric_limits<W>::max() - base)
        throw SourceAllocationError(0x42a260, "source offset overflow");
    return base + static_cast<W>(delta);
}
const SourceAllocationRef &null_allocation() noexcept
{
    static const SourceAllocationRef value{};
    return value;
}
// NEW host performance (perf_world R19). One Context is one synchronous
// producer invocation (42AF30/42AF90/42AD30/42ADD0/42A260 or the42B040 prefix).
// Within it nothing allocates or releases bank storage or stores an opaque
// transport word: reached stores are write_known (which only removes opaque
// metadata), raw kernel/light/packet payload stores and token encodes. The
// memos below therefore return exactly what the uncached calls returned; each
// performs the full uncached admission the first time it is reached.
struct Context
{
    const SceneResources &scene;
    const WorldModelDrawState &draws;
    const SourceSpritePacketStorage &sprites;
    std::shared_ptr<RaceSessionPlatform> platform;
    SourceAllocationBank &bank;
    mutable SourceBankReadMemo memo{bank};
    // decode_address(token) memo: tokens are never recycled and the reached
    // allocations are neither released nor reallocated in this invocation.
    mutable std::array<std::pair<W, SourceAddressLocation>, 8> decoded{};
    mutable unsigned decoded_used{}, decoded_next{};
    // Bindings already admitted by transformed()/prepared_bytes(). The frame
    // and light-buffer binding objects are not reassigned in this invocation.
    mutable const SourceWorldBufferBinding *admitted_transformed{};
    mutable const SourceWorldBufferBinding *admitted_prepared{};
    // source_light_record(source_light_pointer_from_token(token)) memo.
    mutable std::array<std::pair<W, std::span<std::uint8_t>>, 16> lights{};
    mutable unsigned lights_used{};
    static SourceAllocationBank &admit(const SceneResources &scene, const WorldModelDrawState &draws,
                                       const SourceSpritePacketStorage &sprites)
    {
        scene.require_owner_thread();
        scene.require_sprite_binding(draws, sprites);
        const auto &binding = sprites.source_descriptor.source_binding();
        if (binding.platform.expired() || !binding.bank)
            throw SourceAllocationError(0x42af30, "expired source scene binding");
        return *binding.bank;
    }
    Context(const SceneResources &s, const WorldModelDrawState &d, const SourceSpritePacketStorage &p)
        : scene(s), draws(d), sprites(p), platform(p.source_descriptor.source_binding().platform.lock()),
          bank(admit(s, d, p))
    {
    }
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    std::span<std::uint8_t> bytes(const SourceAllocationRef &allocation, W offset, W count) const
    {
        return memo.readable(allocation, offset, count, 0x42a260); //== bank.readable_bytes(...,0x42a260)
    }
    std::span<std::uint8_t> bytes(const SourcePmdLocation &at, W count) const
    {
        return bytes(at.allocation, at.offset, count);
    }
    template <class T> T read_at(const SourceAllocationRef &allocation, W offset) const
    {
        const auto b = bytes(allocation, offset, static_cast<W>(sizeof(T)));
        T v;
        std::memcpy(&v, b.data(), sizeof(T));
        return v;
    }
    template <class T> T read(const SourcePmdLocation &at, W offset = 0) const
    {
        return read_at<T>(at.allocation, add(at.offset, offset));
    }
    template <class T> void write_at(const SourceAllocationRef &allocation, W offset, T v) const
    {
        bank.write_known(allocation, offset, {reinterpret_cast<const std::uint8_t *>(&v), sizeof(T)}, 0x42a260);
    }
    template <class T> void write(const SourcePmdLocation &at, W offset, T v) const
    {
        write_at(at.allocation, add(at.offset, offset), v);
    }
    // == bank.decode_address(token); the reference is valid until the next decode().
    const SourceAddressLocation &decode(W token) const
    {
        for (unsigned i = 0; i < decoded_used; ++i)
            if (decoded[i].first == token)
                return decoded[i].second;
        auto location = bank.decode_address(token);
        auto &entry =
            decoded_used < decoded.size() ? decoded[decoded_used++] : decoded[(decoded_next++) % decoded.size()];
        entry.first = token;
        entry.second = std::move(location);
        return entry.second;
    }
    // == former transformed(): 61C510 capture after one binding admission.
    W transformed_offset(const WorldTransformedVertexCursor &cursor, const SourceAllocationRef *&allocation) const
    {
        const auto &b = cursor.allocation.source_binding();
        if (&b != admitted_transformed)
        {
            if (b.bank != &bank || b.platform.lock() != platform)
                throw SourceAllocationError(0x429520, "foreign source transformed owner");
            admitted_transformed = &b;
        }
        allocation = &b.allocation;
        return add(b.byte_offset, std::uint64_t(cursor.count) * 32);
    }
    // == source_light_record(platform,source_light_pointer_from_token(platform,token)).
    std::span<std::uint8_t> light(W token) const
    {
        for (unsigned i = 0; i < lights_used; ++i)
            if (lights[i].first == token)
                return lights[i].second;
        const auto record = source_light_record(platform, source_light_pointer_from_token(platform, token));
        if (lights_used < lights.size())
            lights[lights_used++] = {token, record};
        return record;
    }
};
// Source PMD fields use normalized file offsets; generated fields use the
// existing allocation token codec. Decode only when the pointer is reached.
// NEW host performance: the PMD allocation identity is borrowed from the
// owning Model (which outlives every Pointer made from it) instead of being
// copied into each pointer value.
struct Pointer
{
    const Context *context{};
    const SourceAllocationRef *relative_file{};
    W raw{}, delta{};
    bool relative() const noexcept
    {
        return relative_file && static_cast<bool>(*relative_file);
    }
    Pointer plus(std::uint64_t n) const
    {
        auto p = *this;
        p.delta = add(p.delta, n);
        return p;
    }
    // Calls f(allocation,offset) with exactly the former location() result.
    template <class F> decltype(auto) at(F &&f) const
    {
        if (relative())
            return raw ? f(*relative_file, add(raw, delta)) : f(null_allocation(), delta);
        const auto &decoded = context->decode(raw);
        return f(decoded.allocation, add(decoded.offset, delta));
    }
    SourcePmdLocation location() const
    {
        return at(
            [](const SourceAllocationRef &allocation, W offset) { return SourcePmdLocation{allocation, offset}; });
    }
    template <class T> T read() const
    {
        if (!relative() && raw == 0x570ad8)
        {
            const auto &normals = context->platform->state().strip.normals;
            if (delta > sizeof(normals) || sizeof(T) > sizeof(normals) - delta)
                throw SourceAllocationError(0x429b70, "static strip normal extent");
            T value;
            std::memcpy(&value, reinterpret_cast<const std::uint8_t *>(normals.data()) + delta, sizeof(T));
            return value;
        }
        // 434090's sole static normal and bounds table are actual mutable owners.
        if (!relative() && raw == 0x4788b0)
        {
            if (delta > sizeof(context->draws.sprite_source.normals) ||
                sizeof(T) > sizeof(context->draws.sprite_source.normals) - delta)
                throw SourceAllocationError(0x429b70, "static sprite normal extent");
            T v;
            std::memcpy(&v, reinterpret_cast<const std::uint8_t *>(context->draws.sprite_source.normals.data()) + delta,
                        sizeof(T));
            return v;
        }
        return at(
            [&](const SourceAllocationRef &allocation, W offset) { return context->read_at<T>(allocation, offset); });
    }
    std::span<std::uint8_t> bytes(W count) const
    { //== context->bytes(location(),count)
        return at(
            [&](const SourceAllocationRef &allocation, W offset) { return context->bytes(allocation, offset, count); });
    }
    template <class T> void write(W offset, T value) const
    { //== context->write(location(),offset,value)
        at([&](const SourceAllocationRef &allocation, W base) {
            context->write_at(allocation, add(base, offset), value);
        });
    }
    W token() const
    {
        return at([&](const SourceAllocationRef &allocation, W offset) {
            return context->bank.encode_address(allocation, offset);
        });
    }
};
// Pointer/extent projection only. Each numeric read checks exactly the reached
// field; neither a PMD snapshot nor a fabricated 80-byte native transform.
struct TransformFields
{
    const Context *context{};
    SourcePmdLocation location;
    std::span<const std::uint8_t> static_owner;
    std::span<const std::uint8_t> bytes(W offset, W size) const
    {
        if (context)
            return context->bytes(location.allocation, add(location.offset, offset), size);
        if (offset > static_owner.size() || size > static_owner.size() - offset)
            throw SourceAllocationError(0x42b2a0, "static transform field extent");
        return static_owner.subspan(offset, size);
    }
    template <class T> T read(W offset) const
    {
        T out;
        const auto b = bytes(offset, static_cast<W>(sizeof(T)));
        std::memcpy(&out, b.data(), sizeof(T));
        return out;
    }
    Vec3 xyz(W offset) const
    {
        return {read<float>(offset), read<float>(offset + 4), read<float>(offset + 8)};
    }
    W tail(unsigned i) const
    {
        return read<W>(0x40u + i * 4u);
    }
};
struct Model
{
    const Context &context;
    std::optional<SourcePmdModelView> pmd;
    std::optional<SourcePmdByteView> captured_pmd_descriptor;
    SourcePmdLocation descriptor;
    bool source_strip{};
    // NEW host performance: SourcePmdModelView::check() admits model metadata
    // lifetime and the actual allocation, both invariant in one invocation.
    mutable bool checked{};
    explicit Model(const Context &c) : context(c)
    {
        const auto &b = c.sprites.source_descriptor.source_binding();
        descriptor = {b.allocation, b.byte_offset};
    }
    Model(const Context &c, W token) : context(c), source_strip(true)
    {
        const auto captured = c.bank.decode_address(token);
        descriptor = {captured.allocation, captured.offset};
        // The token is the queue's actual captured descriptor, not current571314
        // or a later registry replacement. Bank generation protects old leases.
    }
    Model(const Context &c, SourcePmdModelView m)
        : context(c), pmd(m), captured_pmd_descriptor(m.descriptor()), descriptor(captured_pmd_descriptor->location())
    {
        (void)c.bank.allocation_size(descriptor.allocation); // actual same-bank identity admission
    }
    Model(const Context &c, const SourceTrackModelPointer &captured)
        : context(c), pmd(captured.source_model_owner()), captured_pmd_descriptor(captured.source_descriptor()),
          descriptor(captured_pmd_descriptor->location())
    {
        (void)c.bank.allocation_size(descriptor.allocation);
    }
    void check() const
    {
        if (pmd && !checked)
        {
            pmd->check();
            checked = true;
        }
    }
    template <class T> T read(W field) const
    {
        check();
        return context.read<T>(descriptor, field);
    }
    template <class T> void write(W field, T v) const
    {
        check();
        context.write(descriptor, field, v);
    }
    Pointer pointer(W raw) const
    {
        return {&context, pmd ? &descriptor.allocation : nullptr, raw, 0};
    }
    Pointer field_pointer(W field) const
    {
        return pointer(read<W>(field));
    }
    SourcePmdLocation part(Pointer captured) const
    {
        check();
        const auto at = captured.location();
        if (pmd)
        {
            if (at.allocation != descriptor.allocation)
                throw SourceAllocationError(0x42a280, "foreign PMD part");
            (void)pmd->captured_base_part_42b055(at.offset);
        }
        else if (source_strip)
        {
            (void)context.bytes(at, 64); // actual bank part; never a native strip span
        }
        else
        {
            const auto &b = context.sprites.source_part.source_binding();
            if (at.allocation != b.allocation || at.offset != b.byte_offset)
                throw SourceAllocationError(0x42a280, "generated sprite part leaves its actual single allocation");
            (void)context.bytes(at, 64);
        }
        return at;
    }
    TransformFields transform(const SourcePmdLocation &part) const
    {
        check();
        if (pmd)
            return {&context, pmd->captured_base_part_42b055(part.offset).transform().location(), {}};
        if (source_strip)
        {
            if (context.read<W>(part, 8) != 0x474820)
                throw SourceAllocationError(0x42b2a0, "unbound source strip transform");
            return {nullptr,
                    {},
                    {reinterpret_cast<const std::uint8_t *>(&context.platform->state().strip.part_transform), 80}};
        }
        if (context.read<W>(part, 8) != 0x4788c0)
            throw SourceAllocationError(0x42b2a0, "unbound generated part transform");
        return {nullptr, {}, {reinterpret_cast<const std::uint8_t *>(&context.draws.sprite_part_transform), 80}};
    }
    const ModelTransform &outer() const
    {
        return resolve_source_draw_pose(context.draws, read<W>(0xc));
    }
    W selected() const
    {
        const W selector = read<W>(0x5c);
        if (selector >= 4)
            throw SourceAllocationError(0x42a26e, "source descriptor selector outside four slots");
        return read<W>(0x48 + selector * 4);
    }
};
CameraVector vector(Vec3 v)
{
    return {v.x, v.y, v.z, 0};
}
void local_camera(const Model &model, const SourcePmdLocation &part, const CameraState &camera,
                  const ViewMatrixArena &arena, const CameraMatrix &identity, WorldModelViewScratch &scratch)
{
    const auto reference = model.read<W>(0xc);
    const auto count = model.read<W>(0x24);
    const auto &outer = resolve_source_draw_pose(model.context.draws, reference);
    const auto &inverse_outer = resolve_model_matrix(arena, identity, outer.tail[2]);
    const auto transform = model.transform(part);
    const auto position = transform.xyz(0);
    Vec3 pivot{};
    const CameraMatrix *inverse_part = nullptr;
    if (count > 1)
    {
        pivot = {model.context.read<float>(part, 0x10), model.context.read<float>(part, 0x14),
                 model.context.read<float>(part, 0x18)};
        inverse_part = &resolve_model_matrix(arena, identity, transform.tail(1));
    }
    world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                             {camera.position.x, camera.position.y, camera.position.z}, outer.position, inverse_outer,
                             position, pivot, inverse_part, count);
}
void model_view(const Model &model, const SourcePmdLocation *part, const CameraState &camera,
                const ViewMatrixArena &arena, const CameraMatrix &identity, CameraMatrix &destination)
{
    const auto reference = model.read<W>(0xc);
    if (&destination == &identity)
        throw FormatError("source model-view destination aliases shared identity");
    initialize_model_matrix_identity(destination); // 44FBE6 before count/part admission
    const auto count = model.read<W>(0x24);
    std::optional<CameraModelTransform> part_view;
    if (count > 1 && part)
    {
        const auto t = model.transform(*part);
        const float sz = t.read<float>(0x18), sy = t.read<float>(0x14), sx = t.read<float>(0x10);
        const float z = model.context.read<float>(*part, 0x18), y = model.context.read<float>(*part, 0x14),
                    x = model.context.read<float>(*part, 0x10);
        const Vec3 scale{sx, sy, sz};
        const auto &rotation = resolve_model_matrix(arena, identity, t.tail(0));
        const float pz = t.read<float>(8), py = t.read<float>(4), px = t.read<float>(0);
        part_view = CameraModelTransform{{px, py, pz, 0}, vector(scale), {x, y, z, 0}, rotation};
    }
    const auto &outer = resolve_source_draw_pose(model.context.draws, reference);
    const float z = model.read<float>(0x18), y = model.read<float>(0x14), x = model.read<float>(0x10);
    const CameraModelTransform outer_view{vector(outer.position),
                                          vector(outer.scale),
                                          {x, y, z, 0},
                                          resolve_model_matrix(arena, identity, outer.tail[1])};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
    destination = camera_model_view(bound_camera, outer_view, count, part_view ? &*part_view : nullptr);
}
Pointer bound_pointer(const Model &model, W table, W index)
{
    if (model.source_strip && table == 0x5712d8)
    {
        if (index != 0)
            throw SourceAllocationError(0x427b6b, "strip bound pointer table outside sole5712D8");
        const auto bound = model.context.platform->state().strip.bound_index;
        if (bound && *bound != 0)
            throw SourceAllocationError(0x427b6b, "strip bound pointer leaves5712E0 owner");
        return model.pointer(bound ? 0x5712e0u : 0u);
    }
    if (!model.pmd && table == 0x4788a8)
    {
        const auto &a = model.context.draws.sprite_source.bound_indices;
        if (index >= a.size())
            throw SourceAllocationError(0x427b6b, "sprite bound pointer table extent");
        const auto entry = a[index];
        return model.pointer(entry ? add(0x5da220, std::uint64_t(*entry) * 16) : 0);
    }
    return model.pointer(model.pointer(table).plus(std::uint64_t(index) * 4).read<W>());
}
float bound_component(const Model &model, Pointer p, W lane)
{
    if (model.source_strip && p.raw == 0x5712e0)
    {
        const auto at = add(p.delta, lane);
        const auto &strip = model.context.platform->state().strip;
        if (at % 4 || at > 12)
            throw SourceAllocationError(0x427b72, "strip bound vertex outside16-byte owner");
        return std::bit_cast<float>(at == 12 ? strip.retained_bound_tail : strip.texture_names[at / 4]);
    }
    if (!model.pmd && p.raw >= 0x5da220 && p.raw < 0x5da230)
    {
        const auto at = add(p.raw - 0x5da220, add(p.delta, lane));
        const auto &b = model.context.draws.sprite_source.bound_vertices;
        if (at > sizeof(b) || sizeof(float) > sizeof(b) - at)
            throw SourceAllocationError(0x427b72, "sprite bound vertex extent");
        float v;
        std::memcpy(&v, reinterpret_cast<const std::uint8_t *>(b.data()) + at, 4);
        return v;
    }
    return p.plus(lane).read<float>();
}
W classify_whole(const Model &model, const CameraState &camera, const CameraPresentationState &presentation,
                 const CameraLifecycle &lifecycle, const ViewMatrixArena &arena, const CameraMatrix &identity,
                 const ProjectionParameters &projection, CameraMatrix &bound, std::optional<W> part_index = {})
{
    auto selected = model.pointer(model.selected()); // 42804F: capture without part dereference
    std::optional<SourcePmdLocation> selected_part;
    if (part_index)
    {
        selected = selected.plus(std::uint64_t(*part_index) * 64);
        selected_part = model.part(selected);
    }
    const auto bounds = [&](W lane) {
        return selected_part ? model.context.read<std::uint16_t>(*selected_part, 0x20 + lane)
                             : model.read<std::uint16_t>(0x20 + lane);
    };
    const auto radius = bounds(0);
    if (radius)
    {
        const auto center_index = bounds(2);
        const auto table = model.read<W>(0x40);
        const auto outer_reference = model.read<W>(0xc);
        const auto center_pointer = bound_pointer(model, table, center_index); // whole branch never reads XYZ
        const auto &outer = resolve_source_draw_pose(model.context.draws, outer_reference);
        const auto part = model.part(selected);
        const auto t = model.transform(part);
        VehicleVector center{};
        Vec3 pivot{};
        const CameraMatrix *rotation = &identity;
        if (part_index)
        {
            pivot = {model.context.read<float>(part, 0x10), model.context.read<float>(part, 0x14),
                     model.context.read<float>(part, 0x18)};
            rotation = &resolve_model_matrix(arena, identity, t.tail(0));
            center = {bound_component(model, center_pointer, 0), bound_component(model, center_pointer, 4),
                      bound_component(model, center_pointer, 8), 0};
        }
        const WorldModelRadiusPartRead fields{&t, [](const void *owner, unsigned offset) {
                                                  return static_cast<const TransformFields *>(owner)->read<float>(
                                                      offset);
                                              }};
        return classify_world_model_radius_fields(radius, center, !part_index, outer, fields, pivot, *rotation,
                                                  {camera.position.x, camera.position.y, camera.position.z},
                                                  presentation.forward_axis, presentation.planes, lifecycle.clip_far);
    }
    if (bounds(2) == 0)
        return 2;
    model_view(model, selected_part ? &*selected_part : nullptr, camera, arena, identity, bound);
    const auto table = model.read<W>(0x40);
    const auto index = bounds(2);
    W common = 63, any = 0;
    for (W i = 0; i < 8; ++i)
    {
        const auto point = bound_pointer(model, table, add(index, i));
        const auto component = [&](unsigned c) {
            const double yz = double(bound_component(model, point, 4)) * bound[c + 4] +
                              double(bound_component(model, point, 8)) * bound[c + 8];
            return (yz + double(bound_component(model, point, 0)) * bound[c]) + bound[c + 12];
        };
        const double z = component(2), x = component(0) * projection.x;
        const float y = static_cast<float>(component(1) * projection.y);
        const double projected_z = double(projection.z) * z + projection.offset_z;
        const float negative_z = static_cast<float>(-z);
        W mask = 63;
        if (!(projected_z > lifecycle.clip_near))
            mask = 62;
        if (projected_z >= lifecycle.clip_far)
            mask &= ~2u;
        if (!(x > negative_z))
            mask &= ~4u;
        if (x >= z)
            mask &= ~8u;
        if (!(y > negative_z))
            mask &= ~16u;
        if (y >= z)
            mask &= ~32u;
        if (mask == 63 && common != mask)
            return 2;
        any |= mask;
        common &= mask;
    }
    return common == 63 ? 0u : any == 63 ? 2u : 1u;
}
// NEW R20 HOST (wide_3d): 428040 with the ORIGINAL 4:3 planes/projection when
//  a race view is widened. A model rejected by it (1) is visible only because of
//  Hor+; the caller then leaves the PMD LOD words +5C/+64 (428380) untouched so
//  every word the tick reads stays as in the 4:3 original. Reads only (plus the
//  same bound scratch the preceding classification already wrote).
bool visible_only_widened(const Model &model, const CameraState &camera, const CameraPresentationState &presentation,
                          const CameraLifecycle &lifecycle, const ViewMatrixArena &arena, const CameraMatrix &identity,
                          const ProjectionParameters &projection, CameraMatrix &bound)
{
    const auto *original = host_wide::race_original_camera();
    if (!original || !original->planes)
        return false;
    auto original_presentation = presentation;
    original_presentation.planes = *original->planes;
    auto original_projection = projection;
    original_projection.x = original->projection_x;
    original_projection.y = original->projection_y;
    return classify_whole(model, camera, original_presentation, lifecycle, arena, identity, original_projection,
                          bound) == 1;
}
void select_detail(const Model &model, const CameraState &camera, const CameraPresentationState &presentation,
                   const W &rate)
{
    const auto &outer = model.outer();
    const double x = double(outer.position.x) - camera.position.x, y = double(outer.position.y) - camera.position.y,
                 z = double(outer.position.z) - camera.position.z;
    const double distance =
        (((z * presentation.forward_axis.z + y * presentation.forward_axis.y) + x * presentation.forward_axis.x) *
         double(3.2f)) /
        camera.lens;
    model.write(0x64, model.read<W>(0x5c));
    const W selected = !(distance > 5000.0) ? (distance > 3000.0 ? 1u : 0u) : (distance > 10000.0 ? 3u : 2u);
    model.write(0x5c, selected);
    const auto captured_rate = rate;
    if (captured_rate < 25)
        model.write(0x5c, model.read<W>(0x5c) + 2u);
    else if (captured_rate < 50)
        model.write(0x5c, model.read<W>(0x5c) + 1u);
    if (model.read<W>(0x5c) > 3)
        model.write(0x5c, 3u);
}
void transform_vertices(const Model &model, const SourcePmdLocation &part, const ProjectionParameters &projection,
                        bool clipped, const WorldModelViewScratch &scratch, WorldTransformedVertexCursor &cursor)
{
    const auto source = model.pointer(model.context.read<W>(part, 0x30));
    const auto count = model.context.read<std::uint16_t>(part, 0x28);
    const SourceAllocationRef *destination_allocation{};
    const auto destination =
        model.context.transformed_offset(cursor, destination_allocation); // captured61C510, including count0
#ifdef SGN_WORLD_X87_REFERENCE
    const std::array<float, 5> values{projection.x, projection.y, projection.z, projection.offset_z,
                                      projection.clip_near};
#else
    const detail::WorldVertexProjection values{projection.x, projection.y, projection.z, projection.offset_z,
                                               projection.clip_near};
    // NEW HOST (R33 perf-pass-parallel): a large part runs its 429544 / 429673 kernels on the host pool.
    // Exactness: this loop's own work per vertex is two checked span borrows (no payload is read); the
    // kernel reads scratch.model_view, `values` and its 12 XYZ bytes and writes only its own 32-byte output
    // (destination + i*32: disjoint for different i). The borrows are made first, serially and in the
    // original order (so the first failing borrow throws at the same vertex, after the kernels of every
    // vertex before it have run, as before); then the kernels run. If any XYZ range overlaps the output
    // range, a kernel could read an earlier kernel's output, so the kernels then run serially in order.
    struct Job
    {
        const std::uint8_t *xyz;
        std::uint8_t *output;
    };
    thread_local std::vector<Job> jobs; // capacity kept across calls; cleared at entry
    bool queue = false;
    if (count >= detail::pass_parallel_minimum)
    {
        jobs.clear();
        try
        {
            jobs.reserve(count);
            queue = true;
        }
        catch (...)
        {
            // no host memory for the queue: the original loop below runs instead
        }
    }
    if (queue)
    {
        const auto run = [&](bool parallel) noexcept {
            const auto n = static_cast<std::uint32_t>(jobs.size());
            if (n == 0)
                return;
            const auto *items = jobs.data();
            const auto body = [&](std::uint32_t first, std::uint32_t last) noexcept {
                for (auto k = first; k < last; ++k)
                {
                    if (clipped)
                        detail::world_part_vertex_clipped_429544(scratch.model_view.data(), items[k].xyz, values,
                                                                 items[k].output);
                    else
                        detail::world_part_vertex_inside_429673(scratch.model_view.data(), items[k].xyz, values,
                                                                items[k].output);
                }
            };
            if (!parallel)
            {
                body(0, n);
                return;
            }
            SGN_PROFILE_SCOPE(clipped ? "models.transform_clipped_429544_parallel"
                                      : "models.transform_inside_429673_parallel");
            if (sgn::host_profile::enabled())
                sgn::host_profile::sample(clipped ? "models.transform_clipped_429544_elements"
                                                  : "models.transform_inside_429673_elements",
                                          n);
            detail::pass_parallel_for(n, body);
        };
        try
        {
            for (W i = 0; i < count; ++i)
            {
                const auto xyz = source.plus(std::uint64_t(i) * 16).bytes(12);
                auto output = model.context.bytes(*destination_allocation, add(destination, std::uint64_t(i) * 32), 32);
                jobs.push_back({xyz.data(), output.data()}); // capacity reserved above: cannot throw
            }
        }
        catch (...)
        {
            run(false); // the vertices borrowed before the failure are transformed, in order, as before
            throw;
        }
        const auto *outputs = jobs.front().output;
        const std::size_t output_bytes = std::size_t(count) * 32u;
        bool overlap = false;
        for (const auto &job : jobs)
            overlap = overlap || detail::pass_ranges_overlap(job.xyz, 12, outputs, output_bytes);
        run(!overlap);
        return;
    }
#endif
    for (W i = 0; i < count; ++i)
    {
        const auto xyz = source.plus(std::uint64_t(i) * 16).bytes(12);
        auto output = model.context.bytes(*destination_allocation, add(destination, std::uint64_t(i) * 32), 32);
#ifdef SGN_WORLD_X87_REFERENCE
        if (clipped)
            sgn_source_part_vertex_clipped_429544(scratch.model_view.data(), xyz.data(), values.data(), output.data());
        else
            sgn_source_part_vertex_inside_429673(scratch.model_view.data(), xyz.data(), values.data(), output.data());
#else
        // Plain C++ of429544 /429673 (see detail/world_vertex_kernels.hpp).
        if (clipped)
            detail::world_part_vertex_clipped_429544(scratch.model_view.data(), xyz.data(), values, output.data());
        else
            detail::world_part_vertex_inside_429673(scratch.model_view.data(), xyz.data(), values, output.data());
#endif
    }
}
void emit_part(const Model &model, const SourcePmdLocation &part, const WorldModelViewScratch &scratch,
               WorldTransformedVertexCursor &cursor, bool clipped, WorldPolygonProducerList &opaque,
               WorldPolygonProducerList &alpha)
{
    W forced = 0;
    if (model.read<std::uint8_t>(2) & 8u)
    {
        const auto flags = model.context.read<std::uint16_t>(part, 2);
        if (flags & 0x80u)
            forced = W(flags) >> 8;
    }
    const auto normals = model.field_pointer(0x28), indices = model.field_pointer(0x38);
    Pointer headers, vertices, attributes;
    std::uint16_t count;
    // The clipped and inside leaves capture their fields in different orders.
    if (clipped)
    {
        headers = model.pointer(model.context.read<W>(part, 0x34));
        vertices = model.pointer(model.context.read<W>(part, 0x30));
        count = model.context.read<std::uint16_t>(part, 0x24);
    }
    else
    {
        vertices = model.pointer(model.context.read<W>(part, 0x30));
        attributes = model.pointer(model.context.read<W>(part, 0x2c));
        count = model.context.read<std::uint16_t>(part, 0x24);
    }
    detail::PacketEmissionCursors cursors(opaque, alpha, nullptr);
    // NEW host performance: checked record runs over the two local registers.
    WorldPolygonPacketPointer::SourceRun opaque_run(cursors.opaque), alpha_run(cursors.alpha);
    if (clipped)
        attributes = model.pointer(model.context.read<W>(part, 0x2c));
    else
        headers = model.pointer(model.context.read<W>(part, 0x34));
    const SourceAllocationRef *transformed_allocation{};
    // NEW HOST performance (R30 perf-tick): the attribute base token and the last transformed-cursor token
    // of this call. SourceAllocationBank::encode_address(A,o) is admission that cannot change inside this
    // invocation (same thread, no allocation or release, see Context) plus an append-only token lookup
    // (tokens are never erased or renumbered): after (A,o) returned t once, it returns t again with no
    // side effect. `attributes` is fixed for the whole loop and Pointer::at is deterministic.
    std::optional<W> attributes_token;
    SourceAllocationRef encoded_allocation{};
    W encoded_output{}, encoded_token{};
    bool encoded{};
    for (W i = 0; i < count; ++i)
    {
        const auto header = headers.plus(std::uint64_t(i) * 8);
        const auto corner_base = indices.plus(std::uint64_t(header.plus(6).read<std::uint16_t>()) * 2);
        W common = 63;
        Pointer last;
        if (clipped)
        {
            const W corners = header.read<W>() & 255u;
            if (!corners)
                continue;
            W any = 0;
            for (W corner = 0; corner < corners; ++corner)
            {
                const auto attribute = corner_base.plus(std::uint64_t(corner) * 2).read<std::uint16_t>();
                const auto output =
                    model.context.transformed_offset(cursor, transformed_allocation); // fresh61C510 per corner
                last = attributes.plus(std::uint64_t(attribute) * 16);
                const auto vertex = last.plus(8).read<std::uint16_t>();
                const auto mask =
                    model.context.read_at<std::uint16_t>(*transformed_allocation, add(output, add(W(vertex) * 32, 16)));
                any |= mask;
                common &= mask;
            }
            if (any != 63)
                continue;
        }
        else
            last = attributes.plus(std::uint64_t(corner_base.read<std::uint16_t>()) * 16);
        const auto vertex = last.plus(8).read<std::uint16_t>(); // clipped rereads last corner
        const auto point = vertices.plus(std::uint64_t(vertex) * 16);
        const double dx = double(point.read<float>()) - scratch.local_camera.x;
        const double dy = double(point.plus(4).read<float>()) - scratch.local_camera.y;
        const double dz = double(point.plus(8).read<float>()) - scratch.local_camera.z;
        W flags = header.read<W>();
        if (!(flags & 0x80000000u))
        {
            const auto normal = normals.plus(std::uint64_t(header.plus(4).read<std::uint16_t>()) * 16);
            const double z = dz * normal.plus(8).read<float>(), y = dy * normal.plus(4).read<float>();
            const double zy = z + y;
            const double x = dx * normal.read<float>();
            if (zy + x >= 0.0)
                continue;
        }
        if (!clipped && !forced)
            flags = header.read<W>(); // 429E2F rereads the header
        const bool translucent = forced != 0 || (flags & 0x40000000u) != 0;
        auto &current = translucent ? alpha_run : opaque_run;
        const auto store = [&](unsigned lane, W word) {
            current.require_source_platform(model.context.platform);
            current.store_source_word(lane, word);
        };
        W output;
        if (clipped)
        {
            store(0, header.token());
            output = model.context.transformed_offset(cursor, transformed_allocation);
            store(1, corner_base.token());
        }
        else
        {
            store(1, corner_base.token());
            output = model.context.transformed_offset(cursor, transformed_allocation);
            store(0, header.token());
        }
        if (!encoded || !(encoded_allocation == *transformed_allocation) || encoded_output != output)
        {
            encoded_token = model.context.bank.encode_address(*transformed_allocation, output);
            encoded_allocation = *transformed_allocation;
            encoded_output = output;
            encoded = true;
        }
        store(3, encoded_token);
        if (!attributes_token)
            attributes_token = attributes.token();
        store(2, *attributes_token);
        if (clipped)
            store(4, common);
        current.advance();
    }
    cursors.publish(clipped ? detail::PacketEmissionPath::part_clipped : detail::PacketEmissionPath::part_inside,
                    count == 0);
}
// Actual source42B400/42A320/42A570, sharing the same bank/descriptor
// accessors as transform/emission. These helpers never use native light indices.
TransformFields transform_fields(const Model &model, const SourcePmdLocation &part)
{
    const auto raw = model.context.read<W>(part, 8);
    if (model.pmd)
        return {&model.context, model.pointer(raw).location(), {}};
    if (model.source_strip)
    {
        if (raw != 0x474820)
            throw SourceAllocationError(0x42b445, "unrepresented strip lighting transform");
        return {nullptr,
                {},
                {reinterpret_cast<const std::uint8_t *>(&model.context.platform->state().strip.part_transform), 80}};
    }
    if (raw != 0x4788c0)
        throw SourceAllocationError(0x42b445, "unrepresented generated lighting transform");
    return {nullptr, {}, {reinterpret_cast<const std::uint8_t *>(&model.context.draws.sprite_part_transform), 80}};
}
W raw_word(std::span<const std::uint8_t> bytes, W offset)
{
    if (offset > bytes.size() || 4 > bytes.size() - offset)
        throw SourceAllocationError(0x42b400, "source lighting field extent");
    W value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}
std::span<std::uint8_t> prepared_bytes(const Context &context, const SourceWorldBufferBinding &owner, W offset, W size)
{
    if (&owner != context.admitted_prepared)
    { // NEW host performance: invariant owner admission once
        if (owner.bank != &context.bank || owner.platform.lock() != context.platform)
            throw SourceAllocationError(0x42b478, "source prepared lights belong to another platform");
        context.admitted_prepared = &owner;
    }
    return context.bytes(owner.allocation, add(owner.byte_offset, offset), size);
}
std::span<const std::uint8_t> normal_bytes(const Model &model, const Pointer &pointer)
{
    if (!pointer.relative() && pointer.raw == 0x570ad8)
    {
        const auto &values = model.context.platform->state().strip.normals;
        if (pointer.delta > sizeof(values) || 12 > sizeof(values) - pointer.delta)
            throw SourceAllocationError(0x42a413, "static strip normal outside owner");
        return {reinterpret_cast<const std::uint8_t *>(values.data()) + pointer.delta, 12};
    }
    if (!pointer.relative() && pointer.raw == 0x4788b0)
    {
        const auto &values = model.context.draws.sprite_source.normals;
        if (pointer.delta > sizeof(values) || 12 > sizeof(values) - pointer.delta)
            throw SourceAllocationError(0x42a413, "static source normal outside owner");
        return {reinterpret_cast<const std::uint8_t *>(values.data()) + pointer.delta, 12};
    }
    return pointer.bytes(12);
}
void prepare_model_lights(const Model &model, const SourcePmdLocation &part, const ViewMatrixArena &matrices,
                          const CameraMatrix &identity, const WorldModelViewScratch &scratch)
{
    auto &state = model.context.platform->state();
    std::array<float, 6> pose; // original local scaled camera and position spills
    sgn_source_model_light_camera_42b406(&scratch.normalized_camera.x, pose.data());
    const auto count = model.read<W>(0x24), outer_reference = model.read<W>(0xc);
    const auto &outer = resolve_source_draw_pose(model.context.draws, outer_reference);
    const auto rotation_reference = outer.tail[2]; // 42B42E captures a pointer without dereferencing it
    const CameraMatrix *rotation = nullptr;
    CameraMatrix multiplied{}; // only nine initialized/consumed entries below
    if (count > 1)
    {
        const auto transform = transform_fields(model, part);
        const auto inverse_reference = transform.read<W>(0x44);
        const auto &inverse = resolve_model_matrix(matrices, identity, inverse_reference);
        const auto &outer_matrix = resolve_model_matrix(matrices, identity, rotation_reference);
        sgn_source_model_light_basis_44efd0(multiplied.data(), outer_matrix.data(), inverse.data());
        rotation = &multiplied;
    }
    const auto transform = transform_fields(model, part);              // 42B457 reloads part08
    const auto selected_count = state.light_selection.active_count;    // one42B45A capture
    const auto &prepared_base = state.light_vertices.source_binding(); // borrowed in place, unchanged in this call
    // No destination-buffer read on the count0 path. Actual624574 is reset
    // between the Y and Z position spills, exactly at42B482.
    sgn_source_model_light_position_42b462(transform.bytes(0, 12).data(), &outer.position.x, pose.data() + 3,
                                           &state.light_vertex_offset);
    W offset = 0;
    for (W i = 0; i < selected_count; ++i)
    {
        const auto pointer = state.light_selection.ordered.source_at(i);
        auto light = source_light_record(model.context.platform, pointer);
        auto prepared = prepared_bytes(model.context, prepared_base, offset, 36);
        // The single-part, count0 route never dereferences42B42E's matrix.
        const auto &basis = rotation ? *rotation : resolve_model_matrix(matrices, identity, rotation_reference);
        sgn_source_model_light_record_42b499(light.data(), prepared.data(), basis.data(), pose.data(),
                                             &state.light_vertex_offset);
        const auto token = source_light_pointer_token(model.context.platform, pointer);
        std::memcpy(prepared.data() + 0x20, &token, 4); // 42B56A, after all source float stores
        offset = add(offset, 36);
    }
}
std::array<W, 3> ambient_color(const Model &model)
{
    const auto &actual = model.context.platform->state().ambient_light.packed;
    const auto blue = actual[2], red = actual[0], green = actual[1];
    const auto multiplier = model.read<std::uint16_t>(4);
    std::array<W, 3> result{red, green, blue};
    if (multiplier > 1)
        for (auto &value : result)
            value = (value * W(multiplier)) >> 8;
    return result;
}
void shade_model(const Model &model, const SourcePmdLocation &part, const ViewMatrixArena &matrices,
                 const CameraMatrix &identity, const WorldModelViewScratch &scratch, bool specular)
{
    auto &state = model.context.platform->state();
    if (&scratch != &state.polygon_frame.model_view || &matrices != &state.view_matrices ||
        &identity != &state.shared_model_identity)
        throw SourceAllocationError(0x42a320, "source lighting requires this platform's actual frame/matrix owners");
    auto base = specular ? std::array<W, 3>{} : ambient_color(model);
    prepare_model_lights(model, part, matrices, identity, scratch); // 42A37A vs42A57D
    if (specular)
        base = ambient_color(model);
    W shift = 0;
    Pointer vertices, normals, coordinates;
    W stride;
    if (specular)
    {
        shift = 8u + (model.context.read<std::uint8_t>(part, 2) & 3u);
        normals = model.field_pointer(0x28);
        vertices = model.pointer(model.context.read<W>(part, 0x30));
        coordinates = model.pointer(model.context.read<W>(part, 0x2c));
        stride = state.polygon_frame.lighting_stride;
    }
    else
    {
        vertices = model.pointer(model.context.read<W>(part, 0x30));
        stride = state.polygon_frame.lighting_stride;
        normals = model.field_pointer(0x28);
        coordinates = model.pointer(model.context.read<W>(part, 0x2c));
    }
    W ordinal = (stride - 1u) & state.polygon_frame.frame_stamp;
    const auto count = model.context.read<std::uint16_t>(part, 0x26);
    auto coordinate = coordinates.plus(std::uint64_t(ordinal) * 16);
    while (ordinal < count)
    {
        const auto vertex = vertices.plus(std::uint64_t(coordinate.plus(8).read<std::uint16_t>()) * 16);
        const auto normal = normals.plus(std::uint64_t(coordinate.plus(10).read<std::uint16_t>()) * 16);
        auto color = base;
        W highlight = 0;
        const auto &prepared_base =
            state.light_vertices.source_binding();              // 42A3DC/42A62C, before count (borrowed in place)
        const auto active = state.light_selection.active_count; // fresh count per coordinate
        for (W j = 0; j < active; ++j)
        {
            const auto prepared = prepared_bytes(model.context, prepared_base, j * 36u, 36);
            const auto light = model.context.light(
                raw_word(prepared, 0x20)); //== source_light_record(from_token(...)), memoized per token
            const auto kind = raw_word(light, 4);
            if (kind > 1)
                continue;
            const auto normal_xyz = normal_bytes(model, normal);
            const auto incidence = light_dot(prepared.data(), normal_xyz.data(), specular ? 1u : 0u);
            if (!(incidence & 0x80000000u))
                continue; // including negative zero, no floating comparison
            if (kind == 0)
            {
                const auto intensity = light_ftol(incidence);
                for (W lane = 0; lane < 3; ++lane)
                    color[lane] -= raw_word(light, 0x78 + lane * 4) * intensity;
                if (specular)
                {
                    const auto reflected = light_dot(prepared.data() + 0x10, normal_xyz.data(), 0);
                    if (reflected & 0x80000000u)
                        highlight -= light_ftol(reflected);
                }
            }
            else
            {
                const auto point = vertex.bytes(12);
                const auto intensity = light_point(light.data(), point.data());
                for (W lane = 0; lane < 3; ++lane)
                    color[lane] += raw_word(light, 0x78 + lane * 4) * intensity;
            }
        }
        W gamma = 0;
        if (specular)
        {
            const auto shifted = std::bit_cast<std::int32_t>(highlight) >> shift;
            const auto index = shifted < 0 ? 0u : shifted > 255 ? 255u : static_cast<W>(shifted);
            gamma = state.color_tables.raster_gamma[index]; // actual62457C, before RGB clamp
        }
        for (auto &value : color)
            if (value > 0xff00u)
                value = 0xff00;
        const W packed = specular ? ((((gamma & 0xffffff00u) << 8) | (color[2] & 0xff00u)) << 8) | (color[0] >> 8) |
                                        (color[1] & 0xff00u)
                                  : (((color[2] & 0xff00u) | 0x10000u) << 8) | (color[1] & 0xff00u) | (color[0] >> 8);
        stride = state.polygon_frame.lighting_stride; // live read before the coordinate store
        coordinate.write(0xc, packed);
        if (!stride)
            throw SourceAllocationError(specular ? 0x42a80du : 0x42a54fu,
                                        "source zero lighting stride would not terminate");
        coordinate = coordinate.plus(std::uint64_t(stride) * 16);
        ordinal += stride;
    }
}
void produce(const Model &model, const CameraState &camera, const ViewMatrixArena &arena, const CameraMatrix &identity,
             const ProjectionParameters &projection, bool clipped, WorldModelViewScratch &scratch,
             WorldTransformedVertexCursor &cursor, WorldPolygonProducerList &opaque, WorldPolygonProducerList &alpha,
             unsigned lighting_route = 0, const W *split_setting = nullptr, W *lighting_stride = nullptr)
{
    auto captured = model.pointer(model.selected());
    if (model.read<W>(0x24) == 0)
        return;
    W ordinal = 0;
    do
    {
        const auto part = model.part(captured);
        if (model.context.read<std::uint16_t>(part) == 1)
        {
            local_camera(model, part, camera, arena, identity, scratch);
            model_view(model, &part, camera, arena, identity, scratch.model_view);
            transform_vertices(model, part, projection, clipped, scratch, cursor);
            emit_part(model, part, scratch, cursor, clipped, opaque, alpha);
            if (lighting_route && (model.context.read<std::uint8_t>(part, 2) & 4u))
            {
                if (!split_setting || !lighting_stride)
                    throw SourceAllocationError(0x42a870, "source lighting scalar owners absent");
                if (*split_setting)
                    *lighting_stride = 1;
                else
                {
                    const auto current = model.read<W>(0x5c), previous = model.read<W>(0x64);
                    if (current != previous)
                        *lighting_stride = 1;
                    else
                    {
                        const auto word = model.context.read<std::uint16_t>(part, 4);
                        // 42A8A9 writes1 before42A8B5 writes4 for values>5.
                        if (word >= 2)
                            *lighting_stride = 1;
                        if (word < 2 || word > 5)
                            *lighting_stride = 4;
                    }
                }
                const bool specular = lighting_route == 2 && (model.context.read<std::uint8_t>(part, 2) & 3u);
                SGN_PROFILE_SCOPE("models.shade_42a320");
                shade_model(model, part, arena, identity, scratch, specular);
            }
            const auto current = cursor.count;
            const auto count = model.context.read<std::uint16_t>(part, 0x28);
            cursor.count = current + W(count); // 42A2F5 pointer-only publication, no capacity scan
        }
        const auto count = model.read<W>(0x24); // 42A2FB reload before increment
        captured = captured.plus(64);
        ++ordinal;
        if (ordinal >= count)
            break;
    } while (true);
}
void require_queue(const Context &context, std::span<const ModelDrawCommand> commands)
{
    for (const auto *queue : {&context.draws.background, &context.draws.regular})
    {
        for (std::size_t i = 0; i < queue->records.size(); ++i)
            if (commands.data() == queue->records.data() + i && commands.size() <= queue->records.size() - i)
                return;
    }
    throw SourceAllocationError(0x42af30, "source command span is not borrowed from this scene queue");
}
Model queued_model(const Context &, W);
} // namespace
void produce_source_world_background_models_42af30(std::span<const ModelDrawCommand> commands,
                                                   const SceneResources &scene, const WorldModelDrawState &draws,
                                                   const SourceSpritePacketStorage &sprites, const CameraState &camera,
                                                   const CameraPresentationState &presentation,
                                                   const CameraLifecycle &lifecycle, const ViewMatrixArena &arena,
                                                   const CameraMatrix &identity, const ProjectionParameters &projection,
                                                   WorldPolygonFrame &frame, const W &rate)
{
    const Context context(scene, draws, sprites);
    require_queue(context, commands);
    auto live_projection = projection;
    live_projection.clip_near = lifecycle.clip_near;
    for (const auto &command : commands)
    {
        const auto identity_word = command.model_identity;
        if (!identity_word)
            return;
        host_presentation::ModelPacketScope presentation_tag(frame,
                                                             identity_word); // NEW R19 interp host tag (read-only)
        const auto model = queued_model(context, identity_word);
        model.write(0xc, source_draw_pose_reference(draws, command.pose)); // 42AF43 before visibility
        W visibility;
        {
            SGN_PROFILE_SCOPE("background.classify_428040");
            visibility = classify_whole(model, camera, presentation, lifecycle, arena, identity, live_projection,
                                        frame.bound_matrix);
        }
        if (visibility == 1)
            continue;
        if (model.read<W>(0x58) && !visible_only_widened(model, camera, presentation, lifecycle, arena, identity,
                                                         live_projection, frame.bound_matrix))
            select_detail(model, camera, presentation, rate); // NEW R20 guard: identity when not widened
        const auto pair = visibility ? 0u : 2u;
        SGN_PROFILE_SCOPE("background.produce_42a260");
        produce(model, camera, arena, identity, live_projection, visibility != 0, frame.model_view, frame.transformed,
                frame.packets[pair], frame.packets[pair + 1]);
        ++frame.accepted_models;
    }
    throw SourceAllocationError(0x42af84, "source command queue has no reached terminator");
}
void produce_source_world_sprite_unlit_42a260(const SceneResources &scene, const WorldModelDrawState &draws,
                                              const SourceSpritePacketStorage &sprites, const CameraState &camera,
                                              const ViewMatrixArena &arena, const CameraMatrix &identity,
                                              const ProjectionParameters &projection, bool clipped,
                                              WorldModelViewScratch &scratch, WorldTransformedVertexCursor &cursor,
                                              WorldPolygonProducerList &opaque, WorldPolygonProducerList &alpha)
{
    const Context context(scene, draws, sprites);
    const Model model(context);
    produce(model, camera, arena, identity, projection, clipped, scratch, cursor, opaque, alpha);
}

namespace
{
Model queued_model(const Context &context, W identity)
{
    if (identity == strip_model_identity)
    {
        const auto *binding = std::get_if<SceneStripBinding>(&context.scene.source_records()[1].descriptor);
        if (!binding || binding->owner != &context.platform->state().strip)
            throw SourceAllocationError(0x42af30, "source strip registry identity names another or native owner");
        return Model(context, context.bank.encode_address(binding->source_descriptor, 0));
    }
    if (identity == sprite_model_identity)
        return Model(context);
    if (identity >= 0x80000000u)
    {
        if (context.scene.models().size() >= 0x80000000ull)
            throw SourceAllocationError(0x42af30, "source model ids overlap generated allocation tokens");
        return Model(context, identity);
    }
    const auto handle = context.scene.models().resolve(identity);
    if (!handle)
        throw SourceAllocationError(0x42af90, "source queue model identity absent");
    return Model(context, context.scene.models().source_at(*handle));
}
void initialize_track_shading(const Context &context, SourceWorldFrameProduction w)
{
    if (w.lighting_enabled == 1 || w.frame.low_quality_initialized == 1)
        return;
    const auto incoming = w.frame.active_track; // 42AD89 before stores, no dereference
    w.frame.low_quality_initialized = 1;
    w.frame.lighting_stride = 1;
    const Model model(context, incoming);
    auto captured = model.field_pointer(0x48).plus(64);
    W ordinal = 1;
    if (model.read<W>(0x24) <= ordinal)
        return;
    do
    {
        const auto part = model.part(captured);
        if (model.context.read<std::uint8_t>(part, 2) & 4u)
            shade_model(model, part, w.matrices, w.identity, w.frame.model_view, false);
        const auto count = model.read<W>(0x24);
        captured = captured.plus(64);
        ++ordinal;
        if (ordinal >= count)
            break;
    } while (true);
}
} // namespace
void produce_source_world_sky_42ad30(const ModelDrawCommand &command, SourceWorldFrameProduction w)
{
    const Context context(w.scene, w.draws, w.sprites);
    require_queue(context, {&command, 1});
    const auto captured = command.model_identity;
    w.lifecycle.clip_far = std::bit_cast<float>(0x4a989680u); // before model+0C, even bad pointer
    const auto model = queued_model(context, captured);
    model.write(0xc, source_draw_pose_reference(w.draws, command.pose));
    begin_world_polygon_group(w.frame, 0);
    auto projection = w.projection;
    projection.clip_near = w.lifecycle.clip_near;
    produce(model, w.camera, w.matrices, w.identity, projection, true, w.frame.model_view, w.frame.transformed,
            w.frame.packets[0], w.frame.packets[1]);
    finish_world_polygon_group(w.frame, 0);
}
void produce_source_world_track_parts_42add0(SourceWorldFrameProduction w)
{
    const Context context(w.scene, w.draws, w.sprites);
    initialize_track_shading(context, w);
    const Model model(context, w.frame.active_track); // independent61C52C capture
    const auto enabled = w.lighting_enabled;
    auto captured = model.field_pointer(0x48).plus(64); // base LOD0, not current5C
    bool shading = false, specular = false;
    if (enabled == 1 && w.rate == 100)
    {
        if (w.split_setting)
            w.frame.lighting_stride = 1;
        else
        {
            const auto current = model.read<W>(0x5c), previous = model.read<W>(0x64);
            w.frame.lighting_stride = current == previous ? 4u : 1u;
        }
        shading = true;
        specular = w.quality == 1;
    }
    else
        w.quality = 0;
    W ordinal = 1;
    const auto initial_count = model.read<W>(0x24);
    model.write(0xc, 0x4864e8u); // 42AE39 even count0/1
    if (initial_count <= ordinal)
        return;
    auto projection = w.projection;
    projection.clip_near = w.lifecycle.clip_near;
    do
    {
        const auto part = model.part(captured);
        if (context.read<std::uint16_t>(part) == 1)
        {
            // 428040 deliberately reads current5C's corresponding part.
            const auto visibility = classify_whole(model, w.camera, w.presentation, w.lifecycle, w.matrices, w.identity,
                                                   projection, w.frame.bound_matrix, ordinal);
            if (visibility == 1)
            {
                const auto flags = context.read<std::uint8_t>(part, 2);
                context.write(part, 2, static_cast<std::uint8_t>(flags | 0x10u));
            }
            else
            {
                const auto flags = context.read<std::uint16_t>(part, 2);
                context.write(part, 2, static_cast<std::uint16_t>(flags & 0xffefu));
                local_camera(model, part, w.camera, w.matrices, w.identity, w.frame.model_view);
                model_view(model, &part, w.camera, w.matrices, w.identity, w.frame.model_view.model_view);
                const bool clipped = visibility != 0;
                const auto pair = clipped ? 0u : 2u;
                transform_vertices(model, part, projection, clipped, w.frame.model_view, w.frame.transformed);
                emit_part(model, part, w.frame.model_view, w.frame.transformed, clipped, w.frame.packets[pair],
                          w.frame.packets[pair + 1]);
                if (shading)
                {
                    const auto live_flags = context.read<std::uint16_t>(part, 2);
                    if (live_flags & 4u)
                        shade_model(model, part, w.matrices, w.identity, w.frame.model_view,
                                    specular && (live_flags & 3u));
                }
                const auto current = w.frame.transformed.count;
                const auto vertices = context.read<std::uint16_t>(part, 0x28);
                const auto accepted = w.frame.accepted_models + 1u;
                w.frame.transformed.count = current + W(vertices);
                w.frame.accepted_models = accepted;
            }
        }
        const auto count = model.read<W>(0x24);
        captured = captured.plus(64);
        ++ordinal;
        if (ordinal >= count)
            break;
    } while (true);
}
void produce_source_world_regular_models_42af90(std::span<const ModelDrawCommand> commands,
                                                SourceWorldFrameProduction w)
{
    const Context context(w.scene, w.draws, w.sprites);
    require_queue(context, commands);
    const bool enabled = w.lighting_enabled != 0; // one capture before the first queue word
    auto projection = w.projection;
    projection.clip_near = w.lifecycle.clip_near;
    for (const auto &command : commands)
    {
        const auto captured = command.model_identity;
        if (!captured)
            return;
        host_presentation::ModelPacketScope presentation_tag(w.frame, captured); // NEW R19 interp host tag (read-only)
        const auto model = queued_model(context, captured);
        model.write(0xc, source_draw_pose_reference(w.draws, command.pose));
        W visibility;
        {
            SGN_PROFILE_SCOPE("regular.classify_428040");
            visibility = classify_whole(model, w.camera, w.presentation, w.lifecycle, w.matrices, w.identity,
                                        projection, w.frame.bound_matrix);
        }
        if (visibility == 1)
            continue;
        if (model.read<W>(0x58) && !visible_only_widened(model, w.camera, w.presentation, w.lifecycle, w.matrices,
                                                         w.identity, projection, w.frame.bound_matrix))
            select_detail(model, w.camera, w.presentation, w.rate); // NEW R20 guard: identity when not widened
        const auto slot = model.read<W>(0x5c);
        const unsigned route = !enabled || slot >= 2 ? 0u : slot == 1 ? 1u : 2u;
        const auto pair = visibility ? 0u : 2u;
        SGN_PROFILE_SCOPE("regular.produce_42a260");
        produce(model, w.camera, w.matrices, w.identity, projection, visibility != 0, w.frame.model_view,
                w.frame.transformed, w.frame.packets[pair], w.frame.packets[pair + 1], route, &w.split_setting,
                &w.frame.lighting_stride);
        ++w.frame.accepted_models;
    }
    throw SourceAllocationError(0x42b020, "source regular queue has no reached terminator");
}
void produce_source_world_frame_42b040(SourceWorldFrameProduction w)
{
    if (&w.scene.models() != &w.models)
        throw SourceAllocationError(0x42b040, "source world frame belongs to another Models owner");
    const auto ticket = capture_world_polygon_level(w.frame);
    validate_world_polygon_level(w.frame, ticket);
    const Context context(w.scene, w.draws, w.sprites);
    const auto &vertices = w.frame.transformed.allocation.source_binding();
    if (vertices.bank != &context.bank || vertices.platform.lock() != context.platform)
        throw SourceAllocationError(0x42b040, "source frame vertices belong to another Session platform");
    try
    {
        SourceWorldTrackContext track(w.scene, w.draws, w.sprites);
        auto &frame = w.frame;
        auto &background = w.draws.background.records;
        prepare_source_track_initial_view(track, w.camera, w.matrices, w.identity, w.frame_world_view,
                                          frame.accepted_models);
        reset_world_polygon_groups(frame);
        begin_world_polygon_group(frame, 1);
        // 42B06C's queue input is independent of636448 used for the initial view
        // and base part. Preserve this source distinction; NULL rejects at its write.
        const auto incoming = queued_model(context, background[0].model_identity);
        incoming.write(0xc, source_draw_pose_reference(w.draws, background[0].pose));
        if (!incoming.pmd)
            throw SourceAllocationError(0x42ac70, "track producer requires actual PMD metadata");
        const auto input = SourceTrackModelPointer::captured_source(*incoming.pmd, *incoming.captured_pmd_descriptor);
        auto projection = w.projection;
        projection.clip_near = w.lifecycle.clip_near;
        {
            SGN_PROFILE_SCOPE("world.track_base_42ac70");
            track.produce_base_prefix_42ac70(input, w.camera, w.lifecycle, w.matrices, w.identity,
                                             w.presentation.planes, projection, frame, w.animations);
        }
        host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
        finish_world_polygon_group(frame, 1);
        frame.remaining_first = frame.transformed.count;
        {
            SGN_PROFILE_SCOPE("world.sky_42ad30");
            produce_source_world_sky_42ad30(background[1], w);
        }
        w.lifecycle.clip_far = std::bit_cast<float>(0x48f42400u);
        begin_world_polygon_group(frame, 2);
        {
            SGN_PROFILE_SCOPE("world.background_models_42af30");
            produce_source_world_background_models_42af30(std::span(background).subspan(2), w.scene, w.draws, w.sprites,
                                                          w.camera, w.presentation, w.lifecycle, w.matrices, w.identity,
                                                          projection, frame, w.rate);
        }
        {
            SGN_PROFILE_SCOPE("world.track_parts_42add0");
            if (w.rate >= 25)
                produce_source_world_track_parts_42add0(w);
        }
        host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
        finish_world_polygon_group(frame, 2);
        begin_world_polygon_group(frame, 3);
        {
            SGN_PROFILE_SCOPE("world.regular_models_42af90");
            produce_source_world_regular_models_42af90(w.draws.regular.records, w);
        }
        host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
        finish_world_polygon_group(frame, 3);
        const auto track_handle = w.scene.model_handle(3), sky_handle = w.scene.model_handle(2);
        if (!track_handle || !sky_handle)
            throw SourceAllocationError(0x42b740, "source seed model registry incomplete");
        {
            SGN_PROFILE_SCOPE("world.seed_models");
            seed_world_model_draws(w.draws, w.models, *track_handle, *sky_handle, w.sprites, w.ground_level,
                                   w.scene_mode, w.matrices, &w.scene);
        }
        publish_world_polygon_totals(frame);
    }
    catch (...)
    {
        detach_world_polygon_level(w.frame);
        throw;
    }
}
} // namespace sgn::retail
