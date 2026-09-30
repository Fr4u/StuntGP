#pragma once
#include "sgn/source_application_window_start_d3d12.hpp"
#include "sgn/source_cold_scene_startup.hpp"
#include "sgn/source_d3d12_device_production.hpp"
#include "sgn/source_main_menu_children.hpp"
#include "sgn/source_cold_time.hpp"
#include "sgn/source_core_race_update.hpp"
#include "sgn/source_core_race_draw.hpp"
#include <memory>

namespace sgn::retail
{
class SourceApplicationFinalShutdown;
class NativeSourceMenuCrt;
class SourceMenuSampleNative;
class SourceFrontendMaterialBinding;
class SourceApplicationMenuComposition;
struct SourceApplicationHostCallbackState;
struct SourceApplicationShutdownServices;
// NEW host observation of this Core's ordered source calls, no source word.
enum class SourceSceneEpochTransition
{
    cold,
    releasing_images,
    images_released,
    releasing_allocations,
    allocations_released,
    beginning_allocations,
    allocations_begun,
    resetting,
    reset_completed,
    loading_script,
    script_completed,
    preparing_models,
    models_completed,
    preparing_renderer,
    renderer_prefix_completed,
    loading_materials,
    materials_completed,
    initializing_fonts,
    fonts_count_completed,
    preparing_scene_storage,
    scene_storage_completed,
    initializing_camera,
    camera_completed,
    finishing_scene_tail,
    scene_tail_completed,
    rebinding_adapters,
    adapters_ready,
    failed
};
// NEW application lifetime composition. One platform/world, frontend bank,
// material owner, submission scratch, renderer slot and gamma publication.
// Construction prepares host assets only. Source initializers are invoked by
// their recovered callers, never replaced by construction of this owner.
class SourceApplicationCore final
{
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend class SourceApplicationMenuComposition;
    // One Composition claims its actual shutdown receiver while construction
    // is still inert. This also identifies a failed pre-menu startup.
    void bind_menu_lifetime_owner(SourceApplicationShutdownServices &, const SourceApplicationHostCallbackState &);
    // Existing Composition unload dispatch, observed at actual return points.
    void reject_resource_epoch_reentry();
    void release_source_menu_images_431810();
    void release_source_scene_allocations_42eaa0();
    // Existing44204E operation after unload, observed around its actual call.
    // Cold Composition keeps its existing prefix receiver.
    void begin_next_source_scene_allocations_42ea80();
    // Terminal C++ lifetime close only, after the SAME full source shutdown.
    // Composition supplies its retained actual App callback slots, not a claim
    // inferred from a successful retirement hook. Does not run43D180 again.
    void release_menu_after_completed_shutdown(SourceApplicationShutdownServices &,
                                               const SourceApplicationHostCallbackState &);
    // No source shutdown or GPU/input/resource retirement. Destroy only the
    // inert menu/material adapters after actual native callbacks are retired.
    void abandon_menu_adapters(SourceApplicationShutdownServices &, const SourceApplicationHostCallbackState &);

