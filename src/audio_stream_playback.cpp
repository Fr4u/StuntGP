#include "sgn/audio_stream_playback.hpp"
#include "sgn/vehicle_audio_tables.hpp"
#include "sgn/vehicle_audio_math.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace sgn::retail
{
namespace
{
constexpr std::uint32_t invalid_argument = 0x80070057, bad_format = 0x88780064, failed = 0x80004005;
AudioResult signed_result(std::uint32_t n) noexcept
{
    return std::bit_cast<AudioResult>(n);
}
std::uint32_t word_result(AudioResult n) noexcept
{
    return std::bit_cast<std::uint32_t>(n);
}
std::uint16_t short_at(std::span<const std::uint8_t> b, std::size_t p)
{
    return static_cast<std::uint16_t>(std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8));
}
std::uint32_t word_at(std::span<const std::uint8_t> b, std::size_t p)
{
    return std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8) | (std::uint32_t(b[p + 2]) << 16) |
           (std::uint32_t(b[p + 3]) << 24);
}
void clear_stream_buffer(AudioStreamPlaybackWorld w)
{
    AudioBufferMemory locked;
    if (w.creation.lock_buffer(w.music.buffer.borrowed(), 0, w.stream.field_df0c, locked, 0) != 0)
        return;
    if (locked.first.size() < w.stream.field_df0c)
        throw FormatError("source music clear requires one full lock region");
    std::fill_n(locked.first.begin(), w.stream.field_df0c, std::uint8_t{});
    (void)w.creation.unlock_buffer(w.music.buffer.borrowed(), locked.first, {});
}
AudioResult parser_error(AudioStreamPlaybackWorld w, bool clear_file)
{
    (void)w.platform.close_wave(w.stream.file, 0);
    if (clear_file)
        w.stream.file = {};
    w.stream.result = bad_format;
    return signed_result(bad_format);
}
} // namespace

AudioResult parse_audio_stream_wave(AudioStreamPlaybackWorld w)
{
    auto &s = w.stream;
    const auto filename = s.filename();
    s.file = w.platform.open_wave(filename, 1024, 0x10000);
    if (!s.file.value)
    {
        s.result = invalid_argument;
        return signed_result(s.result);
    }
    auto riff = s.chunk();
    const auto descended = w.platform.descend_wave(s.file, riff, nullptr, 0);
    s.chunk(riff);
    if (descended != 0 || riff.id != 0x46464952 || riff.type != 0x45564157)
        return parser_error(w, true);
    AudioWaveChunk format{};
    format.id = 0x20746d66;
    if (w.platform.descend_wave(s.file, format, &riff, 0x10) != 0)
        return parser_error(w, true);
    // Source allocates size&FFFF and reads full size, then copies18 bytes.
    // Invalid source heap accesses are rejected by this modern file boundary.
    if (format.size < 16 || format.size > 0xffff)
        throw FormatError("music fmt outside safe recovered allocation domain");
    std::vector<std::uint8_t> bytes(format.size);
    if (w.platform.read_wave(s.file, bytes) != static_cast<std::int32_t>(format.size))
        return parser_error(w, false);
    s.format = {short_at(bytes, 0),
                short_at(bytes, 2),
                word_at(bytes, 4),
                word_at(bytes, 8),
                short_at(bytes, 12),
                short_at(bytes, 14),
                format.size >= 18 ? short_at(bytes, 16) : std::uint16_t{0}};
    if (s.format.tag != 1)
        return parser_error(w, true);
    if (w.platform.ascend_wave(s.file, format, 0) != 0)
        return parser_error(w, true);
    if (w.platform.seek_wave(s.file, signed_result(s.data_offset + 4u), 0) == -1)
        return parser_error(w, true);
    AudioWaveChunk data{};
    data.id = 0x61746164;
    if (w.platform.descend_wave(s.file, data, &riff, 0x10) != 0)
        return parser_error(w, true);
    s.chunk(data);
    s.field_df30 = 0;
    s.result = 0;
    return 0;
}

