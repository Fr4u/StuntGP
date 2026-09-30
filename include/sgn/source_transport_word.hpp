#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <cstdio>
#include <string>

// NEW R19 (perf_transport, ROOT DIRECTIVE): per-byte provenance of unproduced
// source DWORDs is an optional debug diagnostic, OFF in the shipped game.
//   SGN_SOURCE_PROVENANCE=0 (default): plain bytes. An unproduced original
//     stack/static DWORD (SourceTransportWord::opaque) is resolved at its
//     producer to the documented NEW default value 0 (PORT-FIX style); no
//     origin is ever present, nothing is tracked and SourceOpaqueRead is never
//     thrown by the transport types.
//   SGN_SOURCE_PROVENANCE=1: diagnostic build. Every opaque byte keeps its
//     (function, instruction, frame offset, lane) origin; numeric borrows of
//     such bytes throw SourceOpaqueRead at the reading instruction.
// The public API is identical in both modes.
#ifndef SGN_SOURCE_PROVENANCE
#define SGN_SOURCE_PROVENANCE 0
#endif

namespace sgn::retail
{
static_assert(std::endian::native == std::endian::little,
              "source DWORD byte transport requires the admitted little-endian host");
inline constexpr bool source_provenance_enabled = SGN_SOURCE_PROVENANCE != 0;
// NEW host validity metadata. An origin is neither a guest address nor a
// replacement numeric value. Distinct origins never imply unequal guest bits.
struct SourceOpaqueWordOrigin
{
    std::uint32_t function, instruction;
    std::int32_t frame_offset;
};
// Value handle for one opaque byte's origin (formerly a shared_ptr): keeps the
// pointer-like surface used by callers (contextual bool, reset(), get(),
// operator-> and operator*). Identity is the value (function, instruction,
// frame offset) together with the byte lane.
#if SGN_SOURCE_PROVENANCE
class SourceOpaqueOrigin
{
    SourceOpaqueWordOrigin value_{};
    bool present_{};

  public:
    constexpr SourceOpaqueOrigin() noexcept = default;
    constexpr SourceOpaqueOrigin(std::nullptr_t) noexcept {}
    constexpr explicit SourceOpaqueOrigin(const SourceOpaqueWordOrigin &value) noexcept : value_(value), present_(true)
    {
    }
    explicit SourceOpaqueOrigin(const std::shared_ptr<const SourceOpaqueWordOrigin> &from) noexcept
    {
        if (from)
        {
            value_ = *from;
            present_ = true;
        }
    }
    constexpr explicit operator bool() const noexcept
    {
        return present_;
    }
    constexpr void reset() noexcept
    {
        value_ = {};
        present_ = false;
    }
    constexpr const SourceOpaqueWordOrigin *get() const noexcept
    {
        return present_ ? &value_ : nullptr;
    }
    constexpr const SourceOpaqueWordOrigin &operator*() const noexcept
    {
        return value_;
    }
    constexpr const SourceOpaqueWordOrigin *operator->() const noexcept
    {
        return &value_;
    }
    friend constexpr bool operator==(const SourceOpaqueOrigin &a, const SourceOpaqueOrigin &b) noexcept
    {
        return a.present_ == b.present_ && (!a.present_ || (a.value_.function == b.value_.function &&
                                                            a.value_.instruction == b.value_.instruction &&
                                                            a.value_.frame_offset == b.value_.frame_offset));
    }
};
#else
// Shipped game: provenance compiled out. Never present; all operations are no-ops.
class SourceOpaqueOrigin
{
    static constexpr SourceOpaqueWordOrigin none_{};

