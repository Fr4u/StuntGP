#pragma once
#include "sgn/retail_world_material_loading.hpp"
#include "sgn/retail_world_polygon_frame.hpp"
#include "sgn/source_next_scene_adapter_identity.hpp"
#include <thread>

namespace sgn
{
class MaterialD3D12;
}
namespace sgn::retail
{
class RaceSession;
class SourceApplicationCore;
struct SourceColdSceneStartupContext;
struct LightGammaDevice;
// NEW host admission history, never a source readiness word. Cold flags below
// remain a record of the first attempt; a new epoch never clears them.
enum class SourceMaterialEpochPhase
{
    cold,
    invalidated,
    reset_reserved,
    reset_completed,
    script_reserved,
    script_completed,
    models_reserved,
    models_completed,
    renderer_reserved,
    renderer_prefix_completed,
    materials_reserved,
    materials_completed,
    fonts_reserved,
    fonts_count_completed,
    scene_storage_reserved,
    scene_storage_completed,
    camera_reserved,
    camera_completed,
    scene_tail_reserved,
    scene_tail_completed,
    adapters_reserved,
    adapters_ready,
    failed
};
class FrontendState;
class RaceSessionPlatform;
class SceneResources;
class RetailFontResources;
struct FontBankObserver;
struct SourceBootFilePlatform;
struct SourceScriptPathCall;
struct SourceColdMaterialAttachmentOnly
{
};
// New host lifetime owner. Source caches, namespaces and font storage persist.
// An explicit Core-owned transition can invalidate a completed source scene
// and execute the next42E510. Subsequent warm loading is still not admitted.
class SessionWorldMaterials final
{
    // NEW invocation-only access to this existing cold material owner.
    friend class SessionOuterFrameServices;
    friend class SourceApplicationCore;
    friend class SourceFrontendMaterialBinding;
    friend void finish_source_cold_scene_tail_42e778(SourceColdSceneStartupContext &, LightGammaDevice *const &);
    SourceMaterialEpochPhase source_epoch_phase_{SourceMaterialEpochPhase::cold};
    std::uint64_t source_epoch_{1};
    bool source_scene_tail_completed_{};
    void reset_source_scene_resource_stores_42e510();
    void reset_source_next_scene_epoch_42e510();
    // Only the owning Core's reserved next epoch may enter this actual42C510.
    // Retains cold history, font/cache owners and partial writes; no warm-ready
    // admission is conferred on public cold material getters.
    void load_source_next_scene_materials_42e744(SourceBootFilePlatform &, MaterialTextureBackend &,
                                                 WorldMaterialLoadingPresentation &);
    // Core-only next428FA0 and the following6243BC publication. Uses the
    // retained font owner and actual source bank; never clears cold flags or
    // revalidates any previous-epoch material/font borrower.
    void initialize_source_next_scene_fonts_42e74e(SourceBootFilePlatform &, MaterialD3D12 &);
    // Core-only42E778/42E77D/42E782, stopping before camera402420.
    // No cold-tail flag reset, source buffer preclear or borrower revalidation.
    void prepare_source_next_scene_storage_42e778();
    // NEW HOST protocol, only after Core's complete next scene tail. Ordinary
    // resource getters admit adapters_ready only; no cold history is reset.
    void require_next_scene_adapter_resources(const MaterialD3D12 &) const;
    SourceNextSceneAdapterIdentity capture_next_scene_adapter_identity(const MaterialD3D12 &) const;
    void require_next_scene_adapter_identity(const SourceNextSceneAdapterIdentity &, SourceMaterialEpochPhase) const;
    const WorldMaterialLoadingState &next_scene_material_state_for(const SourceNextSceneAdapterIdentity &,
                                                                   const SceneResources &,
                                                                   const MaterialTextureBackend &) const &;
    PhysicalMaterialStorage &next_scene_replacement_storage(const SourceNextSceneAdapterIdentity &,
                                                            const MaterialTextureBackend &) &;
    RetailFontResources &next_scene_fonts(const SourceNextSceneAdapterIdentity &, const MaterialTextureBackend &) &;
    const RetailFontResources &next_scene_fonts(const SourceNextSceneAdapterIdentity &,
                                                const MaterialTextureBackend &) const &;
    std::weak_ptr<RaceSessionPlatform> platform_;
    std::thread::id owner_thread_;
    WorldPolygonFrameTicket ticket_{};
    SceneResources *scene_{};
    bool source_registry_only_{};
    WorldMaterialLoadingState state_;
    // Non-owning host identity of the backend that actually completed load.
    // Token/count equality cannot identify a backend: tokens restart per bank.
    const MaterialTextureBackend *loaded_backend_{};
    // Sole source record+4 words for this cold world. PE636418..636517 is
    // zero;42E510 and42F090 retain these words instead of resetting them.
    std::array<std::uint32_t, 16> registry_flags_{};
    std::array<std::optional<std::size_t>, 14> loaded_models_; // host binding validity, not source identities
    bool attempted_{}, loaded_{}, published_{}, failed_{}, fonts_loaded_{};
    bool source_scene_tail_attempted_{};  // NEW one-shot tail admission, never source readiness
    bool source_scene_reset_attempted_{}; // NEW one-shot cold admission; survives interruption
    bool source_fonts_attempted_{};       // host cold/reentry reservation, not a source word
    std::shared_ptr<RaceSessionPlatform> require_live() const;
    std::shared_ptr<RaceSessionPlatform> require_registry_live() const;

