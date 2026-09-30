#include "sgn/session_world_materials.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_scene_resource_prefix.hpp"
#include "sgn/source_scene_indexed_base.hpp"
#include "sgn/source_scene_keyboard_prefix.hpp"
#include "sgn/source_scene_packet_bases.hpp"
#include "sgn/source_screen_base.hpp"
#include <limits>

namespace sgn::retail
{
void ModelResources::reset_source_track_globals_42e656()
{
    auto &loading = source_loading_;
    auto &requests = loading.requests;
    requests.check_thread(0x42e656);
    requests.track_name_ = {}; // 42E656:624460; NOT owned by42EB00
    auto &track = loading.track;
    track.spatial_vertex_indices = {};  // 42E65C:624464
    track.spatial_polygon_indices = {}; // 42E662:624468
    track.spatial_bounds = {};          // 42E668:62446C
    track.spatial_nodes = {};           // 42E66E:624470
    track.splines = {};                 // 42E674:624474
    track.opaque58 = {};                // 42E67A:624478
    track.lights = {};                  // 42E680:62447C
    track.collision = {};               // 42E686:624480
    track.texture_indices = {};         // 42E68C:624484
    track.texture_motion = {};          // 42E692:624488
    track.normals = {};                 // 42E698:62448C
    track.generated_normals = {};       // 42E69E:624490
    track.ambient = {};                 // 42E6A4:624494
    track.spline_count = 0;             // 42E6AA:624498
    track.light_count = 0;              // 42E6B0:62449C
    track.render_light_count = 0;       // 42E6B6:6244A0
    track.opaque58_count = 0;           // 42E6BC:6244A4
}

void SessionWorldMaterials::reset_source_cold_scene_resources_42e510()
{
    // NEW admission remains cold-only. Independent61C52C/61C530 publications
    // do not authorize reuse of material/font owners or complete a warm loader.
    const auto platform = require_registry_live();
    require_source_cold_resource_epoch();
    if (!source_registry_only_ || !platform->source_construction_only_entry())
        throw FormatError("42E510 cold entry requires the existing source material attachment");
    const auto &world = platform->state();
    const auto &frame = world.polygon_frame;
    if (world.scene_mode != 2)
        throw FormatError("42E510 cold entry requires the actual caller scene2 store");
    if (!platform->source_allocations().initialized())
        throw FormatError("42E510 cold entry requires earlier442143; it cannot initialize the bank");
    if (source_scene_reset_attempted_ || attempted_ || loaded_ || failed_ || source_fonts_attempted_)
        throw FormatError("42E510 cold entry cannot replay reset or reuse a loaded material owner");
    if (frame.allocations || frame.active_track || frame.active_track_base_part || scene_->models().size() != 0)
        throw FormatError("42E510 cold entry cannot reset preview or previously loaded model resources");
    // Canonical host/source lifetime guards precede all source stores. The
    // reservation survives a later exception; no retry or owner replacement.
    (void)registry_flags_for(*scene_);
    source_scene_reset_attempted_ = true;
    reset_source_scene_resource_stores_42e510();
}

void SessionWorldMaterials::reset_source_next_scene_epoch_42e510()
{
    const auto platform = require_registry_live();
    if (!source_registry_only_ || !platform->source_construction_only_entry() ||
        source_epoch_phase_ != SourceMaterialEpochPhase::invalidated || !source_scene_tail_completed_ || failed_)
        throw FormatError("next42E510 requires the invalidated completed source epoch");
    const auto &bank = platform->source_allocations();
    if (!bank.initialized() || bank.source_words().active != 1)
        throw FormatError("next42E510 requires the same bank after actual44204E ->42EA80");
    if (source_epoch_ == std::numeric_limits<std::uint64_t>::max())
        throw FormatError("source material epoch identity exhausted");
    // Host reservation precedes every source store and survives any failure.
    // No cold flags, names, fonts, cache, source counter or bank is fabricated.
    source_epoch_phase_ = SourceMaterialEpochPhase::reset_reserved;
    ++source_epoch_;
    try
    {
        reset_source_scene_resource_stores_42e510();
        source_epoch_phase_ = SourceMaterialEpochPhase::reset_completed;
    }
    catch (...)
    {
        source_epoch_phase_ = SourceMaterialEpochPhase::failed;
        throw;
    }
    // 42E6E6/42D070 is a separate Core reservation. No renderer, material,
    // font or tail admission follows from completing only this source reset.
}

void SessionWorldMaterials::reset_source_scene_resource_stores_42e510()
{
    const auto platform = require_registry_live();
    auto &world = platform->state();
    auto &frame = world.polygon_frame;
    (void)reset_source_scene_resource_prefix_42e510(*scene_, *this); // 42E519..41C710
    (void)reset_source_scene_indexed_base_42e53c(platform);
    (void)reset_source_scene_keyboard_42e542(platform);     // both64-DWORD regions
    (void)reset_source_scene_packet_bases_42e55a(platform); // bases2,3,0,1
    (void)reset_source_scene_screen_base_42e572(platform);  // retains records/count
    frame.spatial[0].records = WorldSpatialRecords{};       // 42E578:61C518
    frame.spatial[1].records = WorldSpatialRecords{};       // 42E57E:61C51C
    // Clear only624530 publication. No release of the bank payload and no
    // 624574 working-cursor store; the old allocation remains owned by bank.
    world.light_vertices.bind_source(SourceWorldSpan<std::uint8_t>::source(platform, {}, 0)); // 42E584
    for (auto &matrix : world.view_matrices.matrices)
        matrix.fill(0.0f);                                                // 42E58A:6144DWORD
    world.view_matrices.cursor.reset();                                   // 42E596:6227CC=NULL, not active base
    world.view_matrices.count = 0;                                        // 42E59C:6227D0
    frame.active_track.clear();                                           // 42E5A2:61C52C, leaves61C530 untouched
    frame.active_track_base_part.clear();                                 // 42E5A8:61C530, independent store
    world.ui.selected_material = 0xffffffffu;                             // 42E5AE:61C5E4
    frame.transformed.allocation = SourceWorldSpan<WorldPolygonVertex>{}; // 42E5B8:61C50C=NULL
    frame.transformed.count = 0;                                          // 42E5BE:61C510=NULL, base is alreadyNULL
    frame.remaining_first = 0;                                            // 42E5C4:61C514
    frame.source_word_61c5e8 = 0;                                         // 42E5CA:61C5E8; no inferred use
    frame.indexed.clear_current();                                        // 42E5D0:61C520
    world.screen_polygon_count = 0;                                       // 42E5D6:61C604
    // Existing sole descriptor owner. No source child/callback occurs between
    // 42E5DC's112-DWORD clear and42E5E8'snull cursor publication.
    world.effect_resources.animations.clear_scene_42e510();
    // Existing physical table, not namespace/reload/cache or registry+4 flags.
    // This clears1785DWORD at42E5EE then nulls6243B8 at42E5FA;6243BC survives.
    state_.names.materials.reset_scene_42e510();
    for (unsigned pool = 0; pool < 4; ++pool)
    { // 42E600:6243C0..6243DF
        frame.packets[pool].publish_current({});
        frame.totals[pool] = 0;
    }
    for (auto &group : frame.groups)
        for (auto &range : group)
        { // 42E60C:32DWORD
            range.first = {};
            range.count = 0;
        }
    frame.spatial[0].count = 0;                                       // 42E618:61C524=NULL, base alreadyNULL
    frame.spatial[1].count = 0;                                       // 42E61E:61C528=NULL, base alreadyNULL
    frame.clip.sequential_indices.fill(0);                            // 42E624:eightDWORD/16WORD
    frame.clip.attributes = SourceWorldSpan<WorldPolygonAttribute>{}; // 42E626
    frame.clip.headers = SourceWorldSpan<WorldPolygonHeader>{};       // 42E62C
    frame.clip.unused_indices = SourceWorldSpan<std::uint16_t>{};     // 42E632
    frame.clip.vertices = SourceWorldSpan<WorldPolygonVertex>{};      // 42E638
    // Independent stores, with all bases alreadyNULL. Use pointer-object
    // replacement, not native-count operator= on an old source binding.
    frame.clip.attribute_count = SourceClipCurrentPointer{};    // 42E63E:6244D8
    frame.clip.header_count = SourceClipCurrentPointer{};       // 42E644:6244DC
    frame.clip.unused_index_count = SourceClipCurrentPointer{}; // 42E64A:6244E0
    frame.clip.vertex_count = SourceClipCurrentPointer{};       // 42E650:6244E4
    scene_->models().reset_source_track_globals_42e656();       // all18 existing owners
    (void)scene_->models().reset_source_resources_42eb00();     // 42E6C4 JMP, full leaf
    // No bank begin/init/release, payload destruction, source flags reset,
    // font/cache/texture upload, UI readiness or full42E6E0 success is implied.
}
} // namespace sgn::retail
