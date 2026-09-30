#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace sgn::diagnostics
{
struct WorldJoinFingerprint
{
    std::array<std::uint8_t, 32> sha256{};
    std::uint64_t bytes{};
    bool operator==(const WorldJoinFingerprint &) const = default;
};
// NEW standard SHA-256, integer-only CPU checksum of serialized artifact bytes.
class WorldJoinSha256
{
    std::array<std::uint32_t, 8> state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> tail_{};
    std::uint64_t bytes_{};
    std::size_t used_{};
    void block(const std::uint8_t *) noexcept;

  public:
    void append(std::span<const std::uint8_t>);
    WorldJoinFingerprint finish() const;
};
WorldJoinFingerprint fingerprint_world_join_file(const std::filesystem::path &);
} // namespace sgn::diagnostics
