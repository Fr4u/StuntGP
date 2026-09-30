#include "sgn/source_music_windows_runtime.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
std::uint32_t sar(std::uint32_t v, unsigned count)
{
    return as_unsigned(as_signed(v) >> count); // C++20 arithmetic shift
}
} // namespace
void SourceMusicWindowsRuntime::advance_music_gain()
{
    const auto self = shared_from_this();
    (void)self;
    const auto p = owner();
    auto &state = p->state();
    if (state.audio_level.output_a == 0)
        return;
    auto &m = state.audio_maintenance.music;
    auto gain = m.gain + m.gain_step;
    m.gain = gain; // 45F89A before bounds branches
    if (as_signed(gain) < 0 || as_signed(gain) > 255)
    {
        gain = as_signed(gain) < 0 ? 0u : 255u;
        m.gain_step = 0; // 45F8B3 store precedes clamped gain
        m.gain = gain;
        m.cached_volume = -1;
        m.cached_pan = -1;
    }
    apply_music_levels(m.pan, gain); // 45F8D1 actual EDX, not stale requested_volume
}
void SourceMusicWindowsRuntime::apply_music_levels(std::int32_t pan, std::uint32_t volume)
{
    const auto self = shared_from_this();
    (void)self;
    const auto p = owner();
    auto &state = p->state();
    auto &m = state.audio_maintenance.music;
    if (state.audio_level.output_a == 0)
        return;
    auto buffer = m.buffer.borrowed(); // 45F7DE ESI retained before source stores
    m.pan = pan;
    m.requested_volume = volume;
    if (buffer.value == 0)
        return;
    const auto sign = pan < 0 ? 0xffffffffu : 0u;
    auto magnitude = (as_unsigned(pan) ^ sign) - sign; // CDQ/XOR/SUB: INT_MIN remains80000000
    magnitude = sar(magnitude + 1u, 1) << 1;           // INC/SAR/SHL, all32-bit wrap
    const auto address = 0x4898f8u - magnitude;
    auto mapped = as_unsigned(
        static_cast<std::int32_t>(std::bit_cast<std::int16_t>(literals_.read_audio_level_word(address, 0x45f80d))));
    if (pan < 0)
        mapped = 0u - mapped;
    if (as_signed(mapped) != m.cached_pan)
    {
        m.cached_pan = as_signed(mapped); // store before SetPan
        if (buffers_.pan(buffer, as_signed(mapped)) != 0)
            return;
        buffer = m.buffer.borrowed(); // 45F82C: only this successful call reloads ESI
    }
    auto product = m.gain * m.requested_volume;
    product *= state.audio_level.output_a;
    // 45F84B signed64 IMUL; consume high DWORD, add wrapped ECX, SAR15,
    // then add its sign bit. Preserve overflow, not a mathematical division.
    const auto wide = static_cast<std::int64_t>(as_signed(0x81018203u)) * as_signed(product);
    auto quotient = static_cast<std::uint32_t>(static_cast<std::uint64_t>(wide) >> 32);
    quotient = sar(quotient + product, 15);
    quotient += quotient >> 31;
    if (as_signed(quotient) == m.cached_volume)
        return;
    m.cached_volume = as_signed(quotient); // 45F862 precedes table read, including failure
    const auto volume_address = 0x4897f8u + (sar(quotient, 1) << 1);
    const auto mapped_volume = static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(literals_.read_audio_level_word(volume_address, 0x45f86c)));
    (void)buffers_.volume(buffer, mapped_volume);
}
} // namespace sgn::retail
