#pragma once
#include "sgn/captured_front_recipe.hpp"
#include "sgn/world_join_sidecar.hpp"
#include <fstream>
#include <locale>
#include <limits>
#include <sstream>
#include <string_view>

namespace sgn::diagnostics
{
namespace front_bundle_detail
{
inline std::string hex(std::span<const std::uint8_t> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto byte : bytes)
    {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 15]);
    }
    return out;
}
inline WorldJoinFingerprint bytes_fingerprint(std::span<const std::uint8_t> bytes)
{
    WorldJoinSha256 hash;
    hash.append(bytes);
    return hash.finish();
}
inline WorldJoinFingerprint write_bytes(const std::filesystem::path &path, std::span<const std::uint8_t> bytes)
{
    if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)()))
        throw std::length_error("diagnostic output exceeds stream extent");
    const auto expected = bytes_fingerprint(bytes);
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!out)
        throw std::runtime_error("cannot create exclusive diagnostic payload");
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out)
        throw std::runtime_error("diagnostic payload write failed");
    out.close();
    if (!out)
        throw std::runtime_error("diagnostic payload close failed");
    if (fingerprint_world_join_file(path) != expected)
        throw std::runtime_error("saved diagnostic payload differs");
    return expected;
}
inline WorldJoinFingerprint write_text(const std::filesystem::path &path, const std::string &text)
{
    return write_bytes(path, {reinterpret_cast<const std::uint8_t *>(text.data()), text.size()});
}
inline void fresh_directory(const std::filesystem::path &path)
{
    if (path.empty() || !std::filesystem::create_directory(path))
        throw std::runtime_error("diagnostic bundle directory must be new with an existing parent");
}
inline void fingerprint_json(std::ostream &out, std::string_view name, const WorldJoinFingerprint &value)
{
    // All names are fixed literals owned by this writer; no arbitrary JSON text.
    out << "{\"file\":\"" << name << "\",\"sha256\":\"" << hex(value.sha256) << "\",\"bytes\":" << value.bytes << '}';
}
inline bool contains_path(const std::filesystem::path &parent, const std::filesystem::path &child)
{
    const auto a = std::filesystem::weakly_canonical(std::filesystem::absolute(parent));
    const auto b = std::filesystem::weakly_canonical(std::filesystem::absolute(child));
    auto left = a.begin(), right = b.begin();
    for (; left != a.end(); ++left, ++right)
    {
        if (right == b.end())
            return false;
        if (CompareStringOrdinal(left->c_str(), -1, right->c_str(), -1, TRUE) != CSTR_EQUAL)
            return false;
    }
    return true;
}
} // namespace front_bundle_detail
// NEW opt-in CLI preflight. The exclusive create in the writer remains binding.
inline void validate_diagnostic_bundle_path(const std::filesystem::path &directory,
                                            std::span<const std::filesystem::path> other_outputs)
{
    if (directory.empty() || std::filesystem::exists(directory))
        throw std::invalid_argument("diagnostic bundle directory already exists or is empty");
    for (const auto &other : other_outputs)
        if (!other.empty() && (front_bundle_detail::contains_path(directory, other) ||
                               front_bundle_detail::contains_path(other, directory)))
            throw std::invalid_argument("diagnostic bundle directory conflicts with another output");
}
inline std::array<std::uint8_t, 1536> encode_front_gamma(const SdrGammaRamp &gamma) noexcept
{
    std::array<std::uint8_t, 1536> bytes{};
    for (std::size_t i = 0; i < gamma.size(); ++i)
    {
        bytes[i * 2] = static_cast<std::uint8_t>(gamma[i] & 255u);
        bytes[i * 2 + 1] = static_cast<std::uint8_t>(gamma[i] >> 8);
    }
    return bytes;
}
// No renderer/source access here. All outputs bind ONE prior owned snapshot.
// An optional join borrows synchronously from the same application's retention.
inline void export_front_recipe_bundle(const std::filesystem::path &directory,
                                       const CapturedPolygonFrontRecipe &snapshot,
                                       const WorldJoinSidecarView *join = nullptr)
{
    using namespace front_bundle_detail;
    if (!snapshot.submission || !snapshot.submission->device || snapshot.submission->segments.empty() ||
        snapshot.front_image == 0 || snapshot.width == 0 || snapshot.height == 0)
        throw std::invalid_argument("front bundle requires a captured actual front");
    for (const auto &segment : snapshot.submission->segments)
        if (!segment.device_frame || !segment.device_frame->polygon_commands ||
            segment.device_frame->device != snapshot.submission->device)
            throw std::invalid_argument("front bundle has an incomplete or foreign segment");
    fresh_directory(directory);
    const auto recipe = write_bound_displayed_recipe(directory / "recipe.bin", snapshot.submission);
    const auto gamma = write_bytes(directory / "gamma.u16le", encode_front_gamma(snapshot.gamma));
    std::optional<WorldJoinFingerprint> sidecar;
    if (join)
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        const bool complete = write_world_join_sidecar(stream, recipe, snapshot.submission, *join);
        sidecar = write_text(directory / "join.json", stream.str());
        if (!complete)
            throw std::runtime_error("front bundle world join is incomplete for the captured identity");
    }
    std::ostringstream record;
    record.imbue(std::locale::classic());
    record << "{\"schema\":\"sgn.front-recipe-bundle.v1\",\"complete\":true,\"role\":\"front\",\"recipe\":";
    fingerprint_json(record, "recipe.bin", recipe);
    record << ",\"gamma\":";
    fingerprint_json(record, "gamma.u16le", gamma);
    record << ",\"gamma_encoding\":\"u16le-R256-G256-B256\",\"source_tick\":" << snapshot.submission->source_tick
           << ",\"width\":" << snapshot.width << ",\"height\":" << snapshot.height
           << ",\"front_image_tag\":{\"value\":" << snapshot.front_image
           << ",\"scope\":\"process-local-nonportable\"},\"resource_epochs\":[";
    bool first = true;
    for (const auto &segment : snapshot.submission->segments)
    {
        if (!first)
            record << ',';
        first = false;
        record << segment.device_frame->polygon_commands->resource_epoch;
    }
    record << "],\"world_join\":";
    if (sidecar)
        fingerprint_json(record, "join.json", *sidecar);
    else
        record << "null";
    record << ",\"gpu_pixels_or_portable_replay_proven\":false}\n";
    if (!record)
        throw std::runtime_error("front bundle completion record formatting failed");
    // Last file. Partial/truncated records or any missing/hash-mismatched payload
    // are incomplete; preserve evidence and never report atomic multi-file I/O.
    (void)write_text(directory / "front.json", record.str());
}
} // namespace sgn::diagnostics
