#include "sgn/source_replay_decompress.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/bit_reinterpret.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>

extern "C" void sgn_replay_normalize_43b470(std::uint8_t *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
using sgn::bit_reinterpret::as_signed;
using sgn::bit_reinterpret::as_unsigned;
template <class T> T read(std::span<const std::uint8_t> memory, std::size_t at)
{
    if (at > memory.size() || sizeof(T) > memory.size() - at)
        throw std::out_of_range("43BBB0 read leaves borrowed owner; partial output retained");
    T result;
    std::memcpy(&result, memory.data() + at, sizeof(T));
    return result;
}
template <class T> void write(std::span<std::uint8_t> memory, std::size_t at, T value)
{
    if (at > memory.size() || sizeof(T) > memory.size() - at)
        throw std::out_of_range("43BBB0 write leaves borrowed owner; partial output retained");
    std::memcpy(memory.data() + at, &value, sizeof(T));
}
struct Reader
{ // actual43B380 call-local owner; control bits and bytes interleave
    std::span<const std::uint8_t> source;
    std::size_t cursor;
    std::uint8_t shifted, current;
    explicit Reader(std::span<const std::uint8_t> input) : source(input), cursor(0xac)
    {
        refill();
    }
    W byte()
    { // 43B370 reads BEFORE advancing its cursor
        const auto value = read<std::uint8_t>(source, cursor);
        ++cursor;
        return value;
    }
    void refill()
    {
        const auto value = byte();
        current = static_cast<std::uint8_t>(value >> 7);
        shifted = static_cast<std::uint8_t>((value << 1) | 1u);
    }
    W one()
    { // 43B3B0 eagerly fetches the next control byte on the eighth bit
        const W result = current;
        current = shifted >> 7;
        shifted = static_cast<std::uint8_t>(shifted << 1);
        if (shifted == 0)
            refill();
        return result;
    }
    W field(W width)
    { // 43B3F0; reached widths3/4/5, full byte prefix retained
        W result = 0;
        if (width >= 8)
        {
            auto bytes = width >> 3;
            width -= bytes * 8;
            do
            {
                result = (result << 8) | byte();
            } while (--bytes != 0);
        }
        while (width != 0)
        {
            --width;
            result = (result << 1) + one();
        }
        return result;
    }
    W delta(W width, bool optional)
    { // 43BB00, literal signed division by2
        if (optional && one() == 0)
            return 0;
        const auto exponent = as_signed(field(width) - (1u << width) / 2u);
        if (exponent == 0)
            return 0;
        const auto magnitude = frontend_descriptor_absolute_43d9b0(as_unsigned(exponent));
        const W shifted_value = exponent < 0 ? (0xffffffffu << (magnitude & 31u)) : (1u << (magnitude & 31u));
        return as_unsigned(as_signed(shifted_value) / 2);
    }
    W sign()
    { // 43BB90: absent0, bits10 ->-1, bits11 ->+1
        if (one() == 0)
            return 0;
        return one() == 0 ? 0xffffffffu : 1u;
    }
};
} // namespace
W decompress_source_replay_43bbb0(std::span<std::uint8_t> destination, std::span<const std::uint8_t> source)
{
    for (std::size_t at = 0; at < 0xac; at += 4)
        write<W>(destination, at, read<W>(source, at)); // 43BBCA forward REP
    Reader input(source);                               // 43BBE8, BEFORE first-frame copy and terminal test
    std::array<std::uint8_t, 44> previous;
    for (std::size_t at = 0; at < 44; at += 4)
        write<W>(previous, at, read<W>(destination, 0x80 + at)); // 43BC06
    std::array<W, 3> positions{};
    std::array<W, 4> angles{};
    W count = 1;
    std::size_t destination_frame = 0xac;
    while ((previous[0x24] & 1u) == 0)
    {
        positions[0] += input.delta(5, false);
        positions[1] += input.delta(5, true);
        positions[2] += input.delta(5, false);
        for (const std::size_t i : std::array<std::size_t, 3>{2, 0, 1})
            write<W>(previous, i * 4, read<W>(previous, i * 4) + positions[i]);
        for (auto &value : angles)
            value += input.delta(5, false);
        for (const std::size_t i : std::array<std::size_t, 4>{0, 1, 3, 2})
            write<std::uint16_t>(previous, 0xc + i * 2,
                                 static_cast<std::uint16_t>(read<std::uint16_t>(previous, 0xc + i * 2) + angles[i]));
        for (std::size_t at = 0x14; at < 0x1c; at += 2)
        {
            const auto step = input.delta(4, true) << 3;
            write<std::uint16_t>(previous, at, static_cast<std::uint16_t>(read<std::uint16_t>(previous, at) + step));
        }
        for (const auto range : std::array<std::array<std::size_t, 3>, 2>{{{0x1c, 4, 3}, {0x26, 6, 4}}})
            for (std::size_t i = 0; i < range[1]; ++i)
            {
                const auto step = input.delta(static_cast<W>(range[2]), true);
                previous[range[0] + i] = static_cast<std::uint8_t>(previous[range[0] + i] + step);
            }
        const auto rpm = input.delta(5, false);
        write<std::uint16_t>(previous, 0x20, static_cast<std::uint16_t>(read<std::uint16_t>(previous, 0x20) + rpm));
        for (std::size_t at = 0x22; at <= 0x23; ++at)
        {
            const auto step = input.sign();
            previous[at] = static_cast<std::uint8_t>(previous[at] + step);
        }
        for (const std::size_t at : std::array<std::size_t, 2>{0x25, 0x24})
            if (input.one() != 0)
                previous[at] = static_cast<std::uint8_t>(input.byte());
        // 43BE91 makes a SECOND44-byte actual stack local. Only this output
        // copy is normalized; feeding it back to predictor changes the codec.
        auto output = previous;
        sgn_replay_normalize_43b470(output.data() + 0x26);
        sgn_replay_normalize_43b470(output.data() + 0x29);
        ++count;
        for (std::size_t at = 0; at < 44; at += 4)
            write<W>(destination, destination_frame + at, read<W>(output, at));
        destination_frame += 44;
    }
    return 0x80u + count * 44u; // 43BEDA..BEE3, terminal frame included
}
W decompress_frontend_saved_replay_44d9e9(FrontendState &f, W address)
{
    f.shared_platform()->require_owner_thread();
    const auto offset = address - ReplayBank::source_base;
    if (offset >= ReplayBank::size || offset % ReplayBank::stride != 0)
        throw FormatError("44D9E9 destination is not an existing ReplayBank slot");
    // Same retained60000-byte frontend payload region borrowed by Native CRT;
    // no temporary decompressed recording or independent parsed-header copy.
    return decompress_source_replay_43bbb0(f.world().replays.bytes().subspan(offset),
                                           f.source.source_bytes(0x5e3c48, 60000));
}
} // namespace sgn::retail
