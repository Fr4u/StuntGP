#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

// NEW HOST (R30) persistent cache of converted textures (not recovered behaviour). A result of a pure host
// texture conversion (the sharp-HUD upscale, the edge fix / 8-bit / mip chain) is stored once it was
// expensive to compute and is loaded instead of recomputed in a later run. A loaded result is byte for
// byte the one the conversion computes, because an entry is used only when ALL of these match:
//   - the build: an identity of the running executable image (PE link timestamp, image size, checksum,
//     entry point) and the cache format version, so any rebuild invalidates every entry;
//   - the conversion: `kind` and the caller's `params` (settings profile, factor, scaler, level count and
//     the caller's own algorithm version);
//   - the exact source: width, height and every source word (the entry stores a full copy and compares it);
//   - the payload: its length and a 64-bit checksum (a torn or corrupted file is ignored).
// Entries live in <folder>/<build identity>/<key>.sgntex. Writes are asynchronous (background thread,
// temporary file + rename) and never block the caller; I/O errors only disable the affected entry. Entries
// of other builds are removed in the background and the current build's folder is kept under 512 MB
// (oldest entries first). Only files this cache writes (*.sgntex, *.sgntmp) are ever removed.
// Disabled until configure() is called, and when the environment sets SGN_TEXTURE_CACHE=0.
namespace sgn::host_texture_disk_cache
{
void configure(const std::filesystem::path &folder) noexcept;
bool enabled() noexcept;
// NEW HOST (R35): the settings folder (the parent of the configured cache folder), empty until configure()
// ran. The shader bytecode cache (host_shader_cache) keeps its files beside it.
std::filesystem::path settings_folder();
struct Key
{
    std::uint32_t kind{};
    std::array<std::uint64_t, 4> params{};
    std::uint32_t width{}, height{};
    std::span<const std::uint16_t> source;
};
// The payload of a valid entry, or nullopt (disabled, missing, other build, mismatch, corrupt, I/O error).
std::optional<std::vector<std::uint8_t>> load(const Key &) noexcept;
// Queues an asynchronous write of the entry (the key's source is copied). Dropped when disabled or when
// more than 256 MB of writes are already pending.
void store(const Key &, std::vector<std::uint8_t> payload) noexcept;
struct Statistics
{
    std::uint64_t hits{}, misses{}, rejected{}, writes{}, write_failures{}, dropped{};
};
Statistics statistics() noexcept;
// 64-bit hash of 16-bit words (not cryptographic; keys and checksums only).
std::uint64_t hash_words(std::span<const std::uint16_t> words, std::uint64_t seed) noexcept;
std::uint64_t hash_bytes(std::span<const std::uint8_t> bytes, std::uint64_t seed) noexcept;
} // namespace sgn::host_texture_disk_cache
