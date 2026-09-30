#include "sgn/source_cold_scene_tail.hpp"
#include "sgn/source_model_resource_preparation.hpp"
#include "sgn/source_scene_uv.hpp"
#include "sgn/source_track_part_initialization.hpp"
#include "sgn/source_track_contact_pointers.hpp"
#include "sgn/source_world_track_view.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/source_polygon_vertex_clip.hpp"
#include "sgn/source_polygon_indexed.hpp"
#include "sgn/source_polygon_packet_pools.hpp"
#include "sgn/source_screen_base.hpp"
#include "sgn/source_spatial_light_allocation.hpp"
#include "sgn/source_clip_initialization.hpp"
#include "sgn/source_cold_camera_initialization.hpp"
#include "sgn/source_scene_reset_prefix.hpp"
#include "sgn/polygon_device_context.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
PolygonDeviceContext &transform_receiver(DeviceSetupDevice *device)
{
    auto *actual = dynamic_cast<PolygonDeviceContext *>(device);
    if (!actual)
        throw SourceAllocationError(0x431486,
                                    "source transform receiver is not the retained NEW HOST PolygonDeviceContext");
    return *actual;
}
void submit_source_transforms_431420(RaceSessionState &w, DeviceSetupWorld device)
{
    if (device.global_device_5da1c4 == nullptr)
        return; // 431429..430
    const auto projection = std::bit_cast<std::array<std::uint32_t, 16>>(w.frame_projection);
    auto transpose = projection; // 431446 copy toEBP-80
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 4; ++col)
            transpose[row * 4 + col] = projection[col * 4 + row]; // 44EE50, stillEBP-80, NOT passed to slot3
    (void)transpose;
    std::array<std::uint32_t, 16> identity{}; // EBP-40, actual16DWORD clear
    identity[0] = identity[5] = identity[10] = identity[15] = 0x3f800000;
    // Each receiver read is live after the preceding call; ignored HRESULTs.
    (void)transform_receiver(device.global_device_5da1c4).set_transform_represented(1, identity); // 431486
    (void)transform_receiver(device.global_device_5da1c4).set_transform_represented(2, identity); // 431497
    // 43149F is LEA EDX,[EBP-C0], notEBP-80. NEW HOST preserves unspecified
    // stack provenance in actual device state. The sole renderer accepts only
    // pretransformedFVF2C4 vertices; no fabricated projection or C++ UB.
    (void)transform_receiver(device.global_device_5da1c4).set_transform_uninitialized_source_stack(3); // 4314AB
}
} // namespace
void finish_source_scene_post_camera_42e791(SourceColdSceneStartupContext &c,
                                            LightGammaDevice *const &source_gamma_5da1dc)
{
    const auto &platform = c.scene.platform;
    if (!platform)
        throw FormatError("post-camera tail has no retained platform");
    platform->require_owner_thread();
    auto &w = platform->state();
    auto &f = c.renderer.frontend;
    if (f.shared_platform() != platform || &c.renderer.materials != &c.scene.materials || &f.world() != &w)
        throw FormatError("post-camera tail requires the same frontend/platform/material owner");
    auto &scene = platform->source_scene_resources();
    seed_world_model_draws(w.model_draws, scene.models(), scene.model_handle(3), scene.model_handle(2),
                           w.sprite_packets, w.level.ground_level, w.scene_mode, w.view_matrices,
                           &scene);                                // 42E791, actual scene gate and queue producer
    scene.models().initialize_source_menu_lights_42d700(platform); // 42E796
    initialize_source_track_parts_426260(*platform);               // 42E79B, actual current-scene source child
    reset_scene_effects_45e0e0(w.effect_particles);                // 42E7A0
    initialize_source_track_contact_pointers_401d40(*platform);    // 42E7A5, actual scene gate and PMD publications
    reset_view_matrix_cursor(w.view_matrices);                     // 42E7AA
    w.light_fade.last_gamma = 0;                                   // 42E7AF:sole6244F4
    auto reset = c.renderer.calls.reset_context(f);                // NEW borrow only, existing backend/presentation
    if (&reset.frontend != &f)
        throw FormatError("scene tail renderer borrow belongs to another frontend");
    reset.frames.require_source_frontend(f);
    auto *const gamma = source_gamma_5da1dc; // actual Main-owned published slot, not mere CPU object existence
    if (gamma && gamma != &reset.presentation.gamma_device())
        throw FormatError("scene gamma publication differs from the retained presentation receiver");
    set_light_gamma(w.light_fade, 1.0f, gamma); // 42E7BE ->4320C0, actual NULL branch retained
    SourceWorldTrackContext track(scene, w.model_draws, w.sprite_packets);
    (void)track.publish_initial_template_42e7cc(); // actual slot3 NULL branch or descriptor+0C store
    w.outer.device.initialized_6244f8 = 0;         // 42E7D3, keep0 if a later child throws
    submit_source_transforms_431420(w, c.renderer.calls.device_setup_world(f));           // 42E7DD
    fix_source_scene_uv_4312c0(*platform, reset.materials.source_transparency_storage()); // 42E7E2, same backend59207C
    w.outer.device.initialized_6244f8 = 1;       // 42E7E7, AFTER both children normally returned
    w.polygon_frame.low_quality_initialized = 0; // 42E7ED:sole57B41C
}
void SessionWorldMaterials::begin_source_cold_scene_tail_42e778()
{
    const auto platform = require_registry_live();
    require_source_cold_resource_epoch();
    if (!source_registry_only_ || !platform->source_construction_only_entry() || platform->state().scene_mode != 2 ||
        !source_scene_reset_attempted_ || source_scene_tail_attempted_ || failed_ || !fonts_loaded_ || !published_)
        throw FormatError("cold tail requires its once-only actual reset/material/font/count completion");
    source_scene_tail_attempted_ = true; // NEW host reservation; later failures keep it set
}
void finish_source_cold_scene_tail_42e778(SourceColdSceneStartupContext &c,
                                          LightGammaDevice *const &source_gamma_5da1dc)
{
    const auto &platform = c.scene.platform;
    if (!platform)
        throw FormatError("cold tail has no retained platform");
    platform->require_owner_thread();
    auto &w = platform->state();
    auto &f = c.renderer.frontend;
    if (f.shared_platform() != platform || &c.renderer.materials != &c.scene.materials || &f.world() != &w)
        throw FormatError("cold tail requires same frontend/platform/material owner");
    c.scene.materials.begin_source_cold_scene_tail_42e778();
    // Full42E430, all13 allocations/publications in original order.
    (void)allocate_source_polygon_vertex_clip_42e430(platform);
    (void)allocate_source_polygon_indexed_42e47b(platform);
    (void)allocate_source_polygon_packet_pools_42e48c(platform);
    (void)allocate_source_screen_base_42e4d0(platform);
    allocate_source_spatial_light_tail_42e4e1(platform);
    initialize_source_clip_storage_427270(platform); // 42E77D
    prepare_source_model_resources_42e380(platform); // 42E782, actual PMD/generated descriptors and lights
    initialize_source_cold_camera_402420(platform);  // 42E787, Models actual cold entry
    // 42E78C ->44EE00 is exactly RET, no side effect or substitute callback.
    finish_source_scene_post_camera_42e791(c, source_gamma_5da1dc);
    // NEW observation after every actual child/store returned. An attempted
    // tail or initialized6244F8 alone cannot certify a completed resource scene.
    c.scene.materials.source_scene_tail_completed_ = true;
} // 42E7F8 RET, no page/menu lifecycle publication here
void initialize_source_cold_scene_42e6e0(SourceColdSceneStartupContext &c, LightGammaDevice *const &source_gamma_5da1dc)
{
    (void)load_source_cold_scene_through_materials_42e6e0(c);
    finish_source_cold_scene_tail_42e778(c, source_gamma_5da1dc);
}
} // namespace sgn::retail
