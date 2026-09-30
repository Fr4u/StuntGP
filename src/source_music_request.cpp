#include "sgn/source_music_windows_runtime.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_boot_config.hpp"
#include "sgn/retail_pause_music.hpp"
#include <algorithm>
#include <bit>
#include <string>

namespace sgn::retail
{
void SourceMusicWindowsRuntime::start_music(std::uint32_t requested, std::uint32_t loop, SourceBootFilePlatform &files,
                                            SourceMusicTimerCapsReceiving &caps, SourceMusicDriveTypeReceiving &drive,
                                            SourceMusicDiskFreeSpaceReceiving &disk)
{
    const auto self = shared_from_this();
    (void)self;
    const auto p = owner();
    auto &w = p->state();
    if (w.audio_driver.initialized == 0)
        return; // 45F906
    if (w.audio_level.output_a == 0)
        return; // 45F912
    const auto selected = requested != 0 ? requested : w.music.selected;
    auto &s = w.audio_stream;
    s.field_e10c = selected; // store also for selected0
    if (selected == 0)
        return;
    // Literal pointer table4898FC and its24 strings, verified against PE by
    // compose_source.py. No modulo, clamp or synthetic out-of-range track.
    static constexpr std::array<const char *, 24> names{
        "Empty",   "Empty",   "Track2",  "Track3",  "Track4",  "Track5",  "Track6",  "Track7",
        "Track8",  "Track9",  "Track10", "Track11", "Track12", "Track13", "Track14", "Track15",
        "Track16", "Track17", "Track18", "Track19", "Track20", "Track21", "Track22", "Track23"};
    if (selected >= names.size())
        throw SourceMusicWaveUnknown("45F934 pointer outside qualified music table");
    const auto &root = w.boot_config.sound_path; // actual484A1C, not a hardcoded relative path
    const auto end = std::find(root.begin(), root.end(), '\0');
    if (end == root.end())
        throw SourceMusicWaveUnknown("45F94D sound root lacks source terminator");
    std::string path(root.begin(), end);
    path += "\\Music\\";
    path += names[selected];
    path += ".wav"; // 48A500 %s\Music\%s.wav
    if (path.size() >= 256)
        throw SourceMusicWaveUnknown("45F94D sprintf exceeds qualified256-byte source local");
    if (source_boot_file_size_44b3b0(w.boot_config, files, path) == 0)
    {
        s.playback_active = 0;
        return;
    }
    const auto opened = open_source(std::span<const char>(path.c_str(), path.size() + 1),
                                    {loop == 1 ? 0x25u : 0x24u, 0, 0, 0, 0}, caps, drive, disk);
    if (!opened.function_return)
        throw SourceMusicWaveUnknown("45F990 child has not returned;45F999 cannot execute");
    // 45F995 discards EVERY returned HRESULT, including failures.
    auto &m = w.audio_maintenance.music;
    apply_music_levels(0, 0); // actual full45F7D0; native failures return to this tail
    m.pan = 0;                // 45F9A3
    m.requested_volume = 0;   // 45F9AB
    m.gain = 0;               // 45F9B1
    set_music_words_45f8e0(s.field_e100, m.gain_step, 255, 255);
    s.playback_active = 1; // 45F9BC, independent of open HRESULT
}
} // namespace sgn::retail
