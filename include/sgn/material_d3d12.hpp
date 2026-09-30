#pragma once
#include "sgn/renderer.hpp"
#include "sgn/polygon_submission.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <thread>

namespace sgn
{
struct MaterialDeviceRecovery;
// NEW explicit recovery of cold-menu front/back across real loader resets.
struct SourceMenuMaterialRecovery
{
};
// Sole transparency-byte owner: independent of physical material records and
// the GPU resource bank. The initial PE pointer is null until explicit430690.
struct MaterialTransparencyStorage
{
    std::array<std::uint8_t, 255> bytes{};
    std::optional<std::uint32_t> cursor;
};

// New D3D12 adapter for the qualified successful file/generated upload domain.
// WorldMaterialLoadingState remains the sole published material-table owner.
// GPU tokens and snapshot epochs are host identities, not physical indices.
class MaterialD3D12 final : public retail::MaterialTextureBackend
{
    Renderer *renderer_; // Suspended between old-device destruction and atomic recovery.
    MaterialTransparencyStorage &transparency_;
    const bool separate_stage_surfaces_;
    const std::thread::id owner_;
    std::shared_ptr<PolygonGpuTextures> live_;
    std::shared_ptr<const MaterialDeviceRecovery> recovery_; // CPU-only during device recreation.
    std::uint64_t next_token_{1};
    std::uint32_t resource_entries_{};
    // 5D91D0 resource entries are independent of aliases in the material table.
    std::array<std::uint32_t, 255> primary_tokens_{};
    bool pending_{}, failed_{};
    // NEW HOST (R35) resident 430D40 replacement versions: the GPU texture of every recent replacement
    // (exact source words and recorded profile), so swapping an image it showed before into a destination
    // (of the same profile) reuses that immutable texture instead of converting and uploading it again. Cleared
    // whenever the bank is reset or the renderer is suspended.
    std::vector<PolygonGpuTexture> resident_;
    std::size_t resident_bytes_{};
    const PolygonGpuTexture *find_resident(const PolygonTextureProfile &, const retail::PreparedMaterialPixels &,
                                           const ID3D12Resource *current) const;
    void keep_resident(const PolygonGpuTexture &);
    void check_thread() const;
    void suspend_renderer_impl(std::span<ImmutablePolygonSubmission>, bool source_menu_history);
    PolygonSurfaceRoles rebind_renderer_impl(Renderer &, std::span<ImmutablePolygonSubmission>,
                                             std::span<const SdrGammaRamp>);
    struct PendingUpload
    {
        std::array<std::uint32_t, 2> tokens;
        std::uint8_t transparency;
    };
    PendingUpload prepare_upload();
    retail::CompletedMaterialUpload complete(std::vector<PolygonGpuTexture>, std::uint32_t width, std::uint32_t height,
                                             PendingUpload);

  public:
    MaterialD3D12(Renderer &, MaterialTransparencyStorage &, bool separate_stage_surfaces,
                  std::uint64_t first_resource_epoch);
    MaterialD3D12(const MaterialD3D12 &) = delete;
    MaterialD3D12 &operator=(const MaterialD3D12 &) = delete;
    void release_all() override;
    std::uint8_t &current_transparency() override;
    void begin_pending_record(std::uint32_t motion_index) override;
    retail::CompletedMaterialUpload upload(retail::PreparedMaterialPixels) override;
    retail::CompletedMaterialUpload generate(std::uint32_t dimension) override;
    // 4325E0 successful font domain: one256x256primary destination regardless
    // of ordinary alternate policy. Returns host texture token; advances only
    // the GPU resource bank. The outer font target owns physical publication.
    std::uint32_t upload_font_primary(retail::PreparedMaterialPixels);
    void require_font_upload_ready() const;
    // Dispatch only for the separate source430690 event, never infer it from
    // release_all or a physical material-table reset.
    void reset_transparency_430690();
    // Synchronous read-only borrow of the SAME59207C table used by uploads.
    // Caller must keep this backend and its source scene bound for the call.
    const MaterialTransparencyStorage &source_transparency_storage() const &
    {
        check_thread();
        return transparency_;
    }
    const MaterialTransparencyStorage &source_transparency_storage() const && = delete;
    // Copies resource handles, not texture payloads. Older snapshots keep their
    // actual GPU objects until their presentation owner retires them.
    std::shared_ptr<const PolygonGpuTextures> bindings_snapshot() const;
    std::uint64_t resource_epoch() const;
    std::uint32_t resource_entries() const;
    std::uint32_t resource_primary_token(std::uint32_t slot) const;
    // 430D40 loads only resource-entry+4. Replaces its actual GPU object while
    // retaining the source interface token and immutable earlier snapshots.
    void replace_primary(std::uint32_t slot, retail::PreparedMaterialPixels);
    // NEW host lifetime operations, not source reset/reload/replacement.
    // Coordinator must be healthy, idle and outside BeginScene. No source
    // ticks, physical writes, selected publication, tokens or epoch increments.
    bool bound_to_renderer(const Renderer &) const;
    void suspend_renderer_for_recreation(ImmutablePolygonSubmission &displayed);
    void rebind_renderer(Renderer &, ImmutablePolygonSubmission &displayed);
    // Same recovery graph, extended across two front/back recipes and live bank.
    void suspend_renderer_for_recreation(std::span<ImmutablePolygonSubmission>);
    void suspend_renderer_for_recreation(SourceMenuMaterialRecovery, std::span<ImmutablePolygonSubmission>);
    PolygonSurfaceRoles rebind_renderer(Renderer &, std::array<ImmutablePolygonSubmission, 2> &,
                                        const std::array<SdrGammaRamp, 2> &);
};
} // namespace sgn