  public:
    constexpr SourceOpaqueOrigin() noexcept = default;
    constexpr SourceOpaqueOrigin(std::nullptr_t) noexcept {}
    constexpr explicit SourceOpaqueOrigin(const SourceOpaqueWordOrigin &) noexcept {}
    explicit SourceOpaqueOrigin(const std::shared_ptr<const SourceOpaqueWordOrigin> &) noexcept {}
    constexpr explicit operator bool() const noexcept
    {
        return false;
    }
    constexpr void reset() noexcept {}
    constexpr const SourceOpaqueWordOrigin *get() const noexcept
    {
        return nullptr;
    }
    constexpr const SourceOpaqueWordOrigin &operator*() const noexcept
    {
        return none_;
    }
    constexpr const SourceOpaqueWordOrigin *operator->() const noexcept
    {
        return &none_;
    }
    friend constexpr bool operator==(const SourceOpaqueOrigin &, const SourceOpaqueOrigin &) noexcept
    {
        return true;
    }
};
#endif
struct SourceTransportByte
{
    std::uint8_t literal{}, lane{};
    SourceOpaqueOrigin origin;
};
static_assert(std::is_trivially_copyable_v<SourceOpaqueOrigin> && std::is_trivially_copyable_v<SourceTransportByte>);
class SourceOpaqueRead final : public std::runtime_error
{
  public:
    const std::uint32_t instruction;
    const SourceOpaqueOrigin origin;
    SourceOpaqueRead(std::uint32_t at, SourceOpaqueOrigin from)
        : std::runtime_error(describe(at, from.get())), instruction(at), origin(from)
    {
    }
    SourceOpaqueRead(std::uint32_t at, const std::shared_ptr<const SourceOpaqueWordOrigin> &from)
        : SourceOpaqueRead(at, SourceOpaqueOrigin(from))
    {
    }

  private:
    static std::string describe(std::uint32_t at, const SourceOpaqueWordOrigin *o)
    {
        char text[192];
        // clang-format off: keep the long format string one literal (no split)
        if(o)std::snprintf(text,sizeof text,"numeric read reaches a source DWORD whose bits remain opaque (read at 0x%X; unproduced word from 0x%X at 0x%X, frame %d)",
            at,o->function,o->instruction,o->frame_offset);
        else std::snprintf(text,sizeof text,"numeric read reaches a source DWORD whose bits remain opaque (read at 0x%X)",at);
        // clang-format on
        return text;
    }
};
struct SourceTransportWord
{
    std::array<SourceTransportByte, 4> bytes;
    static SourceTransportWord known(std::uint32_t value) noexcept
    {
        SourceTransportWord out{};
        for (unsigned i = 0; i < 4; ++i)
            out.bytes[i].literal = static_cast<std::uint8_t>(value >> (i * 8));
        return out;
    }
    // An unproduced original DWORD. Diagnostic build: four opaque bytes sharing
    // one origin, lanes 0..3. Shipped game: the documented NEW default, known 0.
    static SourceTransportWord opaque(std::uint32_t function, std::uint32_t instruction, std::int32_t offset) noexcept
    {
#if SGN_SOURCE_PROVENANCE
        SourceTransportWord out{};
        const SourceOpaqueOrigin identity(SourceOpaqueWordOrigin{function, instruction, offset});
        for (unsigned i = 0; i < 4; ++i)
        {
            out.bytes[i].origin = identity;
            out.bytes[i].lane = static_cast<std::uint8_t>(i);
        }
        return out;
#else
        (void)function;
        (void)instruction;
        (void)offset;
        return known(0); // NEW default (PORT-FIX style): unproduced source DWORD = 0
#endif
    }
    constexpr bool is_known() const noexcept
    {
        return !bytes[0].origin && !bytes[1].origin && !bytes[2].origin && !bytes[3].origin;
    }
    std::uint32_t numeric(std::uint32_t instruction) const
    {
#if SGN_SOURCE_PROVENANCE
        // Same first-opaque-lane diagnosis order as the former loop.
        for (unsigned i = 0; i < 4; ++i)
            if (bytes[i].origin)
                throw SourceOpaqueRead(instruction, bytes[i].origin);
#else
        (void)instruction;
#endif
        return std::uint32_t(bytes[0].literal) | std::uint32_t(bytes[1].literal) << 8 |
               std::uint32_t(bytes[2].literal) << 16 | std::uint32_t(bytes[3].literal) << 24;
    }
};
static_assert(std::is_trivially_copyable_v<SourceTransportWord>);
} // namespace sgn::retail
