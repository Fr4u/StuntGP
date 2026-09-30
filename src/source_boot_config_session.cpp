#include "sgn/source_boot_config.hpp"
#include "sgn/source_wad_transfer.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
class SessionBootAudio final : public SourceBootAudio
{
    RaceSessionState &world_;

  public:
    explicit SessionBootAudio(RaceSessionState &world) : world_(world) {}
    void configure_460c20(std::uint32_t master, std::uint32_t effects, std::uint32_t music, std::uint32_t rate) override
    {
        if (rate > 44100u)
            rate = 44100u;
        else if (rate < 5512u)
            rate = 5512u;
        world_.audio_initialization.requested_rate = rate; // 460C43, FIRST source store
        auto &level = world_.audio_level;
        if (master <= 100u)
            level.scale = master; // 460C4A
        if (effects <= 100u)
            level.output_b = effects; // 460C55
        if (music <= 100u)
            level.output_a = music;                // 460C63
        level.input_b = level.output_b;            // 460C73
        level.input_a = level.output_a;            // 460C78
        update_level_audio(level, world_.options); // existing full460BC0, same source owners
    }
};
} // namespace
void initialize_platform_source_boot_config_42d660(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    platform.require_owner_thread();
    auto &state = platform.state();
    SessionBootAudio audio(state);
    initialize_source_boot_config_42d660(
        {state.boot_config, state.control_kind, state.source_scheduler.fixed_mode_a, audio},
        platform.source_allocations(), files);
}
void initialize_session_source_boot_config_42d660(RaceSession &session, SourceBootFilePlatform &files)
{
    initialize_platform_source_boot_config_42d660(*session.shared_platform(), files);
}
std::uint32_t transfer_platform_source_wad_44b354(RaceSessionPlatform &platform, const SourceAllocationRef &destination,
                                                  std::uint32_t member_offset, std::uint32_t requested,
                                                  std::uint32_t member_length, std::size_t &copied,
                                                  SourceWadTransferObserver *observer)
{
    platform.require_owner_thread(); // NEW synchronous host admission.
    return detail::transfer_source_wad_to_bank_44b354(platform.state().boot_config.files, platform.source_allocations(),
                                                      destination, member_offset, requested, member_length, copied,
                                                      observer);
}
} // namespace sgn::retail
