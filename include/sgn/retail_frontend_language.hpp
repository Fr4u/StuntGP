#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
inline constexpr unsigned frontend_language_catalog_words = 0x128 / 4;
inline constexpr unsigned frontend_text_count = 0x2dc;
inline constexpr unsigned frontend_language_arena_bytes = 0x6000;
// Native identities, never guest pointers. The input bytes must be retained by
// the actual resource owner while frontend language operations borrow them.
struct FrontendLanguageInput
{
    std::span<const std::uint8_t> bytes;
    std::uint32_t identity;
};
struct FrontendLanguageCatalogViews
{
    // 5E37B8 current language, eight36-byte name/flag records,5E38DC count.
    // This view aliases FrontendSourceStorage's existing cold_suffix owner.
    std::span<std::uint32_t, frontend_language_catalog_words> catalog;
    std::span<std::uint32_t, frontend_text_count> text_handles; // 605CC8
    // 0 denotes the existing empty sentinel;1..8 denotes catalog row0..7.
    std::span<std::uint32_t, 9> language_names; // 606838
    std::uint32_t &input_identity;              // 6087A0, new host resource identity
};
// Legacy post-load host adapter for440AE0 catalog parsing. The caller retains
// the actual resource bytes. This adapter preflights before publication; it does
// not implement440AE0's resource load or interleaved source publication order.
// The return is a host diagnostic of the local cursor; original440AE0 is void.
// No names/flags are defaults. Failed %d retains the last successful value in
// this invocation (440BFF..440C0A). A first failure leaves an undefined local;
// it, invalid input and oversized names/tokens are rejected atomically here.
std::uint32_t initialize_frontend_language_catalog(FrontendLanguageCatalogViews, FrontendLanguageInput);

// The existing FrontendSourceStorage arena and catalog, borrowed without copies.
// Identity0 is null; identities1..6000 encode arena offsets0..5FFF plus1.
// The high-bit form encodes (byte offset+1) in the existing language catalog.
// It preserves a borrowed name pointer across later table changes, not a copy.
// The tag alone denotes the existing empty sentinel at source6087A4.
inline constexpr std::uint32_t frontend_catalog_text_tag = 0x80000000u;
struct FrontendLanguageTextIdentity
{
    std::uint32_t value{};
    bool operator==(const FrontendLanguageTextIdentity &) const = default;
};
struct FrontendLanguageStrings
{
    std::span<const std::uint8_t, frontend_language_arena_bytes> arena;
    std::span<const std::uint32_t, frontend_text_count> text_handles;
    // Optional in legacy two-field callers; actual FrontendState binds both.
    // 606838 immediately follows605CC8's732words; names1..8 address catalog rows.
    std::span<const std::uint32_t> language_names{};
    std::span<const std::uint8_t> catalog{};
    std::span<const std::uint8_t> empty_sentinel{};
};
// Full441170. Bounds checks are a new host rejection of invalid source reads.
FrontendLanguageTextIdentity frontend_language_text_441170(FrontendLanguageStrings, std::uint32_t id);
// New host pointer resolution. Null returns an empty span; a valid empty string
// returns a one-byte span containing NUL. The returned span includes the NUL and
// borrows the existing arena/catalog/sentinel, so source mutations remain visible.
std::span<const std::uint8_t> resolve_frontend_language_text(FrontendLanguageStrings, FrontendLanguageTextIdentity);
} // namespace sgn::retail
