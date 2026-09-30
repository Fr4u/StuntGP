#include "sgn/source_music_periodic.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
std::uint32_t sar(std::uint32_t bits, std::uint32_t count)
{
    count &= 31u;
    if (count == 0)
        return bits;
    return (bits >> count) | ((bits & 0x80000000u) ? (~std::uint32_t{0} << (32u - count)) : 0u);
}
template <class T> T known(const std::optional<T> &value, const char *site)
{
    if (!value)
        throw SourceMusicWaveUnknown(site);
    return *value;
}
} // namespace
void source_music_periodic_460510(std::shared_ptr<RaceSessionPlatform> owner, SourceMusicPeriodicIO &io,
                                  SourceMusicWaveIO &wave)
{
    if (!owner)
        throw std::invalid_argument("460510 requires retained owner");
    owner->require_owner_thread();
    auto &state = owner->state();
    auto &s = state.audio_stream;
    const auto entry_flags = s.flags;
    if ((entry_flags & 0x8000u) != 0 || (entry_flags & 0x4000u) == 0)
        return;
    const auto buffer = state.audio_maintenance.music.buffer.borrowed(); // EBX retained for entire callback
    SourceMusicCursorOutputs cursor;
    (void)io.cursors(buffer, cursor);
    const auto play = known(cursor.play, "460541 unwritten play cursor");
    auto writable = play - s.field_df28;
    if ((writable & 0x80000000u) != 0)
        writable += s.field_df0c; // JNS, not unsigned less-than
    auto advanced = play - s.field_df34;
    if ((advanced & 0x80000000u) != 0)
        advanced += s.field_df0c;
    auto consumed = s.field_df2c;
    s.field_df34 = play;
    const auto loop = s.flags & 1u;
    consumed += advanced;
    s.field_df2c = consumed;
    s.field_e084 = consumed;
    if (loop == 0 && consumed >= s.chunk_size - s.stop_threshold)
    {
        (void)source_music_stop_460330(state, io, wave);
        const auto callback = s.field_df3c;
        if (callback != 0)
            io.completion_thread(owner, callback, s.field_df40, cursor.write_or_thread_id);
        return;
    }
    if (s.chunk_size - s.field_df30 - 0x100u == 0)
        return;
    const auto started = io.milliseconds();
    const auto calls = s.field_e0a0 + 1u;
    const auto maximum = s.field_df24;
    s.field_e0a0 = calls;
    const auto shift = s.field_df18 & 0xffu;
    auto sectors = sar(writable, shift);
    if (as_signed(sectors) > as_signed(maximum))
        sectors = maximum;
    const auto requested = sectors << (shift & 31u);
    if (as_signed(sectors) > 0)
    {
        std::uint32_t first_read = 0, second_read = 0; // only these two locals are zeroed
        const auto offset = s.field_df28;
        SourceMusicSplitLockOutputs locked;
        (void)io.lock_split(buffer, offset, requested, locked, 0);
        auto first = known(locked.first, "46062A unwritten first lock pointer");
        if (first.data() != nullptr)
        {
            const auto bytes = known(locked.first_bytes, "460631 unwritten first lock size");
            first_read = source_music_read_4604e0(s, first, bytes, io);
        }
        auto second = known(locked.second, "46063C second slot still source userdata or unknown");
        if (second.data() != nullptr)
        {
            const auto bytes = known(locked.second_bytes, "460643 unwritten second lock size");
            second_read = source_music_read_4604e0(s, second, bytes, io);
            second = known(locked.second, "46064B second pointer");
        }
        // The sizes are loaded even when the corresponding pointer is NULL.
        const auto second_bytes = known(locked.second_bytes, "460651 unwritten second size");
        first = known(locked.first, "460657 first pointer");
        const auto first_bytes = known(locked.first_bytes, "46065B unwritten first size");
        (void)io.unlock_split(buffer, first, first_bytes, second, second_bytes);
        auto next = offset + second_read + first_read;
        const auto capacity = s.field_df0c;
        s.field_df28 = next;
        if (next >= capacity)
        {
            next -= capacity;
            s.field_df28 = next;
        } // one subtraction, not modulo
        const auto total = second_read + first_read;
        if (as_signed(total) < as_signed(requested) && loop != 0)
        {
            const auto seek_offset = s.data_offset + 4u;
            const auto file = s.file;
            s.field_df2c = 0;
            s.field_df30 = 0;
            (void)wave.seek(file, seek_offset, 0);
            const auto callback = s.field_df3c;
            if (callback != 0)
                io.completion_thread(owner, callback, s.field_df40, cursor.write_or_thread_id);
        }
    }
    const auto ended = io.milliseconds();
    const auto accumulated = s.field_e098 + (ended - started);
    s.field_e098 = accumulated;
    const auto count = s.field_e0a0;
    if (count == 0)
        throw SourceMusicWaveUnknown("4606E5 DIV by zero after source counter wrap");
    s.field_e09c = accumulated / count;
}
} // namespace sgn::retail