AudioResult initialize_audio_stream_playback(AudioStreamPlaybackWorld w)
{
    auto &s = w.stream;
    if (w.platform.timer_capabilities(s.timer_capabilities.minimum(), s.timer_capabilities.maximum()) != 0)
        throw FormatError("source music timer-capabilities failure enters INT3");
    s.timer_period = std::min(std::max(s.timer_capabilities.minimum(), 1u), s.timer_capabilities.maximum());
    (void)w.platform.begin_timer_period(s.timer_period);
    const auto filename = s.filename();
    const std::string root(reinterpret_cast<const char *>(filename.data()), 3);
    if (w.platform.drive_type(root) == 2)
        return signed_result(failed);
    std::uint32_t sector{};
    if (!w.platform.disk_sector_bytes(root, sector) || !sector)
        throw FormatError("source music sector BSF operand is not defined");
    const auto shift = static_cast<unsigned>(std::countr_zero(sector));
    s.field_df18 = (s.field_df18 & 0xffffff00u) | shift;
    if (parse_audio_stream_wave(w) != 0)
        return 0; // 45FE60..45FE6A, source ignores parser error here
    s.field_df20 = s.field_df1c >> 2;
    const double rate = static_cast<double>(s.format.bytes_per_second);
    const double period = static_cast<double>(s.field_df1c);
    const double scaled = (rate * period) * static_cast<double>(std::bit_cast<float>(std::uint32_t{0x3a83126f}));
    // 462860: finite-int64 value domain, low32 result; no x87 state parity claim.
    s.field_df0c = audio_detail::integer(scaled) * s.field_df14;
    s.field_df24 = (s.field_df0c >> shift) + 1u;
    const auto minimum = audio_detail::integer(rate / (1000.0 / period));
    if ((std::uint32_t{1} << shift) * s.field_df24 < minimum)
    {
        close_audio_stream(s, w.music, w.audio.device, w.platform);
        s.result = failed;
        return signed_result(failed);
    }
    s.stop_threshold = (s.field_df20 + s.field_df1c) * (s.format.bytes_per_second / 1000u);
    AudioBufferRequest request{20, 0xe8, s.field_df0c, 0, s.format};
    const auto result = w.music_producer.create_music_buffer(w.device, request, w.music.buffer);
    s.result = word_result(result);
    if (result != 0)
    {
        close_audio_stream(s, w.music, w.audio.device, w.platform);
        return result;
    }
    clear_stream_buffer(w);
    const auto prior_flags = s.flags;
    s.field_df2c = s.field_df34 = 0;
    s.allocated = 1;
    s.flags = prior_flags | 0x4000u;
    if (prior_flags & 4u)
    {
        const auto started = start_audio_stream(w);
        s.result = word_result(started);
        if (started != 0)
        {
            close_audio_stream(s, w.music, w.audio.device, w.platform);
            return started;
        }
    }
    return signed_result(s.result);
}

AudioResult open_audio_stream(AudioStreamPlaybackWorld w, std::string_view relative_name, std::uint32_t flags,
                              std::uint32_t period, std::uint32_t buffers, std::uint32_t completion,
                              std::uint32_t userdata)
{
    auto &s = w.stream;
    if (s.allocated)
        close_audio_stream(s, w.music, w.audio.device, w.platform);
    std::array<std::uint8_t, 260> absolute{};
    if (!w.platform.full_path(relative_name, absolute))
        throw FormatError("source music fullpath failure leaves uninitialized stack bytes");
    // strncpy pads through260 after the first NUL; a full260-byte input remains
    // unterminated exactly as in source. Host open rejects an unterminated path.
    const auto end = std::find(absolute.begin(), absolute.end(), std::uint8_t{});
    std::fill(end, absolute.end(), std::uint8_t{});
    s.filename(absolute);
    switch (flags & 0x78u)
    {
    case 8:
        s.field_df1c = 2000;
        s.field_df14 = 24;
        break;
    case 0x10:
        s.field_df1c = 600;
        s.field_df14 = 5;
        break;
    case 0x20:
        s.field_df1c = 400;
        s.field_df14 = 4;
        break;
    case 0x40:
        s.field_df1c = 100;
        s.field_df14 = 3;
        break;
    default:
        if (!period || !buffers)
            return signed_result(invalid_argument);
        s.field_df1c = period;
        s.field_df14 = buffers;
        break;
    }
    s.flags = flags;
    s.field_df3c = completion;
    s.field_df40 = userdata;
    if (flags & 2u)
    {
        w.platform.start_open_worker([w] { (void)initialize_audio_stream_playback(w); });
        s.result = 0;
        return 0;
    }
    return initialize_audio_stream_playback(w);
}

std::int32_t read_audio_stream(AudioStreamPlaybackWorld w, std::span<std::uint8_t> output)
{
    auto &s = w.stream;
    const auto remaining = s.chunk_size - s.field_df30 - 256u;
    const auto count = std::min<std::size_t>(output.size(), remaining);
    const auto actual = w.platform.read_wave(s.file, output.first(count));
    s.field_df30 += word_result(actual);
    return actual;
}

AudioResult start_audio_stream(AudioStreamPlaybackWorld w)
{
    auto &s = w.stream;
    if (!audio_stream_ready(s))
    {
        s.result = 0x887800aa;
        return signed_result(s.result);
    }
    (void)w.audio.device.status(w.music.buffer.borrowed(), s.status);
    if (s.status & 1u)
    {
        s.result = 0;
        return 0;
    }
    AudioBufferMemory locked;
    (void)w.creation.lock_buffer(w.music.buffer.borrowed(), 0, s.field_df0c, locked, 0);
    if (locked.first.size() != s.field_df0c || !locked.second.empty())
        throw FormatError("source music start lock output is undefined or split");
    (void)read_audio_stream(w, locked.first);
    (void)w.creation.unlock_buffer(w.music.buffer.borrowed(), locked.first, {});
    (void)w.audio.device.volume(w.music.buffer.borrowed(), audio_data::attenuation[0]);
    s.result = word_result(w.audio.device.play(w.music.buffer.borrowed(), 0, 0, 1));
    s.timer = w.platform.start_timer(s.field_df1c, s.field_df20, [w] { advance_audio_stream(w); }, 1);
    std::atomic_ref<std::uint32_t>(s.flags).exchange(s.flags & ~0x8000u, std::memory_order_seq_cst);
    return signed_result(s.result);
}