  public:
    SourceApplicationCore(SourceApplicationLiteralData &, std::filesystem::path retail_root, NewHostNativeWindowBorrow,
                          std::unique_ptr<Renderer> &app_renderer, const std::exception_ptr &window_failure,
                          RenderOptions, PolygonSubmissionCaptureLimits, SourceDrawStartupFailure &,
                          std::optional<std::string> host_tz_snapshot);
    ~SourceApplicationCore();
    SourceApplicationCore(const SourceApplicationCore &) = delete;
    SourceApplicationCore &operator=(const SourceApplicationCore &) = delete;
    const std::shared_ptr<RaceSessionPlatform> &platform() const;
    FrontendState &frontend();
    // NEW R17 borrow of the SAME retained sample owner for the 418A90 receiver.
    SourceMenuSampleNative &source_samples();
    // NEW R17 borrow of the CURRENT material adapter for the4473F0/449FB0/40E030
    // receiver. Resolve at each reached child; never retain across a scene epoch.
    SourceFrontendMaterialBinding &source_race_materials();
    SourceWindowMessageWords window_words();
    SourceLateBoundApplicationSurfaces &surface_bridge();
    NewHostD3D12WindowStartup &window_startup();
    SourceD3D12DeviceProduction &device_production();
    // NEW lifetime borrow of the SAME published slot. May be null before
    // 432070 or after4320A0; retain this reference, not a pointer snapshot.
    // SourceApplicationCore must outlive every borrower of the slot.
    LightGammaDevice *const &source_gamma_publication() const &;
    LightGammaDevice *const &source_gamma_publication() const && = delete;
    SessionWorldMaterials &materials();
    SourceSceneEpochTransition source_scene_epoch_transition() const;
    // Explicit NEW host boundary after THIS Core's completed431810 ->42EAA0
    // and the actual44204E ->42EA80 operation, before42E6E6.
    // Call outside native callbacks and with no outstanding synchronous source
    // borrows. Uses the App lvalues retained during original Composition binding;
    // their depth is not proof of arbitrary spans or source-stack quiescence.
    // No callback/provider is destroyed. Executes actual42E510 only; does not
    // begin a bank or enable warm loading. Caller must obey synchronous borrows.
    void reset_next_source_scene_epoch_42e510();
    // Actual42E6E6 CALL42D070 after this Core's completed next-epoch reset.
    // Uses this Core's existing platform/files and full4453A0 children. The
    // source reads selection_phase itself; no scene/target value is imposed.
    // One attempt, including failure. Same closed synchronous-borrow/native
    // callback boundary as reset. Returns at42E6EB BEFORE44FD60; no model,
    // renderer, material, font or full42E6E0 completion is implied.
    void load_next_source_level_script_42e6e6();
    // Actual42E6EB/42E6F0/42E6F5 after this Core's completed42D070:
    // 44FD60,4342C0,42F090, on the SAME allocation bank, model registry,
    // live selection/cars and sprite owners. One reserved attempt; a failure
    // retains all reached source writes/allocations and permanently closes
    // this epoch. No source borrow may cross this synchronous entry.
    // Returns at42E6FA BEFORE reading6244F8. Does not reset/recreate renderer,
    // upload materials, initialize fonts or enable full warm loading.
    void prepare_next_source_scene_models_42e6eb();
    // Reached42E6FA after this Core's completed42F090. Reads6244F8 once:
    // nonzero runs full431300 on the retained renderer; zero enters actual
    // 431360 (its live-surface recreation boundary remains explicit).
    // Then429510,42E3E0,425070,413190/413220; returns BEFORE42E744/42C510.
    // One attempt, same synchronous-borrow rules. Three clear/Flip passes
    // have scoped access to this epoch's empty physical table only; ordinary
    // menu/font/material borrowers remain invalid. Partial source writes and
    // GPU presentations survive failure. Does not enable Composition warm load.
    void prepare_next_source_scene_renderer_42e6fa();
    // Reached42E744 ->42C510 on the same next-epoch models/materials/backend,
    // then42E749 ->44B1A0 on the actual WAD cache. Uses actual scene selection,
    // source roots, registry flags and loading-presentation owner. Reserves one
    // attempt, retaining all reached source writes/uploads on failure.
    // Returns BEFORE42E74E/428FA0; no new font/count/tail or drawing readiness.
    // Cold claims/guards remain; Composition warm loading is still disabled.
    void load_next_source_scene_materials_42e744();
    // Actual42E74E ->428FA0 on the same current font/allocation/material owners,
    // followed by42E753..42E772 physical-count publication. Prior font records,
    // scratch history and cold flags are retained until source writes replace
    // them. Partial allocation/upload failure permanently fails this epoch.
    // Returns BEFORE42E778/42E430; no tail or menu drawing readiness is implied.
    void initialize_next_source_scene_fonts_42e74e();
    // Actual42E778 ->42E430 (all13 allocations/publications),42E77D ->427270,
    // then42E782 ->42E380 on this same current source registry/light owner.
    // One attempt after this epoch's font/count completion. Old allocation
    // borrows stay stale; only reached source base/cursor stores replace them.
    // Returns BEFORE42E787/402420. No camera/light-setup, full scene-tail or
    // ordinary drawing readiness; Composition warm loading remains guarded.
    void prepare_next_source_scene_storage_42e778();
    // Actual42E787/402420 with this Core's retained files/CRT and source camera bank.
    // Requires completed current storage; retains partial writes on failure.
    // Returns before42E791/42B740; no full-tail or drawing readiness is implied.
    void initialize_next_source_camera_42e787();
    // Actual42E791..42E7F8: source queues/lights/track/effects/contact/matrices,
    // same published gamma receiver, track template, transforms and scene UV.
    // One attempt after this epoch's camera; preserves partial writes on failure.
    // Does not enable stale material/font adapters or the Composition race loop.
    void finish_next_source_scene_tail_42e791();
    // NEW HOST transaction, no PE entry: after THIS Core's complete42E7F8,
    // capture current resource identities, create a fresh material adapter and
    // atomically rewire the retained menu and frame receivers. Old adapters
    // stay alive but reject access through their original epoch. One attempt;
    // failure after reservation permanently closes the epoch. Requires idle
    // App/presentation and no outstanding synchronous source borrows. This
    // does not call442810/4427B0 or enable Composition warm/race dispatch.
    void rebind_next_source_scene_adapters();
    // Actual442810 composition only, on the same retained source owners. The
    // caller supplies one monotonically increasing accepted-source serial, not
    // a refresh-derived tick. Missing mandatory world children throw at their
    // source callsites; any partial update permanently poisons this cadence.
    // The real441DC0 may change scene/resources before return. No4427B0 here.
    SourceOuterOutcome update_source_race_442810(std::uint64_t accepted_outer_serial, SourceCoreRaceUpdateRequired &,
                                                 SceneTransitionServices &, SourceOuterTrace * = nullptr);
    // Actual4427B0 with this Core's same world, images, submission and device.
    // The draw serial is independent from update cadence. Reentry/partial draw
    // poisons this entry and discards capture without undoing source writes.
    // Missing entities/HUD/menu children remain required; no race activation.
    SourceCoreRaceDrawKind draw_source_race_4427b0(std::uint64_t accepted_draw_serial, SourceCoreRaceDrawRequired &);
    SessionOuterFrameServices &frames();
    SourceMenuPresentationBackend &presentation();
    FrontendPerformanceCounter &counter();
    SourceColdTimePlatform &time();
    SourceBootFilePlatform &files();
    SourceApplicationWindowClock &window_clock();
    SourceColdSceneStartupContext cold_scene_context();
    // NEW HOST current identities, not owner copies or source readiness words.
    InputWindowHandles native_window_handles() const;
    // Inert adapter borrowing this Core's actual frontend/device/surface and
    // App HWND/Renderer slots. Core must outlive the returned adapter.
    std::unique_ptr<SourceApplicationFinalShutdown> make_final_shutdown_adapter();
    // No source menu initialization here. The actual device producer must
    // already have run. All receivers outlive every use of the returned menu;
    // CRT is bound to this Core, shutdown is Composition's RetiringShutdown.
    // Composition must have claimed that shutdown receiver before attachment.
    // Repeated attachment admits only the same four service identities.
    SourceMainMenuChildren &attach_menu_after_device(SourceSavePageServices &, SourceMainMenuRequiredServices &,
                                                     NativeSourceMenuCrt &,
                                                     SourceApplicationShutdownServices &same_retiring_shutdown);
    SourceMainMenuChildren &menu();
};
} // namespace sgn::retail
