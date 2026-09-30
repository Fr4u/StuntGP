#include "sgn/source_world_track_view.hpp"
#include "sgn/gpu_world.hpp" //NEW host rendering (gpu_world R19)
#include "sgn/source_draw_pose.hpp"
#include "sgn/source_world_spatial_emission.hpp"
#include "sgn/retail_camera_lifecycle.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/source_bank_read_memo.hpp"
#include "sgn/host_profiler.hpp"
#include "sgn/host_presentation.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace sgn::retail
{
namespace
{
constexpr std::uint32_t template_ref = 0x4864e8, background_pose_base = 0x632cfc;
constexpr std::uint32_t command_stride = 84;
static_assert(sizeof(ModelDrawCommand) == command_stride);
void observe(SourceTrackInitialViewAudit audit, std::uint32_t site)
{
    if (audit.reached)
        audit.reached(audit.context, site);
}
CameraVector vector(Vec3 v, std::uint32_t w)
{
    return {v.x, v.y, v.z, std::bit_cast<float>(w)};
}
CameraVector vector(VehicleVector v)
{
    return {v.x, v.y, v.z, v.w};
}
SourcePmdLocation spatial_offset(SourcePmdLocation base, std::uint32_t offset, std::uint32_t site)
{
    // Host domain: keep allocation identity; never wrap into a different bank
    // allocation or pretend tokens support source-address arithmetic.
    if (offset > std::numeric_limits<std::uint32_t>::max() - base.offset)
        throw SourceAllocationError(site, "spatial address offset overflows host location");
    base.offset += offset;
    return base;
}
template <class T> T spatial_read(SourceAllocationBank &bank, SourcePmdLocation location, std::uint32_t site)
{
    const auto bytes = bank.bytes(location.allocation); // thread/generation/extent, no model-copy lookup
    if (location.offset > bytes.size() || sizeof(T) > bytes.size() - location.offset)
        throw SourceAllocationError(site, "reached spatial field leaves its actual allocation");
    T value;
    std::memcpy(&value, bytes.data() + location.offset, sizeof(T));
    return value;
}
// NEW host performance (perf_world R19): offset-only forms of the two
// helpers above over a SourceBankReadMemo. Same overflow/extent checks, sites
// and messages; no per-field SourcePmdLocation (weak identity) copies.
std::uint32_t spatial_add(std::uint32_t base, std::uint32_t offset, std::uint32_t site)
{
    if (offset > std::numeric_limits<std::uint32_t>::max() - base)
        throw SourceAllocationError(site, "spatial address offset overflows host location");
    return base + offset;
}
template <class T>
T spatial_read(SourceBankReadMemo &memo, const SourceAllocationRef &allocation, std::uint32_t offset,
               std::uint32_t site)
{
    return memo.whole_read<T>(allocation, offset, site, "reached spatial field leaves its actual allocation");
}
std::uint32_t classify_bounds(SourceBankReadMemo &memo, const SourcePmdLocation &bounds, std::uint16_t first,
                              const CameraState &camera, std::span<const CameraVector, 4> planes)
{
    const auto &allocation = bounds.allocation;
    auto point = spatial_add(bounds.offset, std::uint32_t(first) * 16u, 0x42aa80);
    std::uint32_t common = 63, any = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        const auto x = spatial_read<float>(memo, allocation, point, 0x42aa9b);
        const auto y = spatial_read<float>(memo, allocation, spatial_add(point, 4, 0x42aaa0), 0x42aaa0);
        const auto z = spatial_read<float>(memo, allocation, spatial_add(point, 8, 0x42aaa6), 0x42aaa6);
        const auto mask = world_spatial_point_mask_42aa9b(
            {x, y, z}, {camera.position.x, camera.position.y, camera.position.z}, planes);
        if (mask == 63 && common != mask)
            return 2; // 42AB62, no reads of remaining bounds
        common &= mask;
        any |= mask;
        // 42AB6B pointer arithmetic is not a pointed read; no eager next field.
        point = spatial_add(point, 16, 0x42ab6b);
    }
    return common == 63 ? 0 : any == 63 ? 2 : 1;
}
} // namespace
SourceWorldTrackContext::SourceWorldTrackContext(SceneResources &scene, WorldModelDrawState &draws,
                                                 const SourceSpritePacketStorage &sprites)
    : scene_(scene), draws_(draws), sprites_(sprites)
{
    validate_scene();
}
void SourceWorldTrackContext::validate_scene() const
{
    scene_.require_owner_thread();
    scene_.require_sprite_binding(draws_, sprites_);
}
SourcePmdModelView SourceWorldTrackContext::track() const
{
    validate_scene();
    const auto handle = scene_.model_handle(3);
    if (!handle)
        throw FormatError("canonical track context has no scene track");
    auto model = scene_.models().source_at(*handle);
    if (model.kind() != 1)
        throw FormatError("canonical track context requires kind1");
    return model;
}
void SourceWorldTrackContext::require_track(SourcePmdModelView candidate) const
{
    const auto current = track();
    if (candidate.allocation() != current.allocation() ||
        candidate.descriptor().location() != current.descriptor().location())
        throw FormatError("canonical track borrow belongs to another scene model");
}
std::uint32_t SourceWorldTrackContext::pose_reference(const ModelTransform &pose) const
{
    validate_scene();
    return source_draw_pose_reference(draws_, pose);
}
const ModelTransform &SourceWorldTrackContext::resolve_captured_outer(SourcePmdModelView model,
                                                                      std::uint32_t reference) const
{
    require_track(model);
    return resolve_outer_reference(reference);
}
const ModelTransform &SourceWorldTrackContext::resolve_outer_reference(std::uint32_t reference) const
{
    return resolve_source_draw_pose(draws_, reference);
}
bool SourceWorldTrackContext::publish_initial_template_42e7cc() const
{
    validate_scene();
    if (!scene_.model_handle(3))
        return false;
    track().descriptor().field<std::uint32_t>(0x0c).set(template_ref);
    return true;
}
void SourceWorldTrackContext::publish_background_track_42b072() const
{
    const auto model = track();
    const auto handle = scene_.model_handle(3);
    if (draws_.background.records[0].model_identity != scene_.models().identity(handle))
        throw FormatError("background0 does not name this scene's actual track");
    // Equality is an explicit host admission. Store into the admitted actual
    // background0 descriptor, which is this same canonical model allocation.
    model.descriptor().field<std::uint32_t>(0x0c).set(background_pose_base);
}
void SourceWorldTrackContext::publish_active_track_template_42ae39(SourcePmdModelView active) const
{
    require_track(active);
    active.descriptor().field<std::uint32_t>(0x0c).set(template_ref);
}
void SourceWorldTrackContext::local_camera_42b2a0(SourceTrackModelPointer incoming,
                                                  SourceTrackPartPointer captured_part, const CameraState &camera,
                                                  const ViewMatrixArena &arena, const CameraMatrix &identity,
                                                  WorldModelViewScratch &scratch, WorldLocalCameraAudit *audit) const
{
    validate_scene(); // NEW host admission; no same-model assumption
    const auto descriptor = incoming.source_descriptor();
    const auto outer_reference = descriptor.field<std::uint32_t>(0x0c).value(); // 42B2B4
    const auto count = descriptor.field<std::uint32_t>(0x24).value();           // 42B2B7, unsigned
    const auto &outer = resolve_outer_reference(outer_reference);
    const auto &inverse_outer = resolve_model_matrix(arena, identity, outer.tail[2]); // 42B2CB, outer+48
    // 42B30E follows the separately captured stack part, even at count0/1.
    //  This is not model.part(0,0), descriptor44, or descriptor48 re-reading.
    const auto part = captured_part.source_part();
    const auto transform = part.transform().value();
    Vec3 pivot{};
    const CameraMatrix *inverse_part = nullptr;
    if (count > 1)
    { // 42B330; no pivot access/inverse-part resolution when not taken
        const auto value = part.pivot().value();
        pivot = {value.x, value.y, value.z};
        // 42B338 uses the already captured part+08 transform, not a new one.
        inverse_part = &resolve_model_matrix(arena, identity, transform.tail[1]);
    }
    // Reuse the exact arithmetic order and binary32 store boundaries already
    // recovered in42B2A0, including non-popped Z at42B34D and44ED40 normalize.
    // All guarded source inputs are available before any output is committed.
    world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                             {camera.position.x, camera.position.y, camera.position.z}, outer.position, inverse_outer,
                             {transform.position.x, transform.position.y, transform.position.z}, pivot, inverse_part,
                             count, audit);
}
void SourceWorldTrackContext::base_model_view_44fbd0(SourceTrackModelPointer incoming, const CameraState &camera,
                                                     const ViewMatrixArena &arena, const CameraMatrix &identity,
                                                     WorldModelViewScratch &scratch) const
{
    validate_scene();
    auto &destination = scratch.model_view; // sole actual61C428, not a new frame matrix
    if (&destination == &identity)
        throw FormatError("track model-view destination aliases shared matrix identity");
    const auto descriptor = incoming.source_descriptor();
    const auto outer_reference = descriptor.field<std::uint32_t>(0x0c).value(); // 44FBE3, before identity
    initialize_model_matrix_identity(destination);                              // 44FBE6 ->44EE10
    const auto count = descriptor.field<std::uint32_t>(0x24).value();           // 44FBEB even with null EDX
    // 42ACC8 passes literal null.44FBFA therefore skips the optional part for
    //  ALL counts. Do not resolve the separately published61C530 or model+44.
    const auto &outer = resolve_outer_reference(outer_reference); // 44FCA2 first outer dereference
    const auto scale = vector(outer.scale, 0);
    // 44FCD2/D5/D8 captures only the three pivot DWORDs, in Z/Y/X order.
    const auto pivot_z = descriptor.field<float>(0x18).value();
    const auto pivot_y = descriptor.field<float>(0x14).value();
    const auto pivot_x = descriptor.field<float>(0x10).value();
    const auto &rotation = resolve_model_matrix(arena, identity, outer.tail[1]); // 44FCE5, outer+44
    const CameraModelTransform outer_view{vector(outer.position, 0), scale, {pivot_x, pivot_y, pivot_z, 0}, rotation};
    // CameraState's cached matrix is not the source pointer. Resolve the
    //  actual current-camera+4C reference from the invocation's arena.
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
    destination = camera_model_view(bound_camera, outer_view, count, nullptr);
}
SourceAllocationBank &SourceWorldTrackContext::spatial_bank() const
{
    validate_scene();
    const auto bank = scene_.models().source_bank_;
    if (!bank)
        throw SourceAllocationError(0x42abb0, "source spatial context has no retained PMD allocation bank");
    return *bank; // existing ModelResources contract keeps this bank alive
}
std::uint32_t SourceWorldTrackContext::classify_spatial_bounds_42aa70(std::uint16_t first, const CameraState &camera,
                                                                      std::span<const CameraVector, 4> planes) const
{
    auto &bank = spatial_bank();
    SourceBankReadMemo memo(bank);
    // 42AA76 captures62446C once. Do not resolve current/incoming.track().
    return classify_bounds(memo, scene_.models().source_loading_state().track.spatial_bounds, first, camera, planes);
}
void SourceWorldTrackContext::gather_spatial_batches_42abb0(SourcePmdLocation root, const CameraState &camera,
                                                            std::span<const CameraVector, 4> planes,
                                                            WorldPolygonFrame &frame) const
{
    auto &bank = spatial_bank();
    frame.indexed.require_owner_thread();
    for (const auto &list : frame.spatial)
    {
        if (!list.records.source_bound() || list.records.source_binding().bank != &bank)
            throw SourceAllocationError(0x42abb0,
                                        "spatial records must borrow the same source bank as the PMD globals");
    }
    // NEW host performance (perf_world R19): one memoized bank admission per
    // allocation for this synchronous traversal (see source_bank_read_memo.hpp:
    // it only encodes tokens and stores raw record DWORDs, never allocates,
    // releases or stores opaque transport words). The repeated42AA70 entries
    // no longer repeat the invariant scene/sprite/bank admission of
    // spatial_bank(); it was performed above on this same thread and call.
    SourceBankReadMemo memo(bank);
    // Loader globals are re-read at each source load site through this stable
    // reference; nothing reached here writes the loading state.
    const auto &globals = scene_.models().source_loading_state().track;
    // Host call-stack metadata only. No copied nodes/list payload and no global
    // visited set: valid repeated references must be traversed again in order.
    struct Visit
    {
        const SourceAllocationRef *allocation;
        std::uint32_t node;
        unsigned next;
    };
    // NEW HOST (R30 perf-tick): the traversal stack keeps its capacity across calls (one per owner
    // thread; this traversal never re-enters itself) instead of reallocating while it deepens every
    // view. Cleared at entry, so a throw that left entries behind cannot leak into the next traversal.
    thread_local std::vector<Visit> stack;
    stack.clear();
    stack.push_back({&root.allocation, root.offset, 0});
    while (!stack.empty())
    {
        auto &visit = stack.back();
        if (visit.next == 8)
        {
            stack.pop_back();
            continue;
        }
        const auto &node = *visit.allocation;
        const auto entry = spatial_add(visit.node, std::uint32_t(visit.next) * 8u, 0x42abbb);
        ++visit.next;
        const auto child = spatial_read<std::uint16_t>(memo, node, entry, 0x42abbb); // retained DI
        if (child == 0xffffu)
            continue;
        const auto first = spatial_read<std::uint16_t>(memo, node, spatial_add(entry, 6, 0x42abcc), 0x42abcc);
        const auto classification = classify_bounds(memo, globals.spatial_bounds, first, camera, planes);
        if (classification == 1)
            continue;
        if (classification == 0 || child == 0)
        {
            const bool inside = classification == 0;
            const SourcePmdLocation *polygon_base;
            std::uint16_t polygon_index;
            // Inside reads WORD before global; clipped does the reverse.
            if (inside)
            {
                polygon_index = spatial_read<std::uint16_t>(memo, node, spatial_add(entry, 2, 0x42abde), 0x42abde);
                polygon_base = &globals.spatial_polygon_indices; // 42ABE1
            }
            else
            {
                polygon_base = &globals.spatial_polygon_indices; // 42AC12
                polygon_index = spatial_read<std::uint16_t>(memo, node, spatial_add(entry, 2, 0x42ac19), 0x42ac19);
            }
            const auto polygons =
                spatial_add(polygon_base->offset, std::uint32_t(polygon_index) * 2u, inside ? 0x42abe7u : 0x42ac1cu);
            auto &list = frame.spatial[inside ? 1u : 0u];
            const auto cursor = list.count; // 42ABEA /42AC1F, before vertex WORD read
            const auto vertex_index = spatial_read<std::uint16_t>(
                memo, node, spatial_add(entry, 4, inside ? 0x42abf1u : 0x42ac26u), inside ? 0x42abf1u : 0x42ac26u);
            // Existing token codec performs reached host admission for live
            // allocation and offset. It does not dereference either list.
            list.records.store_source_word(cursor, 0, bank.encode_address(polygon_base->allocation, polygons));
            const auto &vertex_base = globals.spatial_vertex_indices; // 42ABF7 /42AC2C
            list.count = cursor + 1u;                                 // 42AC00 /42AC35, before the second payload store
            const auto vertices =
                spatial_add(vertex_base.offset, std::uint32_t(vertex_index) * 2u, inside ? 0x42ac05u : 0x42ac3au);
            list.records.store_source_word(cursor, 1, bank.encode_address(vertex_base.allocation, vertices));
            continue;
        }
        // 42AC42 reloads process-global624470 for EACH recursive child.
        const auto &nodes = globals.spatial_nodes;
        const auto next = spatial_add(nodes.offset, std::uint32_t(child) * 64u, 0x42ac52);
        for (const auto &ancestor : stack)
            if (ancestor.node == next && *ancestor.allocation == nodes.allocation)
                throw SourceAllocationError(0x42ac54,
                                            "cyclic source spatial recursion; completed publications retained");
        stack.push_back({&nodes.allocation, next, 0});
    }
}
void SourceWorldTrackContext::produce_base_prefix_42ac70(
    SourceTrackModelPointer incoming, const CameraState &camera_state, CameraLifecycle &camera,
    const ViewMatrixArena &arena, const CameraMatrix &identity, std::span<const CameraVector, 4> planes,
    const ProjectionParameters &projection, WorldPolygonFrame &frame, SourceTrackAnimationOwners &animations,
    SourceTrackBasePrefixObserver *observer, WorldLocalCameraAudit *audit) const
{
    // NEW host admission only. Do not require incoming==track: the source
    // retains ECX in ESI independently of its later global636448 load.
    validate_scene();
    frame.indexed.require_owner_thread();
    if (&animations.effects.template_pose != &draws_.template_pose)
        throw FormatError("source42AC70 animation owners belong to another draw-state lifetime");
    if (incoming && !incoming.source_bound())
        throw FormatError("source42AC70 cannot borrow a nonnull native ModelResource descriptor");
    const auto stored = [&](std::uint32_t site) {
        if (observer)
            observer->stored(site, camera, frame);
    };
    camera.clip_far = std::bit_cast<float>(std::uint32_t{0x48f42400});
    stored(0x42ac84);
    // 42AC8E captures the global model identity before either cursor store.
    //  This metadata read does not dereference its descriptor or base part.
    const auto track_handle = scene_.model_handle(3);
    // Existing normalized spatial cursors: count0 denotes the captured base
    // 61C518/61C51C. No payload access or records/native-span conversion occurs.
    frame.spatial[0].count = 0;
    stored(0x42ac93);
    frame.spatial[1].count = 0;
    stored(0x42ac99);
    // 42ACA4 is the first dereference of that separately captured global.
    //  Missing/expired source storage retains clip_far and both cursor stores.
    if (!track_handle)
        throw SourceAllocationError(0x42aca4, "source42AC70 global636448 has no PMD descriptor for+44");
    const auto global_model = scene_.models().source_at(*track_handle);
    const auto part_reference = global_model.descriptor().field<std::uint32_t>(0x44).value();
    const auto captured_part = SourceTrackPartPointer::source(global_model, part_reference);
    // No part-count/LOD/kind/relocation/nonnull admission occurs here. The
    // source publishes even a zero/bad part before the mandatory child reads it.
    frame.spatial_batch_count = 0;
    stored(0x42acae);
    frame.active_track = incoming;
    stored(0x42acb8);
    frame.active_track_base_part = captured_part;
    stored(0x42acbe);
    // 42ACAC/AD captured these call arguments before publishing either global.
    {
        SGN_PROFILE_SCOPE("track.view_42b2a0_44fbd0");
        local_camera_42b2a0(incoming, captured_part, camera_state, arena, identity, frame.model_view, audit);
        // 42ACCA still uses ESI, not a new61C52C/global636448 read. The preceding
        //  XOR EDX,EDX deliberately discards the part argument used by42B2A0.
        base_model_view_44fbd0(incoming, camera_state, arena, identity, frame.model_view);
    }
    // 42ACD6 is the independent LAST-loader global, not incoming.track().nodes.
    const auto spatial_root = scene_.models().source_loading_state().track.spatial_nodes;
    {
        SGN_PROFILE_SCOPE("track.gather_42abb0");
        gather_spatial_batches_42abb0(spatial_root, camera_state, planes, frame);
    }
    frame.indexed.copy_base_to_current(); // 42ACE1/42ACE7, includes actual NULL copy
    {
        SGN_PROFILE_SCOPE("track.transform_clipped_429880");
        transform_spatial_vertices_clipped_429880(projection, frame);
    } // 42ACED
    {
        SGN_PROFILE_SCOPE("track.transform_inside_429730");
        // NEW host rendering (gpu_world R19): mode 1 draws the inside list from
        // static GPU buffers; 429730 output is then read by nobody (DESIGN.md 2).
        if (!gpu_world::begin_track_frame_42acf2(spatial_bank(), frame, projection))
            transform_spatial_vertices_inside_429730(projection, frame);
    } // 42ACF2
    host_presentation::pass_yield_point(); // NEW HOST (R33): may present one interpolated frame, no source work
    {
        SGN_PROFILE_SCOPE("track.emit_42a220");
        emit_source_world_spatial_polygons_42a220(spatial_bank(), frame);
    } // 42ACF7
    // 42ACFC captures the independent part;42AD01 captures current BEFORE
    // the low WORD count access. This must not reset current to61C50C.
    const auto tail_part = frame.active_track_base_part;
    const auto current = frame.transformed.count;
    const auto tail_location = tail_part.source_location();
    const auto vertices =
        spatial_read<std::uint16_t>(spatial_bank(), spatial_offset(tail_location, 0x28, 0x42ad09), 0x42ad09);
    // Normalized source count: each increment is32 bytes. No new capacity or
    // payload read belongs to this pointer-only tail; consumers admit extents.
    frame.transformed.count = current + std::uint32_t(vertices); // 42AD12
    const auto active = frame.active_track;                      // 42AD18, independently reloaded61C52C
    if (active.source_descriptor().field<std::uint32_t>(0x24).value() > 1u)
    {
        SGN_PROFILE_SCOPE("track.animate_426ed0");
        animate_source_track_geometry_426ed0(spatial_bank(), scene_.models().source_loading_state(), incoming,
                                             animations);
    } // 42AD26 retains incoming ESI
    // 42AD2C RET: all reached source children completed.
}
void prepare_source_track_initial_view(const SourceWorldTrackContext &context, const CameraState &camera,
                                       const ViewMatrixArena &arena, const CameraMatrix &identity,
                                       CameraMatrix &destination, std::uint32_t &accepted,
                                       SourceTrackInitialViewAudit audit)
{
    const auto model = context.track(); // new host preflight, before recovered stores
    if (&destination == &identity)
        throw FormatError("track destination aliases shared matrix identity");
    accepted = 0;
    observe(audit, 0x42b04b);
    const auto descriptor = model.descriptor();
    const auto captured_part = descriptor.field<std::uint32_t>(0x44).value();
    observe(audit, 0x42b055);
    const auto captured_outer = descriptor.field<std::uint32_t>(0x0c).value();
    observe(audit, 0x44fbe3);
    initialize_model_matrix_identity(destination);
    observe(audit, 0x44fbe6);
    const auto count = descriptor.field<std::uint32_t>(0x24).value();
    observe(audit, 0x44fbeb);
    std::optional<CameraModelTransform> part_view;
    if (count > 1 && captured_part != 0)
    {
        observe(audit, 0x44fc00);
        const auto part = model.captured_base_part_42b055(captured_part);
        const auto transform = part.transform().value();
        const auto pivot = part.pivot().value();
        part_view = CameraModelTransform{vector(transform.position), vector(transform.scale), vector(pivot),
                                         resolve_model_matrix(arena, identity, transform.tail[0])};
    }
    observe(audit, 0x44fca2);
    const auto &outer = context.resolve_captured_outer(model, captured_outer);
    const auto pivot = descriptor.field<VehicleVector>(0x10).value();
    const CameraModelTransform outer_view{vector(outer.position, outer.position_w), vector(outer.scale, outer.scale_w),
                                          vector(pivot), resolve_model_matrix(arena, identity, outer.tail[1])};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = resolve_model_matrix(arena, identity, camera.world_to_camera_reference);
    destination = camera_model_view(bound_camera, outer_view, count, part_view ? &*part_view : nullptr);
    observe(audit, 0x44fd50);
}
} // namespace sgn::retail
