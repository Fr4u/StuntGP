#include "sgn/source_track_replay.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <array>
#include <cstring>

namespace sgn::retail
{
namespace
{
std::uint32_t read_source_replay_4199f0(RaceSessionPlatform &platform, SourceBootFilePlatform &files,
                                        std::span<const std::uint8_t> written_name)
{
    SourceScriptPathCall path; // actual4199F0 local128
    auto &s = platform.state();
    format_replay_file_path_419a0b(s.boot_config, platform.source_application_literals(), written_name, path);
    const auto result = source_boot_file_size_44b3b0(s.boot_config, files, path.completed_path()); // 419A16
    if (result == 0)
        return 0; // 419A1B..23: no destination access or reset
    // 419A24 always supplies20000, independent of the probe's returned size.
    // The destination is the full live6-slot arena at4D5CC0, not one RPL slot.
    // Writes may reach the next slot. No complete-header preflight or rollback.
    std::size_t copied = 0;
    return source_boot_file_read_44b260(s.boot_config, files, path.completed_path(), 0x20000u, s.replays.bytes(),
                                        copied);
}
} // namespace
void load_source_track_replay_442ef0(RaceSessionPlatform &platform, SourceBootFilePlatform &files)
{
    platform.require_owner_thread();
    auto &s = platform.state();
    std::array<std::uint8_t, 16> local;       // actual442EF0 local16, no pre-zero
    s.trial_selection.track.replay.reset();   // 442F07
    s.trial_selection.track.vehicle.reset();  // 442F0D
    s.trial_selection.track.header_value = 0; // 442F13
    SourceScriptPathCall format;
    format_track_replay_name_442f19(s.boot_config, platform.source_application_literals(), local, format);
    const auto returned = read_source_replay_4199f0(
        platform, files, {local.data(), format.initialized_prefix}); // 442F29 borrows actual local bytes
    if (returned == 0)
        return;
    std::uint32_t header;
    const auto bank = s.replays.bytes();
    std::memcpy(&header, bank.data() + 0x3c, 4);      // 442F32: read even after a short successful read
    s.trial_selection.track.replay = ReplayHandle{0}; // 442F38
    s.trial_selection.track.header_value = header;    // 442F42
    s.trial_selection.track.vehicle.reset();          // 442F48
    // Raw file bytes, including header+10, remain source32 address bits.
    // ReplayBank::cursor decodes only for native access. Source41AB14 resets
    // a stale loaded cursor before playback; this I/O does not activate it.
}
} // namespace sgn::retail
