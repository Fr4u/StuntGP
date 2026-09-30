#include "sgn/source_replay_compress.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
static_assert(std::endian::native == std::endian::little);
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
template <class T> T read(std::span<const std::uint8_t> memory, std::size_t offset)
{
    if (offset > memory.size() || sizeof(T) > memory.size() - offset)
        throw std::out_of_range("43B640 source read outside borrowed recording; partial output retained");
    T value;
    std::memcpy(&value, memory.data() + offset, sizeof(value));
    return value;
}
template <class T> void write(std::span<std::uint8_t> memory, std::size_t offset, T value)
{
    if (offset > memory.size() || sizeof(T) > memory.size() - offset)
        throw std::out_of_range("43B640 output write outside borrowed destination; partial output retained");
    std::memcpy(memory.data() + offset, &value, sizeof(value));
}
W signed_byte(std::span<const std::uint8_t> memory, std::size_t offset)
{
    return as_unsigned(std::bit_cast<std::int8_t>(read<std::uint8_t>(memory, offset)));
}
W signed_half(std::span<const std::uint8_t> memory, std::size_t offset)
{
    return as_unsigned(std::bit_cast<std::int16_t>(read<std::uint16_t>(memory, offset)));
}
// Only the actual call-local43B270 writer. No new persistent buffer owner.
struct Writer
{
    std::span<std::uint8_t> destination;
    W count = 1, control = 0, bit = 0, capacity;
    Writer(std::span<std::uint8_t> out, W source_capacity) : destination(out), capacity(source_capacity)
    {
        write<std::uint8_t>(destination, 0, 0); // 43B28A, even with source capacity0
    }
    void byte(W value)
    { // 43B2A0: append beside, not inside, current control byte
        const auto at = count++;
        write<std::uint8_t>(destination, at, static_cast<std::uint8_t>(value));
    }
    void one(W value)
    { // 43B2B0
        if (value != 0)
        {
            const auto old = read<std::uint8_t>(destination, control);
            write<std::uint8_t>(destination, control, static_cast<std::uint8_t>(old | (0x80u >> bit)));
        }
        if (++bit == 8)
        {
            bit = 0;
            control = count++;
            write<std::uint8_t>(destination, control, 0);
        }
    }
    void field(W value, W width)
    { // 43B300, actual widths1/3/4/5; byte path retained
        if (width >= 8)
        {
            auto bytes = width >> 3;
            auto shift = width - 8;
            width -= bytes * 8;
            do
            {
                byte(as_unsigned(as_signed(value) >> (shift & 31u)));
                shift -= 8;
            } while (--bytes != 0);
        }
        while (width != 0)
        {
            --width;
            one((value >> (width & 31u)) & 1u);
        }
    }
};
// 43B4F0. All reached widths3,4,5 keep source shifts/divides in this domain.
W quantize(Writer &out, W value, W width, bool optional)
{
    const W levels = 1u << width, center = levels / 2;
    auto magnitude = frontend_descriptor_absolute_43d9b0(value);
    W encoded = center, reconstructed = 0;
    if (magnitude != 0)
    {
        // 43D9B0(INT_MIN)==INT_MIN; SAR43B516 never reaches zero. Do not hang
        // the modern host or reinterpret that source path as a valid delta.
        if (magnitude == 0x80000000u)
            throw std::domain_error("43B4F0 INT_MIN delta enters nonterminating source SAR loop at43B515");
        W length = 0;
        do
        {
            ++length;
            magnitude >>= 1;
        } while (magnitude != 0);
        const auto signed_length =
            as_signed(value) < 0 ? -static_cast<std::int32_t>(length) : static_cast<std::int32_t>(length);
        auto code = signed_length + static_cast<std::int32_t>(center);
        if (code < 0)
            code = 0;
        if (code > static_cast<std::int32_t>(levels - 1))
            code = static_cast<std::int32_t>(levels - 1);
        encoded = static_cast<W>(code);
        const auto exponent = code - static_cast<std::int32_t>(center);
        if (exponent < 0)
        {
            const auto shifted = 0xffffffffu << static_cast<unsigned>(-exponent);
            reconstructed = as_unsigned(as_signed(shifted) / 2); // 43B53F..552, signed truncation
        }
        else
            reconstructed = (1u << static_cast<unsigned>(exponent)) / 2;
    }
    // 43B578/582 call the same side-effect-free absolute leaf; results discarded.
    if (optional)
    {
        out.field(reconstructed != 0 ? 1u : 0u, 1);
        if (reconstructed == 0)
            return 0;
    }
    out.field(encoded, width);
    return reconstructed;
}
W sign_delta(Writer &out, W value)
{ // 43B5F0: 0=>0, negative=>10, positive=>11
    if (value == 0)
    {
        out.field(0, 1);
        return 0;
    }
    out.field(1, 1);
    out.field(as_signed(value) < 0 ? 0u : 1u, 1);
    return as_signed(value) < 0 ? 0xffffffffu : 1u;
}
} // namespace
W compress_source_replay_43b640(std::span<std::uint8_t> destination, W capacity, std::span<const std::uint8_t> source)
{
    if (as_signed(capacity) < 0xac)
        return 0; // 43B64A..66F, no borrowed memory access
    for (std::size_t at = 0; at < 0xac; at += 4)
        write<W>(destination, at, read<W>(source, at)); // 43B682 forward DWORD copy
    // Do not trim to capacity: source43B270 writes even when capacity-172==0.
    if (destination.size() < 0xac)
        throw std::out_of_range("43B640 header exceeds destination");
    Writer out(destination.subspan(0xac), capacity - 0xacu);
    std::array<std::uint8_t, 44> previous; // actual44-byte source local, fully copied
    for (std::size_t at = 0; at < 44; at += 4)
        write<W>(previous, at, read<W>(source, 0x80 + at));
    std::array<W, 3> position_delta{}; // actual zeroed locals/EBX
    std::array<W, 4> angle_delta{};
    std::size_t frame = 0xac;
    while ((previous[0x24] & 1u) == 0)
    {
        if (as_signed(out.count) > as_signed(out.capacity - 0x80u))
            return 0; // 43B6BB..C6
        // 43B6C8..740: second differences for positions; Y has a zero marker.
        for (std::size_t i = 0; i < 3; ++i)
        {
            const auto error = read<W>(source, frame + i * 4) - read<W>(previous, i * 4) - position_delta[i];
            position_delta[i] += quantize(out, error, 5, i == 1);
        }
        for (std::size_t i = 0; i < 3; ++i)
            write<W>(previous, i * 4, read<W>(previous, i * 4) + position_delta[i]);
        // 43B745..799 reads ALL four halfword differences before emitting any.
        // 43B210 subtracts DWORDs, but only AX is sign-extended by the caller.
        std::array<W, 4> angle_error;
        for (std::size_t i = 0; i < 4; ++i)
        {
            const auto low = static_cast<std::uint16_t>(read<std::uint16_t>(source, frame + 0xc + i * 2) -
                                                        read<std::uint16_t>(previous, 0xc + i * 2));
            angle_error[i] = as_unsigned(std::bit_cast<std::int16_t>(low)) - angle_delta[i];
        }
        for (std::size_t i = 0; i < 4; ++i)
            angle_delta[i] += quantize(out, angle_error[i], 5, false);
        for (const std::size_t i : std::array<std::size_t, 4>{0, 2, 3, 1})
            write<std::uint16_t>(
                previous, 0xc + i * 2,
                static_cast<std::uint16_t>(read<std::uint16_t>(previous, 0xc + i * 2) + angle_delta[i]));
        // 43B823..8EC: signed halfword differences /8, truncating toward zero.
        for (std::size_t i = 0; i < 4; ++i)
        {
            const auto at = 0x14 + i * 2;
            const auto difference = as_signed(signed_half(source, frame + at) - signed_half(previous, at));
            const auto step = quantize(out, as_unsigned(difference / 8), 4, true);
            write<std::uint16_t>(previous, at,
                                 static_cast<std::uint16_t>(read<std::uint16_t>(previous, at) + (step << 3)));
        }
        // 43B8E3..953: four signed suspension bytes, then43B958..BA19 six basis bytes.
        for (const auto range : std::array<std::array<std::size_t, 3>, 2>{{{0x1c, 4, 3}, {0x26, 6, 4}}})
        {
            for (std::size_t i = 0; i < range[1]; ++i)
            {
                const auto at = range[0] + i;
                const auto step = quantize(out, signed_byte(source, frame + at) - signed_byte(previous, at),
                                           static_cast<W>(range[2]), true);
                previous[at] = static_cast<std::uint8_t>(previous[at] + step);
            }
        }
        // 43BA20..46 uses UNSIGNED RPM words before the signed difference.
        const auto rpm_error = W{read<std::uint16_t>(source, frame + 0x20)} - read<std::uint16_t>(previous, 0x20);
        const auto rpm_step = quantize(out, rpm_error, 5, false);
        write<std::uint16_t>(previous, 0x20,
                             static_cast<std::uint16_t>(read<std::uint16_t>(previous, 0x20) + rpm_step));
        for (std::size_t at = 0x22; at <= 0x23; ++at)
        {
            const auto step = sign_delta(out, signed_byte(source, frame + at) - signed_byte(previous, at));
            previous[at] = static_cast<std::uint8_t>(previous[at] + step);
        }
        // 43BA76..BADD: effect byte first, flags second; bytes are interleaved
        // with the current control byte. No forced flush of a partial group.
        for (const std::size_t at : std::array<std::size_t, 2>{0x25, 0x24})
        {
            const auto current = read<std::uint8_t>(source, frame + at);
            const bool changed = current != previous[at];
            out.field(changed ? 1u : 0u, 1);
            if (changed)
            {
                out.byte(current);
                previous[at] = current;
            }
        }
        frame += 44;
    }
    return out.count; // 43BAEB, excludes the copied172-byte prefix
}
W compress_source_replay_to_frontend_43b640(FrontendSourceStorage &frontend, W capacity,
                                            std::span<const std::uint8_t> source)
{
    if (as_signed(capacity) < 0xac)
        return 0;
    // Actual44DC56 owner, already shared with Native replay persistence.
    return compress_source_replay_43b640(frontend.source_bytes(0x5eb178, 30000), capacity, source);
}
} // namespace sgn::retail
