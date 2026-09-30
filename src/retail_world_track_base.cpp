#include "sgn/retail_world_track_base.hpp"

namespace sgn::retail
{
namespace
{
CameraVector vector(Vec3 value, std::uint32_t w = 0) noexcept
{
    return {value.x, value.y, value.z, std::bit_cast<float>(w)};
}
WorldPolygonPartSource track_part_source(const WorldPolygonFrame &frame)
{
    auto *model = frame.active_track.native_model();
    const auto part = frame.active_track_base_part.native_part();
    if (!model || !part.owner)
        throw FormatError("track descriptor/base-part publication is null");
    // 429EA6/429EB9: descriptor28/38 come from61C52C; part2C/30 and
    // vertices come from61C530. Neither publication is inferred from the other.
    return {part.owner->source_attributes(part.lod, part.part),
            WorldPolygonAttributeView(part.owner->source_coordinates(part.lod, part.part)),
            part.owner->source_vertices(part.lod, part.part),
            model->source_normals(),
            model->source_corner_indices(),
            model->source_model_flags_02(),
            part.owner->source_part_flags_02(part.lod, part.part)};
}
} // namespace
void produce_world_track_base(ModelResource &model, const CameraState &camera,
                              const CameraPresentationState &presentation, CameraLifecycle &lifecycle,
                              const ViewMatrixArena &matrices, const CameraMatrix &identity,
                              const ProjectionParameters &projection, WorldPolygonFrame &frame,
                              std::span<TrackPartAnimation> animations, TrackNormalView normals,
                              TrackGeometryAnimationEffects effects, TrackGeometryAnimationAudit audit)
{
    frame.indexed.require_owner_thread(); // host preflight before recovered stores
    const auto *outer = model.source_outer_transform();
    const auto *spatial = model.source_spatial();
    if (!outer || !spatial)
        throw FormatError("source track base requires its bound outer transform and actual spatial data");
    const auto lod = model.source_lod_index(0);
    const auto count = static_cast<std::uint32_t>(model.part_count(lod));
    if (count == 0)
        throw FormatError("source track base part is absent");
    const auto checked_source = world_polygon_part_source(model, lod, 0);
    if (frame.transformed.count > frame.transformed.allocation.size() ||
        checked_source.vertices.size() > frame.transformed.allocation.size() - frame.transformed.count)
        throw FormatError("source base transformed vertex allocation");
    lifecycle.clip_far = 500000.0f;
    frame.spatial[0].count = 0;
    frame.spatial[1].count = 0;
    // The retained native42B040 caller admits this incoming model as the
    // actual global track before entry. Capture its44 independently of52C.
    const auto captured_base = SourceTrackPartPointer::native(model, lod, 0); // 42ACA4
    frame.spatial_batch_count = 0;                                            // 42ACAE
    frame.active_track = &model;                                              // 42ACB8
    frame.active_track_base_part = captured_base;                             // 42ACBE
    const auto published_part = frame.active_track_base_part.native_part();
    const auto part = published_part.owner->part(published_part.lod, published_part.part);
    const auto source = track_part_source(frame);
    const auto &inverse_outer = resolve_model_matrix(matrices, identity, outer->tail[2]);
    const auto *inverse_part = count > 1 ? &resolve_model_matrix(matrices, identity, part.transform.tail[1]) : nullptr;
    auto &scratch = frame.model_view;
    world_model_local_camera(scratch.local_camera, scratch.normalized_camera,
                             {camera.position.x, camera.position.y, camera.position.z}, outer->position, inverse_outer,
                             part.transform.position, part.pivot, inverse_part, count);
    const auto &pivot = model.source_model_vector_10();
    const CameraModelTransform outer_view{vector(outer->position, outer->position_w),
                                          vector(outer->scale, outer->scale_w),
                                          {pivot.x, pivot.y, pivot.z, pivot.w},
                                          resolve_model_matrix(matrices, identity, outer->tail[1])};
    auto bound_camera = camera;
    bound_camera.world_to_camera_rotation = resolve_model_matrix(matrices, identity, camera.world_to_camera_reference);
    scratch.model_view = camera_model_view(bound_camera, outer_view, count, nullptr); // 42ACCA explicitly passes no
                                                                                      // part
    gather_world_spatial_batches(*spatial, 0, {camera.position.x, camera.position.y, camera.position.z},
                                 presentation.planes, frame.spatial[0], frame.spatial[1]);
    frame.indexed.copy_base_to_current(); // 42ACE1..42ACE7
    const auto clipped = frame.spatial[0].records.first(frame.spatial[0].count),
               inside = frame.spatial[1].records.first(frame.spatial[1].count);
    auto vertices = frame.transformed.allocation.subspan(frame.transformed.count, source.vertices.size());
    // 429886..429896 copies the live camera+198 at this call, independently
    // of the cached projection coefficients.
    auto live_projection = projection;
    live_projection.clip_near = lifecycle.clip_near;
    transform_world_spatial_vertices_clipped(clipped, source.vertices, vertices, frame.indexed, frame.frame_stamp,
                                             scratch.model_view, live_projection);
    transform_world_spatial_vertices_inside(inside, source.vertices, vertices, frame.indexed, frame.frame_stamp,
                                            scratch.model_view, projection);
    emit_world_spatial_polygons(clipped, inside, source, vertices, scratch.local_camera, frame.spatial_batch_count,
                                frame.packets[0], frame.packets[1], frame.packets[2], frame.packets[3]);
    const auto current_base = frame.active_track_base_part.native_part(); // 42ACFC
    frame.transformed.count +=
        static_cast<std::uint32_t>(current_base.owner->source_vertices(current_base.lod, current_base.part).size());
    auto *current_model = frame.active_track.native_model(); // 42AD18
    if (!current_model)
        throw FormatError("track descriptor publication is null at42AD18");
    if (current_model->part_count(current_model->source_lod_index(0)) > 1)
        // 42AD24 restores ECX from retained ESI: the original incoming model,
        // not the separately reread61C52C receiver used only for the predicate.
        animate_track_geometry(model, animations, normals, effects, audit);
}
} // namespace sgn::retail