  public:
    // Current synchronous source borrows only. Raw references/spans must never
    // cross a scene boundary; stable adapters retain and check the epoch.
    std::uint64_t source_resource_epoch() const;
    SourceMaterialEpochPhase source_resource_epoch_phase() const;
    void require_source_cold_resource_epoch() const;
    void require_source_resource_epoch(std::uint64_t) const;
    explicit SessionWorldMaterials(RaceSession &);
    // Real source owner attachment, reserved once on the same platform.
    // No source physical-table init or preview load/render readiness.
    SessionWorldMaterials(SourceColdMaterialAttachmentOnly, std::shared_ptr<RaceSessionPlatform>);
    // Actual cold scene2 reset on the same platform and allocation owner.
    void reset_source_cold_scene_resources_42e510();
    // Host admission only, after actual fonts/count, before first42E430 allocation.
    void begin_source_cold_scene_tail_42e778();
    ~SessionWorldMaterials();
    SessionWorldMaterials(const SessionWorldMaterials &) = delete;
    SessionWorldMaterials &operator=(const SessionWorldMaterials &) = delete;
    SessionWorldMaterials(SessionWorldMaterials &&) = delete;
    SessionWorldMaterials &operator=(SessionWorldMaterials &&) = delete;
    // Actual42C510 with the current Session's models and filesystem. Caller
    // supplies source-selected context, a live backend and required loading
    // presentation. No fabricated successful upload or transparency reset.
    void load(WorldMaterialLoadingContext &, MaterialTextureBackend &, WorldMaterialLoadingPresentation * = nullptr);
    // Reached42E744 only, on the already attached canonical source world.
    // Performs42C510 against this same private state/flags and backend.
    // Does not perform preceding startup children, fonts or count publication.
    void load_source_materials_42e744(WorldMaterialLoadingContext &, SourceBootFilePlatform &, MaterialTextureBackend &,
                                      WorldMaterialLoadingPresentation * = nullptr);
    // Reached42E74E only; same source material state/backend and font bank.
    void initialize_source_fonts_42e74e(WorldMaterialLoadingContext &, SourceBootFilePlatform &, MaterialD3D12 &,
                                        FontBankObserver * = nullptr);
    const WorldMaterialLoadingState &source_material_state_for(const SceneResources &,
                                                               const MaterialTextureBackend &) const &;
    const WorldMaterialLoadingState &source_material_state_for(const SceneResources &,
                                                               const MaterialTextureBackend &) const && = delete;
    // Cold42C510 ->428FA0 composition using the same actual backend, source
    // roots, image/reload/material owners and persistent Session font bank.
    // No reentry or callback mutation of roots/viewport/format during this call.
    // General-cache44B1A0 has no legacy cache owner in the native file adapter.
    // Full42E6E0 and warm reload are not implemented by this cold entry.
    void load_with_fonts(WorldMaterialLoadingContext &, MaterialD3D12 &, WorldMaterialLoadingPresentation * = nullptr);
    // Explicit font-only allocation migration. Uses platform.source_allocations
    // after its caller has performed source42EAB0. Does not initialize/begin
    // the bank or qualify earlier42C510 producers/full cold or warm lifecycle.
    // Optional existing read-only font observer; no ownership or initializer.
    void load_with_source_fonts(WorldMaterialLoadingContext &, MaterialD3D12 &,
                                WorldMaterialLoadingPresentation * = nullptr, FontBankObserver * = nullptr);
    // Reached42E772 after source428FA0; same physical cursor and backend.
    void publish_source_count_42e772(const MaterialTextureBackend &);
    // 42E3E0 through42E3FE: caller supplies actual477C30 read; true selects
    // the root sprintf branch owned by Main, which must complete before loading.
    void initialize_source_format_431383(FrontendState &); // sole58332C, no resource construction/reset
    bool initialize_source_material_limits_42e3e0(std::uint32_t word_477c30);
    // Full42E3E0 on this material owner and its SAME platform/App/config.
    // Actual477C30 read precedes Models' prefix; the formatter runs only on !CF.
    // Failure retains quality/max and reached48491C stores, never resets a bank.
    void initialize_source_material_roots_42e3e0(SourceScriptPathCall &);
    void initialize_source_material_roots_42e3e0();
    SourceGraphicsRoots source_graphics_roots() const &;
    SourceGraphicsRoots source_graphics_roots() const && = delete;
    WorldMaterialLoadingContext source_frontend_loading_context(const SourceGraphicsRoots &, WorldMaterialNames);
    std::uint32_t &source_loading_shown_word() &;
    // Reached frontend scene2 slice42E744..42E772; no earlier initialization.
    void load_source_frontend_assets_42e744(WorldMaterialLoadingContext &, SourceBootFilePlatform &, MaterialD3D12 &,
                                            WorldMaterialLoadingPresentation * = nullptr, FontBankObserver * = nullptr);
    // Same source owners after actual fonts and count publication, no race lease.
    PhysicalMaterialStorage &source_frontend_replacement_storage(const MaterialTextureBackend &) &;
    RetailFontResources &source_frontend_fonts(const MaterialTextureBackend &) &;
    const RetailFontResources &source_frontend_fonts(const MaterialTextureBackend &) const &;
    void publish_count_42e772(); // source signed pointer-difference /28, valid owned table
    const WorldMaterialLoadingState &state() const &;
    const WorldMaterialLoadingState &state() const && = delete;
    // Bound synchronous borrow. Rejects a different Session, stale model
    // bindings, incomplete loading or missing source42E772 publication.
    const WorldMaterialLoadingState &validate_for(const RaceSession &) const &;
    const WorldMaterialLoadingState &validate_for(const RaceSession &) const && = delete;
    // No ownership transfer. The original loading backend must remain alive
    // through this check and any bound capture/release operation.
    void require_backend_for(const RaceSession &, const MaterialTextureBackend &) const;
    const RetailFontResources &validate_fonts_for(const RaceSession &) const &;
    const RetailFontResources &validate_fonts_for(const RaceSession &) const && = delete;
    PhysicalMaterialStorage &replacement_storage(const RaceSession &) &;
    PhysicalMaterialStorage &replacement_storage(const RaceSession &) && = delete;
    std::span<std::uint32_t, 16> registry_flags() &;
    std::span<const std::uint32_t, 16> registry_flags_for(const SceneResources &) const &;
    std::span<const std::uint32_t, 16> registry_flags_for(const SceneResources &) const && = delete;
    std::span<std::uint32_t, 16> registry_flags() && = delete;
};
} // namespace sgn::retail
