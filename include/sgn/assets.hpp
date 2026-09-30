#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sgn
{
using Bytes = std::vector<std::uint8_t>;
struct FormatError : std::runtime_error
{
    using runtime_error::runtime_error;
};
Bytes read_file(const std::filesystem::path &path);
std::string canonical(std::string value);
std::uint16_t name_hash(std::string_view name);
struct Vec3
{
    float x{}, y{}, z{};
};
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, float b);
float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
Vec3 normal(Vec3 a);
struct ArchiveEntry
{
    std::string name;
    std::size_t offset{}, size{};
};
class Archive
{
    Bytes bytes_;
    std::vector<ArchiveEntry> entries_;
    // NEW HOST (R30): (canonical name, entry index), sorted by name. Names are unique per archive (the
    // constructor rejects a duplicate), so a lookup finds the one entry a linear scan would find.
    std::vector<std::pair<std::string, std::size_t>> by_name_;

  public:
    explicit Archive(Bytes bytes);
    const auto &entries() const
    {
        return entries_;
    }
    std::span<const std::uint8_t> data(const ArchiveEntry &entry) const;
    // The entry whose canonical() name equals `canonical_name` (already canonical), or null.
    const ArchiveEntry *find(std::string_view canonical_name) const;
};
struct Image
{
    std::uint32_t width{}, height{};
    Bytes rgba;
};
Image decode_tm(std::span<const std::uint8_t> bytes);
// NEW HOST (R30): the 16-bit A1R5G5B5 words decode_tm decodes before its UNORM8 expansion (the same
// stream, checks and errors). The loaders' exact inverse of that expansion gives these words back, so
// they can take them directly.
struct TmWords
{
    std::uint32_t width{}, height{};
    std::vector<std::uint16_t> words;
};
TmWords decode_tm_words(std::span<const std::uint8_t> bytes);
struct Vertex
{
    Vec3 position, normal;
    float u{}, v{};
    std::uint32_t color{0xffffffff};
};
static_assert(sizeof(Vertex) == 36);
struct Surface
{
    std::uint32_t first{}, count{}, texture{};
    bool translucent{}, two_sided{};
    std::uint32_t part{};
};
struct Model
{
    std::vector<Vertex> vertices;
    std::vector<Surface> surfaces;
    std::vector<std::string> textures;
    Vec3 minimum{}, maximum{};
    std::uint32_t kind{}, lod_count{}, parts{};
};
Model decode_pmd(std::span<const std::uint8_t> bytes, unsigned lod = 0);
// Authored records retained for the recovered animation system. Raw geometry
// includes disabled children, because a later retail tick may enable them.
struct PmdPart
{
    std::uint16_t flags{};
    std::uint32_t transform_index{};
    Vec3 pivot{}, position{}, scale{}, forward{}, up{};
};
struct PmdRig
{
    Model geometry;
    std::vector<PmdPart> parts;
};
PmdRig decode_pmd_rig(std::span<const std::uint8_t> bytes, unsigned lod = 0);
class AssetStore
{
    struct Location
    {
        std::size_t archive{}, entry{};
    };
    std::vector<Archive> archives_;
    std::map<std::string, std::size_t> archive_names_; // metadata only; payloads stay in archives_
    std::map<std::string, Location> index_;
    std::map<std::string, std::string> track_sets_;
    std::filesystem::path root_;
    void bind_textures(Model &, const std::string &name, std::string_view track_set) const;

  public:
    explicit AssetStore(std::filesystem::path root);
    const Archive *archive(std::string_view filename) const;
    std::span<const std::uint8_t> resource(std::string_view name) const;
    Image texture(const std::string &pmd_name) const;
    Model model(const std::string &name, unsigned lod = 0, std::string_view track_set = {}) const;
    PmdRig rig(const std::string &name, unsigned lod = 0, std::string_view track_set = {}) const;
    std::vector<std::string> models() const;
    std::vector<std::string> resources(std::string_view prefix) const;
    std::size_t image_count() const;
    std::size_t entry_count() const
    {
        return index_.size();
    }
    std::size_t archive_count() const
    {
        return archives_.size();
    }
    std::uint64_t validate_images() const;
};
} // namespace sgn
