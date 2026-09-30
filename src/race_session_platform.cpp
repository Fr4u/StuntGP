#include "sgn/race_session_platform.hpp"
#include "source_music_period_owner.hpp"
#include "sgn/source_application_literals.hpp"
#include <optional>
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/window_input_owner.hpp"
#include "sgn/source_allocation_bank.hpp"
#include "sgn/source_camera_stack_words.hpp"
#include "sgn/source_contact_stack_words.hpp"
#include <thread>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <bit>

namespace sgn::retail
{
struct RaceSessionPlatform::Storage
{
    const std::thread::id owner_thread{std::this_thread::get_id()};
    std::filesystem::path root;
    // Weak App identity only. A platform never initializes or owns these bytes.
    std::optional<SourceApplicationLiteralRef> application_literals;
    // Lifetime only: no42EAB0/42EA80 call in this or AssetStore construction.
    // Declared before consumers, so final host cleanup happens after them.
    SourceAllocationBank source_allocations;
    // Host relocation metadata must outlive every camera/query payload.
    // It never calls source42E8D0 and survives scene allocation release.
    SourceCameraStackWords camera_stack_words;
    SourceContactStackWords contact_stack_words;
    const float master_gain;
    bool source_entry{true};
    std::unique_ptr<AssetStore> assets;
    // Once per platform allocation, in actual existing world owners. This is
    // the 18 identified PE rows, not a full-world PE qualification or cold call.
    RaceSessionState world;
    // NEW native period accounting, independent of source allocated/ready.
    // Destroyed after later native consumers and before the source world.
    std::unique_ptr<SourceMusicPeriodOwner> music_periods;
    // One canonical registry; destroyed before its borrowed geometry/assets/bank.
    std::unique_ptr<SceneResources> source_scene;
    bool source_scene_claimed{};
    std::unique_ptr<WindowsAudioDevice> device;
    std::unique_ptr<WindowsAudioStreamPlatform> stream;
    // Last member: finalized before every raw borrowed source/backend owner.
    std::unique_ptr<WindowInputOwner> input;
    static float checked_gain(float value)
    {
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::invalid_argument("audio master gain outside0..1");
        return value;
    }
    Storage(std::filesystem::path path, float gain) : root(std::move(path)), master_gain(checked_gain(gain))
    {
        // Retain default-initialization of all other source storage; do not
        // aggregate-value-initialize and accidentally define unwritten words.
        world.random.state = 1;                            // 4959C0
        world.audio_level.scale = 255;                     // 489790
        world.audio_initialization.requested_rate = 22050; // 489794
        world.audio_level.output_b = 255;                  // 489798
        world.audio_level.output_b_copy = 255;             // 48979C
        world.audio_initialization.spatial_capability = 1; // 4897AC
        world.audio_initialization.spatial_enabled = 1;    // 4897B0
        world.audio_driver.spatial_mode = 1;               // 4897B4
        // Remaining proven PE values, once in these existing owners. No
        // initialize_world_gravity, audio_set_range or source reset is called.
        world.gravity.factor = std::bit_cast<float>(0x3f800000u);            // 482668
        world.gravity.inverse_factor = std::bit_cast<float>(0x3f800000u);    // 48266C
        world.gravity.magnitude = std::bit_cast<float>(0x3e8b851fu);         // 482670
        world.gravity.inverse_magnitude = std::bit_cast<float>(0x406adcc5u); // 482674
        world.gravity.vector.x = std::bit_cast<float>(0x00000000u);          // 482678
        world.gravity.vector.y = std::bit_cast<float>(0xbe8b851fu);          // 48267C
        world.gravity.vector.z = std::bit_cast<float>(0x00000000u);          // 482680
        // PE482684 is an unmodeled four-byte gap before unit, not host padding
        // to memcpy into WorldGravity. Its zero bytes are captured as evidence.
        world.gravity.unit.x = std::bit_cast<float>(0x00000000u);             // 482688
        world.gravity.unit.y = std::bit_cast<float>(0xbf800000u);             // 48268C
        world.gravity.unit.z = std::bit_cast<float>(0x00000000u);             // 482690
        world.gravity.unit.w = std::bit_cast<float>(0x00000000u);             // 482694
        world.vehicle_audio.controller_gain = 100;                            // 4746C0
        world.vehicle_audio.motion_gain = std::bit_cast<float>(0x3f400000u);  // 4747B8
        world.load_selection.configuration_id = 1;                            // 472C74, signed source word
        world.contact_cast.mode = 1;                                          // 4754A0
        world.audio_driver.range = 7500;                                      // 4897A0
        world.audio_driver.range_squared = 56250000;                          // 4897A4
        world.audio_driver.inverse_range = std::bit_cast<float>(0x390bcf65u); // 4897A8
        // Literal DS3DBUFFER source size64, independent of the host ABI.
        world.audio_driver.spatial_template.size = 64;                                              // 4897B8
        world.audio_driver.spatial_template.position.x = std::bit_cast<float>(0x00000000u);         // 4897BC
        world.audio_driver.spatial_template.position.y = std::bit_cast<float>(0x00000000u);         // 4897C0
        world.audio_driver.spatial_template.position.z = std::bit_cast<float>(0x00000000u);         // 4897C4
        world.audio_driver.spatial_template.velocity.x = std::bit_cast<float>(0x00000000u);         // 4897C8
        world.audio_driver.spatial_template.velocity.y = std::bit_cast<float>(0x00000000u);         // 4897CC
        world.audio_driver.spatial_template.velocity.z = std::bit_cast<float>(0x00000000u);         // 4897D0
        world.audio_driver.spatial_template.inside_cone_angle = 360;                                // 4897D4
        world.audio_driver.spatial_template.outside_cone_angle = 360;                               // 4897D8
        world.audio_driver.spatial_template.cone_orientation.x = std::bit_cast<float>(0x00000000u); // 4897DC
        world.audio_driver.spatial_template.cone_orientation.y = std::bit_cast<float>(0x00000000u); // 4897E0
        world.audio_driver.spatial_template.cone_orientation.z = std::bit_cast<float>(0x3f800000u); // 4897E4
        world.audio_driver.spatial_template.outside_cone_volume = 0; // 4897E8, signed source word
        world.audio_driver.spatial_template.minimum_distance = std::bit_cast<float>(0x44a28000u); // 4897EC
        world.audio_driver.spatial_template.maximum_distance = std::bit_cast<float>(0x45ea6000u); // 4897F0
        world.audio_driver.spatial_template.mode = 0;                                             // 4897F4
        world.options.texture = 0; // 60D74C; later preview choice8 remains explicit
    }
    ~Storage()
    {
        if (std::this_thread::get_id() != owner_thread)
            std::terminate();
        // Legacy source cleanup is conditional on its actual backend.
        // NEW period fallback runs later through member destruction,
        // independently of the source allocated gate, while world lives.
        if (stream)
        {
            try
            {
                close_audio_stream(world.audio_stream, world.audio_maintenance.music, *device, *stream);
            }
            catch (...)
            { /* Native final owners still release their resources. */
            }
            stream->cancel_pending_callbacks();
        }
    }
};

// NEW host obligation owner. This API does not admit the retail producer or
// change45FCE0. The same strong platform argument spans every backend call.
class SourceMusicPeriodHostAccess final
{
  public:
    static void validate_prepare(const std::shared_ptr<RaceSessionPlatform> &owner)
    {
        if (!owner)
            throw std::invalid_argument("music period host requires a retained platform");
        owner->require_owner_thread();
        const auto &storage = *owner->storage_;
        if (!storage.source_entry || storage.stream)
            throw std::logic_error("music period host requires a source platform without legacy streaming");
        if (storage.music_periods)
            throw std::logic_error("music period backend is already bound and cannot be replaced");
    }
    static std::shared_ptr<const SourceMusicPeriodReport> bind(const std::shared_ptr<RaceSessionPlatform> &owner,
                                                               std::shared_ptr<SourceMusicPeriodCalls> calls)
    {
        validate_prepare(owner);
        if (!calls)
            throw std::invalid_argument("receiving music period backend is null");
        auto prepared = std::make_unique<SourceMusicPeriodOwner>(std::move(calls));
        const auto report = prepared->report();
        owner->storage_->music_periods = std::move(prepared);
        return report;
    }
    static SourceMusicPeriodOwner &get(const std::shared_ptr<RaceSessionPlatform> &owner)
    {
        if (!owner)
            throw std::invalid_argument("music period operation requires a retained platform");
        owner->require_owner_thread();
        if (!owner->storage_->music_periods)
            throw std::logic_error("music period owner has not been prepared");
        return *owner->storage_->music_periods;
    }
};
std::shared_ptr<const SourceMusicPeriodReport> prepare_installed_music_period_owner(
    std::shared_ptr<RaceSessionPlatform> owner)
{
    SourceMusicPeriodHostAccess::validate_prepare(owner);
    return SourceMusicPeriodHostAccess::bind(owner, make_installed_music_period_calls());
}
std::shared_ptr<const SourceMusicPeriodReport> prepare_receiving_music_period_owner(
    std::shared_ptr<RaceSessionPlatform> owner, std::shared_ptr<SourceMusicPeriodCalls> calls)
{
    return SourceMusicPeriodHostAccess::bind(owner, std::move(calls));
}
std::uint32_t begin_owned_music_period(std::shared_ptr<RaceSessionPlatform> owner, std::uint32_t period)
{
    return SourceMusicPeriodHostAccess::get(owner).begin(period);
}
std::uint32_t end_owned_music_period(std::shared_ptr<RaceSessionPlatform> owner, std::uint32_t period)
{
    return SourceMusicPeriodHostAccess::get(owner).end(period);
}

RaceSessionPlatform::RaceSessionPlatform(SourceColdConstructionOnly, std::filesystem::path root, float gain)
    : storage_(std::make_unique<Storage>(std::move(root), gain))
{
}
RaceSessionPlatform::RaceSessionPlatform(PreviewHostEntry, std::filesystem::path root, float gain)
    : RaceSessionPlatform(SourceColdConstructionOnly{}, std::move(root), gain)
{
    storage_->source_entry = false;
    prepare_host_assets();
    prepare_host_audio();
}
RaceSessionPlatform::RaceSessionPlatform(PreviewHostEntry, std::filesystem::path root, float gain,
                                         SourceApplicationLiteralData &literals)
    : RaceSessionPlatform(SourceColdConstructionOnly{}, std::move(root), gain)
{
    bind_source_application_literals(literals);
    storage_->source_entry = false;
    prepare_host_assets();
    prepare_host_audio();
}
RaceSessionPlatform::RaceSessionPlatform(std::filesystem::path root, float gain)
    : RaceSessionPlatform(PreviewHostEntry{}, std::move(root), gain)
{
}
RaceSessionPlatform::~RaceSessionPlatform() = default;
void RaceSessionPlatform::bind_source_application_literals(SourceApplicationLiteralData &owner)
{
    require_owner_thread();
    const auto reference = owner.reference();
    if (storage_->application_literals)
    {
        if (*storage_->application_literals != reference)
            throw std::logic_error("platform application source literal identity cannot be replaced");
        return;
    }
    storage_->application_literals = reference;
}
SourceApplicationLiteralRef RaceSessionPlatform::source_application_literals() const
{
    require_owner_thread();
    if (!storage_->application_literals)
        throw std::logic_error("platform application source literals are unbound");
    // Value retrieval does not read the literal or require its owner alive.
    return *storage_->application_literals;
}
SourceContactStackWords &RaceSessionPlatform::source_contact_stack_words()
{
    require_owner_thread();
    return storage_->contact_stack_words;
}
SourceCameraStackWords &RaceSessionPlatform::source_camera_stack_words()
{
    require_owner_thread();
    return storage_->camera_stack_words;
}
const SourceCameraStackWords &RaceSessionPlatform::source_camera_stack_words() const
{
    require_owner_thread();
    return storage_->camera_stack_words;
}
SourceAllocationBank &RaceSessionPlatform::source_allocations()
{
    require_owner_thread();
    return storage_->source_allocations;
}
const SourceAllocationBank &RaceSessionPlatform::source_allocations() const
{
    require_owner_thread();
    return storage_->source_allocations;
}
void RaceSessionPlatform::release_scene_allocations_42eaa0()
{
    require_owner_thread();
    storage_->source_allocations.release_42eaa0(); // actual42EAA0 tail42EA00
}
void RaceSessionPlatform::begin_scene_allocations_42ea80()
{
    require_owner_thread();
    storage_->source_allocations.begin_42ea80(); // 42EA80: actual42EA00 before three stores
}
void RaceSessionPlatform::prepare_host_assets()
{
    require_owner_thread();
    if (!storage_->assets)
        storage_->assets = std::make_unique<AssetStore>(storage_->root);
}
void RaceSessionPlatform::prepare_source_scene_resources()
{
    require_owner_thread();
    if (!storage_->source_entry)
        throw FormatError("source scene owner requires the source construction entry");
    if (storage_->source_scene)
        return;
    if (storage_->source_scene_claimed)
        throw FormatError("source scene owner construction already attempted");
    (void)assets(); // readiness precondition; never prepare assets implicitly
    storage_->source_scene_claimed = true;
    storage_->source_scene = std::make_unique<SceneResources>(*storage_->assets, storage_->root);
}
SceneResources &RaceSessionPlatform::source_scene_resources()
{
    require_owner_thread();
    if (!storage_->source_scene)
        throw FormatError("source scene owner has not been prepared");
    return *storage_->source_scene;
}
WindowsAudioDevice &RaceSessionPlatform::prepare_source_audio_owner()
{
    require_owner_thread();
    if (!storage_->device)
        storage_->device = std::make_unique<WindowsAudioDevice>(DeferredAudioConstructionOnly{}, storage_->master_gain);
    return *storage_->device;
}
WindowsAudioDevice &RaceSessionPlatform::source_audio_owner()
{
    require_owner_thread();
    if (!storage_->device)
        throw FormatError("source audio owner has not been constructed");
    return *storage_->device;
}
void RaceSessionPlatform::prepare_host_audio()
{
    require_owner_thread();
    if (storage_->music_periods)
        throw FormatError("legacy audio preparation cannot share a source music period owner");
    // Commit each completed owner once. A stream failure retains the existing
    // device identity; retry must not replace it or reset source audio words.
    auto &device = prepare_source_audio_owner();
    device.prepare_host_backend();
    if (!storage_->stream)
        storage_->stream = std::make_unique<WindowsAudioStreamPlatform>(*storage_->device, storage_->root);
}
PlatformHostReadiness RaceSessionPlatform::host_readiness() const
{
    require_owner_thread();
    return {bool(storage_->assets), storage_->device && storage_->device->host_backend_ready(), bool(storage_->stream),
            bool(storage_->input)};
}
bool RaceSessionPlatform::source_construction_only_entry() const noexcept
{
    return storage_->source_entry;
}
void RaceSessionPlatform::claim_session_attachment()
{
    require_owner_thread();
    if (session_attachment_claimed_)
        throw FormatError("this platform already claimed its single session attachment");
    session_attachment_claimed_ = true;
}
void RaceSessionPlatform::require_owner_thread() const
{
    if (std::this_thread::get_id() != storage_->owner_thread)
        throw std::logic_error("race world and platform belong to the creating game thread");
}
RaceSessionState &RaceSessionPlatform::state() noexcept
{
    return storage_->world;
}
const RaceSessionState &RaceSessionPlatform::state() const noexcept
{
    return storage_->world;
}
const std::filesystem::path &RaceSessionPlatform::retail_root() const noexcept
{
    return storage_->root;
}
const AssetStore &RaceSessionPlatform::assets() const
{
    require_owner_thread();
    if (!storage_->assets)
        throw FormatError("platform host assets have not been prepared");
    return *storage_->assets;
}
WindowsAudioDevice &RaceSessionPlatform::audio_device()
{
    require_owner_thread();
    if (!storage_->device || !storage_->device->host_backend_ready())
        throw FormatError("platform host audio device has not been prepared");
    return *storage_->device;
}
WindowsAudioStreamPlatform &RaceSessionPlatform::audio_stream_platform()
{
    require_owner_thread();
    if (!storage_->stream)
        throw FormatError("platform host audio stream has not been prepared");
    return *storage_->stream;
}
WindowInputOwner &RaceSessionPlatform::attach_window_input(std::uintptr_t instance, std::uintptr_t window)
{
    require_owner_thread();
    auto &raw = storage_->world.source_input.acquisition;
    const InputWindowHandles handles{instance, window};
    if (storage_->input)
    {
        storage_->input->require_binding(raw, handles);
        return *storage_->input;
    }
    // Publish only a completely constructed native owner. Failure leaves the
    // slot empty and raw state untouched; no source startup happened here.
    storage_->input = std::make_unique<WindowInputOwner>(raw, handles);
    return *storage_->input;
}
WindowInputOwner &RaceSessionPlatform::window_input(std::uintptr_t instance, std::uintptr_t window)
{
    require_owner_thread();
    if (!storage_->input)
        throw FormatError("window input has not been attached to this platform");
    storage_->input->require_binding(storage_->world.source_input.acquisition, {instance, window});
    return *storage_->input;
}
FrontendState::FrontendState(std::shared_ptr<RaceSessionState> world, std::shared_ptr<RaceSessionPlatform> platform)
    : world_(std::move(world)), platform_(std::move(platform))
{
    if (!world_ || !platform_ || world_.owner_before(platform_) || platform_.owner_before(world_) ||
        world_.get() != &platform_->state())
        throw FormatError("frontend requires the actual world alias of a single platform owner");
    platform_->require_owner_thread();
    resource_memory_ =
        std::make_unique<SourceFrontendResourceMemory>(world_->boot_config, source.language_input_identity);
}
} // namespace sgn::retail