void advance_audio_stream(AudioStreamPlaybackWorld w)
{
    auto &s = w.stream;
    if ((s.flags & 0x8000u) || !(s.flags & 0x4000u))
        return;
    std::uint32_t play{}, write{};
    if (w.cursors.play_cursors(w.music.buffer.borrowed(), play, write) != 0)
        throw FormatError("source music cursor failure has undefined stack outputs");
    auto pending = play - s.field_df28;
    if (signed_result(pending) < 0)
        pending += s.field_df0c;
    auto consumed = play - s.field_df34;
    if (signed_result(consumed) < 0)
        consumed += s.field_df0c;
    s.field_df34 = play;
    s.field_df2c += consumed;
    s.field_e084 = s.field_df2c;
    const auto loop = s.flags & 1u;
    if (!loop && s.chunk_size - s.stop_threshold <= s.field_df2c)
    {
        (void)stop_audio_stream(s, w.music, w.audio.device, w.platform);
        if (s.field_df3c)
            w.platform.invoke_completion(s.field_df3c, s.field_df40);
        return;
    }
    if (s.chunk_size - s.field_df30 == 256u)
        return;
    const auto started = w.platform.milliseconds();
    ++s.field_e0a0;
    const auto shift = s.field_df18 & 31u;
    const auto sectors = std::min(signed_result(pending) >> shift, signed_result(s.field_df24));
    const auto count = word_result(sectors) << shift;
    if (sectors > 0)
    {
        const auto offset = s.field_df28;
        AudioBufferMemory locked;
        (void)w.creation.lock_buffer(w.music.buffer.borrowed(), offset, count, locked, 0);
        // The source only reads non-null returned regions. A host failure that
        // does not publish regions must not manufacture successful playback.
        if (locked.first.size() + locked.second.size() != count)
            throw FormatError("source music refill lock output is undefined");
        std::int32_t first{}, second{};
        if (locked.first.data())
            first = read_audio_stream(w, locked.first);
        if (locked.second.data())
            second = read_audio_stream(w, locked.second);
        (void)w.creation.unlock_buffer(w.music.buffer.borrowed(), locked.first, locked.second);
        const auto total = word_result(first) + word_result(second);
        s.field_df28 = offset + total;
        if (s.field_df0c <= s.field_df28)
            s.field_df28 -= s.field_df0c;
        if (signed_result(total) < signed_result(count) && loop)
        {
            s.field_df2c = s.field_df30 = 0;
            (void)w.platform.seek_wave(s.file, signed_result(s.data_offset + 4u), 0);
            if (s.field_df3c)
                w.platform.invoke_completion(s.field_df3c, s.field_df40);
        }
    }
    s.field_e098 += w.platform.milliseconds() - started;
    if (!s.field_e0a0)
        throw FormatError("source music elapsed counter division by zero");
    s.field_e09c = s.field_e098 / s.field_e0a0;
}

void start_audio_music(AudioStreamPlaybackWorld w, const AudioMusicTrack &selection, std::uint32_t requested,
                       std::uint32_t loop)
{
    if (!w.audio.state.initialized || !w.audio.level.output_a)
        return;
    auto &s = w.stream;
    s.field_e10c = requested ? requested : selection.selected;
    if (!s.field_e10c)
        return;
    // Literal pointer table4898FC, pinned EXE SHA4a1d234c...; entries0/1 Empty.
    if (s.field_e10c >= 24)
        throw FormatError("music index outside recovered pointer table");
    const auto name = s.field_e10c == 1 ? std::string("Empty") : std::string("Track") + std::to_string(s.field_e10c);
    const auto relative = std::string("sound/Music/") + name + ".wav";
    if (!w.platform.resource_length(relative))
    {
        s.playback_active = 0;
        return;
    }
    (void)open_audio_stream(w, relative, loop == 1 ? 0x25u : 0x24u, 0, 0, 0, 0);
    audio_apply_music_levels(w.audio, w.music, 0, 0);
    w.music.pan = 0;
    w.music.requested_volume = 0;
    w.music.gain = 0;
    s.field_e100 = 255;
    w.music.gain_step = 255;
    s.playback_active = 1;
}

void advance_audio_music_delay(AudioStreamPlaybackWorld w, const AudioMusicTrack &selection, std::uint32_t &delay_ticks)
{
    if (delay_ticks && --delay_ticks == 0)
        start_audio_music(w, selection, 0, 1);
}
void stop_audio_music(AudioMusicStopWorld w)
{
    if (w.stream.playback_active && w.level.output_a)
    {
        (void)stop_audio_stream(w.stream, w.music, w.device, w.platform);
        w.stream.playback_active = 0;
    }
}
void stop_audio_music(AudioStreamPlaybackWorld w)
{
    stop_audio_music(AudioMusicStopWorld{w.stream, w.music, w.audio.level, w.audio.device, w.platform});
}
} // namespace sgn::retail
