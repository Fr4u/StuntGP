#include "sgn/source_music_buffer_size.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include <stdexcept>

namespace sgn::retail
{
bool source_music_buffer_size_45fe6d(AudioStreamState &stream, std::uint8_t saved_bl)
{
#if defined(_M_X64)
    SourceMusicBufferSizeFields fields{&stream.field_df1c,     &stream.format.bytes_per_second,
                                       &stream.field_df14,     &stream.field_df20,
                                       &stream.field_df0c,     &stream.field_df24,
                                       &stream.stop_threshold, saved_bl};
    return source_music_buffer_size_x64(&fields) != 0;
#else
    (void)stream;
    (void)saved_bl;
    throw std::runtime_error("45FE6D requires the recovered x87 arithmetic implementation");
#endif
}
SourceMusicBufferEntryResult receive_source_music_buffer_entry_45fda0(std::shared_ptr<RaceSessionPlatform> owner,
                                                                      SourceMusicTimerCapsReceiving &caps,
                                                                      SourceMusicDriveTypeReceiving &drive,
                                                                      SourceMusicDiskFreeSpaceReceiving &disk,
                                                                      SourceMusicWaveIO &io)
{
    const auto wave = receive_source_music_wave_entry_45fda0(owner, caps, drive, disk, io);
    if (wave.function_return || !wave.helper_result)
        return {wave, std::nullopt, wave.next_site};
    // Successful helper is the only path reaching this point. The parser can
    // write the stream, so use the saved BL, not the current byte of field_df18.
    const auto saved_bl = wave.disk_prefix.written_shift;
    if (!saved_bl)
        throw std::logic_error("music sizing reached without saved BL");
    owner->require_owner_thread();
    const bool sufficient = source_music_buffer_size_45fe6d(owner->state().audio_stream, *saved_bl);
    return {wave, sufficient, sufficient ? 0x0045ff30u : 0x0045fee4u};
}
} // namespace sgn::retail
