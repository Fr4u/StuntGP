#pragma once
#include "sgn/scene_resources.hpp"
#include "sgn/world_model_draw.hpp"
#include "sgn/retail_camera_transform.hpp"
#include "sgn/source_track_publications.hpp"
#include "sgn/source_track_geometry_animation.hpp"
#include <span>

namespace sgn::retail
{
struct CameraLifecycle;
struct ProjectionParameters;
struct WorldPolygonFrame;
struct WorldModelViewScratch;
struct WorldLocalCameraAudit;
// NEW read-only observation after each reached source store. A throw retains
// the writes already reached. The observer must not mutate or retire any of
// the invocation's owners/bindings; it is not a recovered game callback.
struct SourceTrackBasePrefixObserver
{
    virtual ~SourceTrackBasePrefixObserver() = default;
    virtual void stored(std::uint32_t source_site, const CameraLifecycle &, const WorldPolygonFrame &) = 0;
};
// Synchronous borrow of one real scene and its registered draw/sprite owners.
// No PMD, pose, LOD, bank, active-track or Session payload is retained/copied.
// Caller keeps these objects alive/stable throughout the borrow. Methods
// recheck actual scene binding and PMD lifetime; a returned pose reference
// must not escape the synchronous caller or be reused after owner destruction.
class SourceWorldTrackContext
{
    SceneResources &scene_;
    WorldModelDrawState &draws_;
    const SourceSpritePacketStorage &sprites_;
    void validate_scene() const;
    // Called only after scene admission; resolves this draw owner's external
    // pose identity without imposing model==636448 on a separate source ECX.
    const ModelTransform &resolve_outer_reference(std::uint32_t captured_reference) const;
    SourceAllocationBank &spatial_bank() const;

