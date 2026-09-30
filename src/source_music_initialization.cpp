#include "sgn/source_music_initialization.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include <bit>

namespace sgn::retail
{
std::uint32_t source_music_read_4604e0(AudioStreamState &s, std::span<std::uint8_t> destination,
                                       std::uint32_t requested_bits, SourceMusicLockedReadIO &io)
{
    const auto remaining = s.chunk_size - s.field_df30 - 0x100u;
    const auto count = requested_bits <= remaining ? requested_bits : remaining;
    const auto result = io.read_locked(s.file, destination, count);
    s.field_df30 += result; // actual return bits, including -1; reload after callback
    return result;
}
std::uint32_t source_music_start_460240(std::shared_ptr<RaceSessionPlatform> owner, SourceMusicInitializationIO &io)
{
    if (!owner)
        throw std::invalid_argument("music start requires owner");
    owner->require_owner_thread();
    auto &state = owner->state();
    auto &s = state.audio_stream;
    if (s.allocated == 0 || (s.flags & 0x4000u) == 0)
    {
        s.result = 0x887800aau;
        return s.result;
    }
    (void)io.get_status(state.audio_maintenance.music.buffer.borrowed(), s.status); // 460220 ignores HRESULT
    if ((s.status & 1u) != 0)
    {
        s.result = 0;
        return 0;
    }
    SourceMusicClearOutputs outputs;
    (void)io.lock(state.audio_maintenance.music.buffer.borrowed(), 0, s.field_df0c, outputs, 0);
    // 46028C/46028F: load count then pointer even if Lock failed.
    if (!outputs.first_bytes)
        throw SourceMusicWaveUnknown("46028C unwritten Lock byte count");
    const auto bytes = *outputs.first_bytes;
    if (!outputs.first)
        throw SourceMusicWaveUnknown("46028F unwritten Lock pointer");
    (void)source_music_read_4604e0(s, *outputs.first, bytes, io);
    (void)io.unlock(state.audio_maintenance.music.buffer.borrowed(), *outputs.first, s.field_df0c);
    const auto volume = static_cast<std::int32_t>(std::bit_cast<std::int16_t>(io.volume_word_4897f8()));
    (void)io.set_volume(state.audio_maintenance.music.buffer.borrowed(), volume);
    const auto result = io.play(state.audio_maintenance.music.buffer.borrowed(), 0, 0, 1);
    const auto resolution = s.field_df20; // 4602D5 load before result store
    s.result = result;
    const auto timer = io.start_timer(s.field_df1c, resolution, owner, 0x00460510u, 1);
    const auto flags = s.flags; // 4602F9 load before timer store
    s.timer = timer;
    (void)io.exchange_flags(s.flags, flags & 0xffff7fffu);
    return s.result;
}
void source_music_clear_45fd40(AudioStreamState &stream, AudioBufferId buffer, SourceMusicLockIO &io)
{
    SourceMusicClearOutputs outputs; // both retail locals start unwritten
    const auto result = io.lock(buffer, 0, stream.field_df0c, outputs, 0);
    if (result != 0)
        return;                           // source caller does not inspect this result
    const auto count = stream.field_df0c; // 45FD6A rereads global AFTER Lock
    if (!outputs.first)
        throw SourceMusicWaveUnknown("45FD74 unwritten lock pointer");
    const auto region = *outputs.first;
    // EAX=0 from successful Lock; REP STOSD then remaining REP STOSB.
    // Keep completed DWORD writes when a later destination is not representable.
    std::size_t offset = 0;
    for (std::uint32_t i = 0; i < (count >> 2); ++i)
    {
        if (offset > region.size() || region.size() - offset < 4)
            throw SourceMusicWaveUnknown("45FD7C DWORD beyond first lock region");
        for (std::size_t j = 0; j < 4; ++j)
            region[offset + j] = 0;
        offset += 4;
    }
    for (std::uint32_t i = 0; i < (count & 3u); ++i)
    {
        if (offset >= region.size())
            throw SourceMusicWaveUnknown("45FD87 byte beyond first lock region");
        region[offset++] = 0;
    }
    // Source ignores returned first_bytes and uses the reloaded global count.
    (void)io.unlock(buffer, region, count);
}
std::uint32_t source_music_stop_460330(RaceSessionState &state, SourceMusicInitializationIO &io,
                                       SourceMusicWaveIO &wave)
{
    auto &s = state.audio_stream;
    // 460200: short circuit allocated first, then the actual flag word.
    if (s.allocated == 0 || (s.flags & 0x4000u) == 0)
    {
        s.result = 0x887800aau;
        return s.result;
    }
    // 460344..460353: plain load, OR, then InterlockedExchange; NOT fetch_or.
    const auto replacement = s.flags | 0x8000u;
    (void)io.exchange_flags(s.flags, replacement);
    const auto timer = s.timer;
    if (timer != 0)
    {
        (void)io.kill_timer(timer);
        s.timer = 0; // even when the imported call reports failure
    }
    (void)io.stop_buffer(state.audio_maintenance.music.buffer.borrowed());
    (void)io.set_position(state.audio_maintenance.music.buffer.borrowed(), 0); // reload after Stop
    const auto file = s.file; // 460388 captures handle before the state clears
    s.field_df28 = 0;
    s.field_df2c = 0;
    s.field_df30 = 0;
    s.field_df34 = 0;
    s.field_e084 = 0;
    if (file.value != 0)
        (void)wave.seek(file, s.data_offset + 4u, 0);
    s.playback_active = 0;
    s.result = 0;
    return 0;
}
void source_music_close_45fce0(RaceSessionState &state, SourceMusicInitializationIO &io, SourceMusicWaveIO &wave)
{
    auto &s = state.audio_stream;
    if (s.allocated == 0)
        return;
    (void)source_music_stop_460330(state, io, wave);
    (void)io.release(state.audio_maintenance.music.buffer.borrowed()); // reload after stop
    (void)io.end_period(s.timer_period);
    if (s.file.value != 0)
    {
        (void)wave.close(s.file, 0);
        s.file = {};
    }
    const auto flags = s.flags;
    s.allocated = 0;
    s.flags = flags & 0xffffbfffu;
    // Retail leaves the released buffer identity and other fields untouched.
}
SourceMusicInitializationResult source_music_initialize_45fda0(std::shared_ptr<RaceSessionPlatform> owner,
                                                               SourceMusicTimerCapsReceiving &caps,
                                                               SourceMusicDriveTypeReceiving &drive,
                                                               SourceMusicDiskFreeSpaceReceiving &disk,
                                                               SourceMusicWaveIO &wave, SourceMusicInitializationIO &io)
{
    const auto prefix = receive_source_music_buffer_entry_45fda0(owner, caps, drive, disk, wave);
    if (prefix.wave.function_return)
        return {prefix, prefix.wave.function_return, std::nullopt};
    if (!prefix.sufficient_capacity)
        return {prefix, std::nullopt, prefix.next_site};
    owner->require_owner_thread();
    auto &state = owner->state();
    auto &s = state.audio_stream;
    if (!*prefix.sufficient_capacity)
    {
        source_music_close_45fce0(state, io, wave); // 45FEE4 before result store
        s.result = 0x80004005u;
        return {prefix, s.result, std::nullopt};
    }
    // 45FF30..45FF57: real WAVEFORMATEX owner, output writes actual buffer.
    SourceMusicBufferDescription desc{0x14, 0xe8, s.field_df0c, 0, &s.format};
    const auto created = io.create(state, desc, state.audio_maintenance.music.buffer, 0);
    s.result = created;
    if (created != 0)
    {
        source_music_close_45fce0(state, io, wave);
        return {prefix, created, std::nullopt}; // EDI survives cleanup's result writes
    }
    source_music_clear_45fd40(s, state.audio_maintenance.music.buffer.borrowed(), io);
    const auto flags = s.flags;
    s.field_df2c = 0;
    s.field_df34 = 0;
    s.allocated = 1;
    s.flags = flags | 0x4000u;
    if ((flags & 4u) != 0)
    {
        const auto started = source_music_start_460240(owner, io);
        s.result = started;
        if (started != 0)
        {
            source_music_close_45fce0(state, io, wave);
            return {prefix, started, std::nullopt};
        }
    }
    return {prefix, s.result, std::nullopt}; // 45FFBD reload; no fabricated success
}
} // namespace sgn::retail
