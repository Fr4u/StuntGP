#include "sgn/source_input_cold_init.hpp"
#include <bit>
#include <type_traits>

namespace sgn::retail
{
namespace
{
static_assert(std::endian::native == std::endian::little);
static_assert(std::is_same_v<std::uint8_t, unsigned char>);
static_assert(sizeof(std::uint32_t) == 4);
static_assert(sizeof(std::array<std::uint32_t, 79>) == 316);
static_assert(sizeof(std::array<std::uint32_t, 8>) == 32);
static_assert(sizeof(std::array<std::uint8_t, 32>) == 32);
static_assert(sizeof(std::array<std::uint8_t, 2>) == 2);
static_assert(sizeof(std::array<std::uint8_t, 4>) == 4);
constexpr std::size_t physical_stride = 0x1dc;
constexpr std::size_t physical_count = 4;
using ByteTargets = std::array<std::uint8_t *, physical_stride * physical_count>;
template <class T> void bind_object(ByteTargets &view, std::size_t base, T &object) noexcept
{
    auto *bytes = reinterpret_cast<unsigned char *>(&object);
    for (std::size_t i = 0; i < sizeof(T); ++i)
        view[base + i] = bytes + i;
}
ByteTargets borrow_physical_bytes(SourceInputColdWorld w) noexcept
{
    ByteTargets view{};
    for (std::size_t slot = 0; slot < physical_count; ++slot)
    {
        const auto base = slot * physical_stride;
        auto &raw = w.input.acquisition.physical[slot];
        auto &extra = w.input.physical[slot];
        auto &words = w.menu.raw_words[slot];
        auto &binding = w.frontend.input_bindings[slot];
        bind_object(view, base + 0x00, words.word_0);
        bind_object(view, base + 0x04, words.word_4);
        bind_object(view, base + 0x08, raw.word_8);
        bind_object(view, base + 0x0c, raw.word_0c);
        bind_object(view, base + 0x10, extra.activity_kind);
        bind_object(view, base + 0x14, extra.profile_index);
        bind_object(view, base + 0x18, binding.vehicle);
        bind_object(view, base + 0x1c, binding.kind);
        bind_object(view, base + 0x20, extra.feedback_mode);
        bind_object(view, base + 0x24, raw.word_24);
        bind_object(view, base + 0x28, raw.word_28);
        bind_object(view, base + 0x2c, raw.words_from_2c);
        bind_object(view, base + 0x168, extra.bytes_168_187);
        bind_object(view, base + 0x188, extra.bytes_188_1a7);
        const auto edges = borrow_menu_input_bank(w.menu.edges[slot], raw.extra_edges);
        for (std::size_t i = 0; i < 7; ++i)
        {
            view[base + 0x1a8 + i] = &edges.current[i].get();
            view[base + 0x1af + i] = &edges.previous[i].get();
        }
        bind_object(view, base + 0x1b6, extra.bytes_1b6_1b7);
        bind_object(view, base + 0x1b8, w.feedback.physical_words[slot]);
        bind_object(view, base + 0x1d8, extra.bytes_1d8_1db);
    }
    return view;
}
void store_dword(const ByteTargets &view, std::size_t offset, std::uint32_t value) noexcept
{
    // Aliasing through unsigned char is defined for the actual scalar owners.
    // No cast pretends paired edge structs are source-contiguous records.
    for (unsigned byte = 0; byte < 4; ++byte)
        *view[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
}
static_assert(11 * 4 + 79 * 4 + 32 + 32 + 7 + 7 + 2 + 8 * 4 + 4 == physical_stride);
} // namespace
void initialize_source_physical_input_45ec30(SourceInputColdWorld world) noexcept
{
    const auto targets = borrow_physical_bytes(world); // address binding only
    // 45EC31..45EC3D: ECX=0x1DC counts DWORDs, not bytes or single records.
    for (std::size_t offset = 0; offset < physical_count * physical_stride; offset += 4)
        store_dword(targets, offset, 0);
    // 45EC4C..45EC63 in literal source order after ALL four records were cleared.
    for (std::size_t slot = 0; slot < physical_count; ++slot)
    {
        const auto base = slot * physical_stride;
        store_dword(targets, base + 0x00, 0);
        store_dword(targets, base + 0x08, 0);
        store_dword(targets, base + 0x0c, 0);
        store_dword(targets, base + 0x14, 0);
        store_dword(targets, base + 0x18, 0xffff);
        store_dword(targets, base + 0x1c, 0);
        store_dword(targets, base + 0x20, 0);
        store_dword(targets, base + 0x24, 0);
        store_dword(targets, base + 0x28, 0);
    }
}
} // namespace sgn::retail
