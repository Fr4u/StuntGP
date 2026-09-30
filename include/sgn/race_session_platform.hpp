#pragma once
#include <filesystem>
#include <cstdint>
#include <memory>

namespace sgn
{
class AssetStore;
class WindowsAudioDevice;
class WindowsAudioStreamPlatform;
namespace retail
{
struct RaceSessionState;
class WindowInputOwner;
class SourceAllocationBank;
class SourceCameraStackWords;
class SourceContactStackWords;
class SceneResources;
class SourceApplicationLiteralData;
class SourceApplicationLiteralRef;
class SourceMusicPeriodHostAccess;
struct SourceColdConstructionOnly
{
};
struct PreviewHostEntry
{
};
struct PlatformHostReadiness
{
    bool assets{}, audio_device{}, audio_stream{}, window_input{};
};
// New host lifetime. A single allocation owner retains the source world and
// the device/file identities referenced by it across per-level destruction.
// RaceSession::shared_state() aliases this owner's control block; callbacks borrow state
// and never retain a shared_ptr back to the owner.
// All access and the final shared-owner release belong to the creating game
// thread. Destruction on another thread terminates before native HWND/map
// teardown instead of leaving a callback window with a dangling owner.
class SourceMusicCallBinding;
class RaceSessionPlatform final
{
    struct Storage;
    std::unique_ptr<Storage> storage_;
    std::shared_ptr<SourceMusicCallBinding> source_music_calls_; // destroyed before Storage and its source device
    // New host guard for the cold-only material adapter. This survives wrapper
    // destruction and level detach/reattach. Warm loading needs a separately
    // implemented source resource lifecycle; it cannot reset this reservation.
    bool cold_materials_claimed_{};
    bool source_material_owner_claimed_{}; // persistent; never reset on failure/destruction
    friend class SessionWorldMaterials;
    bool session_attachment_claimed_{};
    void claim_session_attachment();
    friend class RaceSession;
    friend class SourceMusicPeriodHostAccess;

  public:
    // No archive I/O, native audio/stream creation, or cold source child.
    // Only the documented bounded PE subset is admitted at this entry.
    RaceSessionPlatform(SourceColdConstructionOnly, std::filesystem::path retail_root, float master_gain = 1.0f);
    // Separate NEW host compatibility bridge: eagerly prepares host backends.
    RaceSessionPlatform(PreviewHostEntry, std::filesystem::path retail_root, float master_gain = 1.0f);
    // Explicit App-owned data is bound before host backend preparation.
    RaceSessionPlatform(PreviewHostEntry, std::filesystem::path retail_root, float master_gain,
                        SourceApplicationLiteralData &);
    explicit RaceSessionPlatform(std::filesystem::path retail_root, float master_gain = 1.0f);
    // NEW one-time weak binding. Same identity is idempotent; replacement is
    // rejected even after expiry. Legacy construction stays truly unbound.
    void bind_source_application_literals(SourceApplicationLiteralData &);
    SourceApplicationLiteralRef source_application_literals() const;
    void prepare_host_assets();
    // Explicit future source owner entry; requires already prepared assets.
    // No native backend, allocation boot, Session or load/render admission.
    void prepare_source_scene_resources();
    SceneResources &source_scene_resources();
    void prepare_host_audio();
    // Reached source stage only: construct/borrow the SAME deferred device
    // owner. Does not prepare COM, XAudio2, stream, or source device itself.
    WindowsAudioDevice &prepare_source_audio_owner();
    // Borrow an already constructed source owner; never constructs a fallback.
    WindowsAudioDevice &source_audio_owner();
    PlatformHostReadiness host_readiness() const;
    bool source_construction_only_entry() const noexcept;
    bool session_attachment_claimed() const noexcept
    {
        return session_attachment_claimed_;
    }
    ~RaceSessionPlatform();
    RaceSessionPlatform(const RaceSessionPlatform &) = delete;
    RaceSessionPlatform &operator=(const RaceSessionPlatform &) = delete;
    RaceSessionPlatform(RaceSessionPlatform &&) = delete;
    RaceSessionPlatform &operator=(RaceSessionPlatform &&) = delete;
    void require_owner_thread() const;
    std::shared_ptr<SourceMusicCallBinding> source_music_calls() const;
    void bind_source_music_calls(std::shared_ptr<SourceMusicCallBinding>);
    // One persistent589E2C bank for all future migrated source producers.
    // Access does not initialize it, register AssetStore storage or qualify
    // warm release. Explicit442143->42EAB0 and producer binding are required.
    // NEW retained relative stack-word owner for the admitted40D5C6 camera
    // call context. No retail numeric ESP or allocation-bank side effects.
    SourceContactStackWords &source_contact_stack_words();
    SourceCameraStackWords &source_camera_stack_words();
    const SourceCameraStackWords &source_camera_stack_words() const;
    SourceAllocationBank &source_allocations();
    const SourceAllocationBank &source_allocations() const;
    // Reached4420E5 ->42EAA0 on this SAME retained bank. No implicit boot,
    // persistent-category override, model/font reset, scene assignment or
    // replacement bank. Existing allocation views expire by generation.
    void release_scene_allocations_42eaa0();
    // Reached44204E ->42EA80: release the old active scope, then write
    // active=1, next ordinal=0 and release base=0 on the existing bank.
    // Keeps category1 unless the source latch allows its release; does not
    // clear frontend/PMD/font descriptors or reset any host admission guard.
    void begin_scene_allocations_42ea80();
    // New host late attachment after HWND creation. Same binding returns the
    // existing owner even across wrapper lifetimes; another HWND/HINSTANCE is
    // rejected. No detach/replacement, source initializer or raw reset here.
    WindowInputOwner &attach_window_input(std::uintptr_t instance, std::uintptr_t window);
    WindowInputOwner &window_input(std::uintptr_t instance, std::uintptr_t window);
    RaceSessionState &state() noexcept;
    const RaceSessionState &state() const noexcept;
    const std::filesystem::path &retail_root() const noexcept;
    const AssetStore &assets() const;
    WindowsAudioDevice &audio_device();
    WindowsAudioStreamPlatform &audio_stream_platform();
};
} // namespace retail
} // namespace sgn
