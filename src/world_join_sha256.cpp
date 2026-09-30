#include "sgn/world_join_sha256.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace sgn::diagnostics
{
void WorldJoinSha256::block(const std::uint8_t *data) noexcept
{
    static constexpr std::array<std::uint32_t, 64> k{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::array<std::uint32_t, 64> w{};
    for (unsigned i = 0; i < 16; ++i)
        w[i] = (std::uint32_t(data[4 * i]) << 24) | (std::uint32_t(data[4 * i + 1]) << 16) |
               (std::uint32_t(data[4 * i + 2]) << 8) | data[4 * i + 3];
    for (unsigned i = 16; i < 64; ++i)
    {
        const auto a = w[i - 15], b = w[i - 2];
        w[i] = w[i - 16] + (std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3)) + w[i - 7] +
               (std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10));
    }
    auto [a, b, c, d, e, f, g, h] = state_;
    for (unsigned i = 0; i < 64; ++i)
    {
        const auto t1 =
            h + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        const auto t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}
void WorldJoinSha256::append(std::span<const std::uint8_t> data)
{
    if (data.size() > (std::numeric_limits<std::uint64_t>::max)() / 8 - bytes_)
        throw std::length_error("SHA256 input length overflow");
    bytes_ += data.size();
    while (!data.empty())
    {
        if (used_ == 0 && data.size() >= 64)
        {
            block(data.data());
            data = data.subspan(64);
            continue;
        }
        const auto take = (std::min)(data.size(), tail_.size() - used_);
        std::copy_n(data.data(), take, tail_.data() + used_);
        used_ += take;
        data = data.subspan(take);
        if (used_ == 64)
        {
            block(tail_.data());
            used_ = 0;
        }
    }
}
WorldJoinFingerprint WorldJoinSha256::finish() const
{
    auto copy = *this;
    const auto bits = bytes_ * 8;
    copy.tail_[copy.used_++] = 0x80;
    if (copy.used_ > 56)
    {
        std::fill(copy.tail_.begin() + copy.used_, copy.tail_.end(), std::uint8_t{0});
        copy.block(copy.tail_.data());
        copy.used_ = 0;
    }
    std::fill(copy.tail_.begin() + copy.used_, copy.tail_.begin() + 56, std::uint8_t{0});
    for (unsigned i = 0; i < 8; ++i)
        copy.tail_[63 - i] = static_cast<std::uint8_t>(bits >> (i * 8));
    copy.block(copy.tail_.data());
    WorldJoinFingerprint out;
    out.bytes = bytes_;
    for (unsigned i = 0; i < 8; ++i)
        for (unsigned j = 0; j < 4; ++j)
            out.sha256[i * 4 + j] = static_cast<std::uint8_t>(copy.state_[i] >> (24 - j * 8));
    return out;
}
WorldJoinFingerprint fingerprint_world_join_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot read displayed recipe for fingerprint");
    std::array<char, 65536> bytes{};
    WorldJoinSha256 hash;
    while (in)
    {
        in.read(bytes.data(), bytes.size());
        const auto n = in.gcount();
        if (n > 0)
            hash.append({reinterpret_cast<const std::uint8_t *>(bytes.data()), static_cast<std::size_t>(n)});
    }
    if (in.bad() || !in.eof())
        throw std::runtime_error("displayed recipe fingerprint read failed");
    return hash.finish();
}
} // namespace sgn::diagnostics