  public:
    SourceWorldTrackContext(SceneResources &, WorldModelDrawState &, const SourceSpritePacketStorage &);
    SourceWorldTrackContext(const SourceWorldTrackContext &) = delete;
    SourceWorldTrackContext &operator=(const SourceWorldTrackContext &) = delete;
    SourcePmdModelView track() const;
    void require_track(SourcePmdModelView) const;
    // Field-specific source-address-shaped numeric keys, never host pointers
    // or PMD-relative offsets.0 remains absent; arbitrary foreign poses reject.
    std::uint32_t pose_reference(const ModelTransform &) const;
    const ModelTransform &resolve_captured_outer(SourcePmdModelView, std::uint32_t captured_reference) const;
    bool publish_initial_template_42e7cc() const;
    void publish_background_track_42b072() const;
    // Caller supplies the real61C52C model borrow. This method only implements
    // the reached store; it does not run42ADD0 lighting or the rate gate.
    void publish_active_track_template_42ae39(SourcePmdModelView actual_active_track) const;
    // Complete42B2A0 adapter, including existing44ED40 math. Arguments are the
    // independently captured source model descriptor and part, not inferred
    // from each other or reread from61C52C/61C530. Source fields and matrices
    // are borrowed from their actual owners; no native ModelResource mirror.
    // Part+08/position is required even for count0/1. Pivot and inverse part
    // are resolved only for unsigned count>1. Existing opaque-W policy is
    // retained: count<=1 source-uninitialized W keeps local.W; normalized.W
    // copies it. Audit records that limitation, not a fabricated source byte.
    // Input guards precede output stores; hardware FP-trap/intermediate-fault
    // parity is not claimed. Caller supplies stable, distinct actual outputs.
    void local_camera_42b2a0(SourceTrackModelPointer incoming_descriptor, SourceTrackPartPointer captured_part,
                             const CameraState &, const ViewMatrixArena &, const CameraMatrix &shared_identity,
                             WorldModelViewScratch &, WorldLocalCameraAudit * = nullptr) const;
    // Complete44FBD0 for42ACD1's literal EDX=0. Even count>1 must skip ALL
    // part/LOD/descriptor44 accesses. ECX is the retained original input,
    // independent of this scene's current636448 and published61C52C/61C530.
    // Destination is the supplied scratch's actual61C428. The captured
    // descriptor+0C resolves within this context's same draw-pose owner.
    // 44EE10 identity is committed after+0C, before+24 and later guards.
    // Existing camera_model_view arithmetic commits the remaining result
    // after its required inputs are admitted. Thus a later host guard keeps
    // identity; partial x87-store/FP-trap parity is not claimed. No W lane is
    // read from the PMD pivot:44FBD0 uses only+10/+14/+18. Caller keeps actual
    // camera, arena, identity and scratch owners stable and distinct.
    void base_model_view_44fbd0(SourceTrackModelPointer incoming_descriptor, const CameraState &,
                                const ViewMatrixArena &, const CameraMatrix &shared_identity,
                                WorldModelViewScratch &) const;
    // Complete42AA70: capture the actual loader62446C, read reached XYZ only,
    // preserve early return and the existing PC53/round-nearest numeric policy.
    // Caller borrows current4722D0 position and its actual+118..+150 planes.
    std::uint32_t classify_spatial_bounds_42aa70(std::uint16_t first, const CameraState &,
                                                 std::span<const CameraVector, 4> actual_planes) const;
    // Complete42ABB0 traversal on captured ECX; child recursion reloads624470.
    // No native ModelSpatialData mirror. Actual source-bank two-DWORD records
    // receive existing allocator address tokens, then normalized cursor count.
    // Polygons store, cursor advance, vertices store retain source order.
    // Tokens admit live allocation/offset or NULL0; invalid pointer arithmetic
    // and ancestor cycles are explicit host errors, not source success.
    // No list sentinel/contents are read here. Each selected list global is
    // captured independently; no inference from current/incoming track model.
    void gather_spatial_batches_42abb0(SourcePmdLocation captured_spatial_root, const CameraState &,
                                       std::span<const CameraVector, 4> actual_planes, WorldPolygonFrame &) const;
    // Complete source-PMD429880/429730. Consume lane1 address tokens from
    // actual spatial records, read sentinel WORDs lazily and borrow source
    // part+30 vertices. No count/header/W read or native geometry mirror.
    // Caller supplies actual61C3E8/3FC/410/420 and current-camera+198 as
    // ProjectionParameters, stable through invocation with the same frame.
    // Inside captures61C530 once; clipped rereads it per changed vertex.
    // Source16-bit stamp compares to the full32-bit captured frame. Indexed
    // payload stores occur during traversal; current publishes only on return.
    // Win64 x87 leaves preserve original spills/masks and the caller's CW.
    // Valid source-bank/relocated-part/offset/XYZ extents are the host domain;
    // invalid-pointer and intermediate hardware-fault parity is not claimed.
    void transform_spatial_vertices_clipped_429880(const ProjectionParameters &, WorldPolygonFrame &) const;
    void transform_spatial_vertices_inside_429730(const ProjectionParameters &, WorldPolygonFrame &) const;
    // Complete42AC70, including42A220, transformed-current tail and426ED0.
    //  incoming_descriptor is the caller's already-captured source ECX, NOT
    //  inferred from this context's scene slot3/636448. Source NULL is admitted;
    //  nonnull native ModelResource pointers are a host admission error.
    //  It is copied to61C52C without dereference/re-reading a PMD header.
    //  The separate part comes from the captured slot3 model's current+44,
    //  read at42ACA4, and is published without dereferencing that part.
    //  Caller supplies its actual4722D0 camera lifecycle and polygon frame,
    //  and keeps their base bindings stable for this synchronous invocation.
    //  Six observed prefix stores plus camera/matrix outputs, spatial lists and
    //  indexed-current publication are made. Observer covers only six stores.
    //  Caller supplies current camera's actual presentation planes; no cached
    //  substitute is derived. Animation borrows the SAME session owners; the
    //  warm loader/426260 admission is not bypassed by this producer.
    void produce_base_prefix_42ac70(SourceTrackModelPointer incoming_descriptor, const CameraState &, CameraLifecycle &,
                                    const ViewMatrixArena &, const CameraMatrix &shared_identity,
                                    std::span<const CameraVector, 4> actual_planes,
                                    const ProjectionParameters &actual_projection, WorldPolygonFrame &,
                                    SourceTrackAnimationOwners &, SourceTrackBasePrefixObserver * = nullptr,
                                    WorldLocalCameraAudit * = nullptr) const;
};

// Optional statement-boundary observation, not a transitive PE trace. A test
// observer may throw to stop at a documented prefix. Ordinary calls omit it.
struct SourceTrackInitialViewAudit
{
    void *context{};
    void (*reached)(void *, std::uint32_t source_site){};
};
// Bounded42B040 prefix through its initial44FBD0. Does not publish42B072 or
// run any world geometry/groups/material work. Caller borrows actual61C594,
// 6249AC, current camera, arena and shared identity from its invocation owner.
// Host preflight failure: no recovered writes. After entry: accepted=0 and
// the actual destination is initialized at44FBE6. Later guarded-input failure
// retains that identity. Arithmetic commits once after all guards; intermediate
// x87 matrix-store failure parity is explicitly NOT implemented by this API.
void prepare_source_track_initial_view(const SourceWorldTrackContext &, const CameraState &, const ViewMatrixArena &,
                                       const CameraMatrix &shared_identity, CameraMatrix &destination,
                                       std::uint32_t &accepted_models, SourceTrackInitialViewAudit = {});
} // namespace sgn::retail
