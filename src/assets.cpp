#include "sgn/assets.hpp"
#include "sgn/format_require.hpp"
#include "sgn/host_load_timeline.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <future>
#include <limits>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

namespace sgn
{
std::vector<std::string> AssetStore::resources(std::string_view prefix) const
{
    const auto normalized = canonical(std::string(prefix));
    std::vector<std::string> names;
    for (const auto &[name, location] : index_)
    {
        (void)location;
        if (name.starts_with(normalized))
            names.push_back(name);
    }
    return names;
}
namespace
{
using sgn::format_check::require;
class Reader
{
    std::span<const std::uint8_t> b_;

  public:
    explicit Reader(std::span<const std::uint8_t> b) : b_(b) {}
    std::span<const std::uint8_t> view(std::size_t at, std::size_t n) const
    {
        require(at <= b_.size() && n <= b_.size() - at, "truncated or out-of-range asset");
        return b_.subspan(at, n);
    }
    std::uint16_t u16(std::size_t at) const
    {
        auto s = view(at, 2);
        return static_cast<std::uint16_t>(s[0] | (s[1] << 8));
    }
    std::uint32_t u32(std::size_t at) const
    {
        auto s = view(at, 4);
        return s[0] | (std::uint32_t(s[1]) << 8) | (std::uint32_t(s[2]) << 16) | (std::uint32_t(s[3]) << 24);
    }
    float f32(std::size_t at) const
    {
        auto v = std::bit_cast<float>(u32(at));
        require(std::isfinite(v), "non-finite asset number");
        return v;
    }
    Vec3 vec(std::size_t at) const
    {
        return {f32(at), f32(at + 4), f32(at + 8)};
    }
    std::string text(std::size_t at, std::size_t limit) const
    {
        auto s = view(at, limit);
        auto end = std::find(s.begin(), s.end(), 0);
        require(end != s.end(), "unterminated asset name");
        return {s.begin(), end};
    }
};
struct Section
{
    std::size_t at{}, size{};
};
} // namespace
Bytes read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw FormatError("cannot open " + path.string());
    auto size = in.tellg();
    require(size >= 0 && size <= 512 * 1024 * 1024, "invalid asset file size");
    Bytes bytes(static_cast<std::size_t>(size));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char *>(bytes.data()), size))
        throw FormatError("incomplete read: " + path.string());
    return bytes;
}
std::string canonical(std::string value)
{
    for (char &c : value)
    {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
        if (c == '/')
            c = '\\';
    }
    value.erase(std::unique(value.begin(), value.end(), [](char a, char b) { return a == '\\' && b == '\\'; }),
                value.end());
    return value;
}
// Retail archive lookup, confirmed against the EXE before import qualification.
std::uint16_t name_hash(std::string_view name)
{
    unsigned h = 0;
    for (unsigned char c : name)
    {
        if (c >= 'A' && c <= 'Z')
            c += 32;
        h = (((h & 511) << 1) | (h >> 9)) + c;
        h &= 1023;
    }
    return static_cast<std::uint16_t>(h);
}
const Archive *AssetStore::archive(std::string_view filename) const
{
    const auto found = archive_names_.find(canonical(std::string(filename)));
    return found == archive_names_.end() ? nullptr : &archives_.at(found->second);
}
std::span<const std::uint8_t> AssetStore::resource(std::string_view name) const
{
    auto found = index_.find(canonical(std::string(name)));
    if (found == index_.end())
        throw FormatError("missing original resource: " + std::string(name));
    const auto &location = found->second;
    const auto &archive = archives_.at(location.archive);
    return archive.data(archive.entries().at(location.entry));
}
Vec3 operator+(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 operator-(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 operator*(Vec3 a, float b)
{
    return {a.x * b, a.y * b, a.z * b};
}
float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
bool finite(Vec3 a)
{
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
Vec3 normal(Vec3 a)
{
    require(finite(a), "non-finite calculated PMD normal");
    float n = std::sqrt(dot(a, a));
    require(std::isfinite(n), "PMD normal length overflow");
    return n > 1e-20f ? a * (1 / n) : Vec3{};
}

Archive::Archive(Bytes bytes) : bytes_(std::move(bytes))
{
    Reader r(bytes_);
    require(r.u32(0) == 0x1a524944, "expected DIR archive");
    require(r.u32(4) == bytes_.size(), "DIR file-size mismatch");
    auto table = r.u32(8);
    require(table >= 12, "DIR data boundary below header");
    r.view(table, 4100);
    std::set<std::uint32_t> nodes;
    std::set<std::string> names;
    for (unsigned bucket = 0; bucket < 1024; ++bucket)
    {
        auto relative = r.u32(table + 4 + bucket * 4);
        while (relative)
        {
            require(relative >= 4100 && nodes.insert(relative).second, "DIR cycle/shared node/invalid node offset");
            std::size_t at = std::size_t(table) + relative;
            r.view(at, 13);
            auto offset = r.u32(at + 4), size = r.u32(at + 8);
            require(offset >= 12 && offset <= table && size <= table - offset, "DIR entry exceeds data area");
            auto name = r.text(at + 12, bytes_.size() - at - 12);
            require(!name.empty() && name_hash(name) == bucket, "DIR name/hash mismatch");
            require(names.insert(canonical(name)).second, "DIR duplicate name");
            entries_.push_back({std::move(name), offset, size});
            relative = r.u32(at);
        }
    }
    by_name_.reserve(entries_.size());
    for (std::size_t i = 0; i < entries_.size(); ++i)
        by_name_.emplace_back(canonical(entries_[i].name), i);
    std::sort(by_name_.begin(), by_name_.end());
}
std::span<const std::uint8_t> Archive::data(const ArchiveEntry &e) const
{
    return Reader(bytes_).view(e.offset, e.size);
}
const ArchiveEntry *Archive::find(std::string_view canonical_name) const
{
    const auto found = std::lower_bound(by_name_.begin(), by_name_.end(), canonical_name,
                                        [](const auto &entry, std::string_view name) { return entry.first < name; });
    return found != by_name_.end() && found->first == canonical_name ? &entries_[found->second] : nullptr;
}

// Direct reconstruction of 0x004234E0. Safety bounds are new host policy.
// R30: split into the word decode (decode_tm_words) and the unchanged UNORM8 expansion.
TmWords decode_tm_words(std::span<const std::uint8_t> bytes)
{
    Reader r(bytes);
    require(r.u32(0) == 0x1a214d54 && r.u16(4) == 3, "unsupported TM header");
    TmWords image{r.u16(6), r.u16(8), {}};
    auto count = std::size_t(image.width) * image.height;
    require(count > 0 && count <= 64 * 1024 * 1024, "TM dimensions outside limits");
    std::vector<std::uint16_t> pixels;
    pixels.reserve(count);
    pixels.push_back(r.u16(10));
    std::size_t at = 12;
    for (;;)
    {
        auto word = r.u16(at);
        at += 2;
        if (word & 0x8000)
        {
            require(pixels.size() < count, "TM literal overflow");
            pixels.push_back(word);
        }
        else if (word & 0x4000)
        {
            const unsigned distance = (word & 0x7ff) + 1, length = ((word >> 11) & 7) + 2;
            require(distance <= pixels.size() && length <= count - pixels.size(), "TM invalid LZ copy");
            for (unsigned i = 0; i < length; ++i)
                pixels.push_back(pixels[pixels.size() - distance]);
        }
        else
        {
            const auto n = word & 0x3fff;
            if (n == 0)
                break;
            require(std::size_t(n) <= count - pixels.size(), "TM zero-run overflow");
            pixels.insert(pixels.end(), n, 0);
        }
    }
    require(pixels.size() == count, "TM incomplete pixel stream");
    image.words = std::move(pixels);
    return image;
}
Image decode_tm(std::span<const std::uint8_t> bytes)
{
    const auto decoded = decode_tm_words(bytes);
    Image image{decoded.width, decoded.height, {}};
    image.rgba.reserve(decoded.words.size() * 4);
    // Modern texture upload: exact integer 5-bit values expanded to UNORM8.
    for (auto v : decoded.words)
    {
        for (unsigned shift : {10u, 5u, 0u})
            image.rgba.push_back(static_cast<std::uint8_t>((((v >> shift) & 31) * 255 + 15) / 31));
        image.rgba.push_back((v & 0x8000) ? 255 : 0);
    }
    return image;
}

namespace
{
Model decode_pmd_impl(std::span<const std::uint8_t> bytes, unsigned lod, std::vector<PmdPart> *rig)
{
    Reader r(bytes);
    require(r.text(0, 24) == "PMD V1.83", "only pinned PC PMD V1.83 is supported");
    r.view(0, 328);
    Model result;
    result.kind = r.u32(0x18);
    require(result.kind <= 2, "unknown PMD kind");
    std::array<Section, 11> sections{};
    constexpr std::array<unsigned, 11> strides = {16, 8, 16, 16, 2, 0, 4, 16, 4, 80, 64};
    std::vector<std::pair<std::size_t, std::size_t>> occupied;
    for (unsigned i = 0; i < 11; ++i)
    {
        auto &s = sections[i];
        s = {328 + std::size_t(r.u32(0x20 + i * 4)), r.u32(0xb4 + i * 4)};
        r.view(s.at, s.size);
        require(!strides[i] || s.size % strides[i] == 0, "PMD misaligned section");
        if (s.size)
            occupied.emplace_back(s.at, s.at + s.size);
    }
    std::sort(occupied.begin(), occupied.end());
    for (std::size_t i = 1; i < occupied.size(); ++i)
        require(occupied[i - 1].second <= occupied[i].first, "PMD overlapping sections");
    // 0x42EBF0: descriptor at header+0x4c, not the header itself.
    auto descriptor = 328 + std::size_t(r.u32(0x4c));
    r.view(descriptor, 128);
    result.parts = r.u32(descriptor + 0x24);
    result.lod_count = r.u32(descriptor + 0x58) + 1;
    require(result.parts > 0 && result.lod_count >= 1 && result.lod_count <= 4 && lod < result.lod_count,
            "PMD invalid LOD");
    auto first_part = r.u32(descriptor + 0x48 + lod * 4);
    require(std::uint64_t(first_part) + result.parts <= sections[10].size / 64, "PMD LOD parts overflow");
    bool terminated = sections[6].size == 0;
    for (std::size_t i = 0; i < sections[6].size; i += 4)
    {
        auto offset = r.u32(sections[6].at + i);
        if (!offset)
        {
            terminated = true;
            break;
        }
        auto at = 328 + std::size_t(offset);
        auto end = sections[5].at + sections[5].size;
        require(at >= sections[5].at && at < end, "PMD texture-name pointer outside section");
        result.textures.push_back(r.text(at, end - at));
    }
    require(terminated, "PMD texture table missing terminator");
    std::map<std::tuple<unsigned, bool, bool, unsigned>, std::vector<Vertex>> batches;
    for (unsigned p = 0; p < result.parts; ++p)
    {
        auto part = sections[10].at + (first_part + p) * 64;
        // 0x42AC70 base dispatch; 0x42ADD0 visits children only when flag==1.
        if (!rig && p && r.u16(part) != 1)
            continue;
        auto transform = r.u32(part + 8);
        require(transform < sections[9].size / 80, "PMD bad transform index");
        auto tr = sections[9].at + transform * 80;
        Vec3 pos = r.vec(tr), scale = r.vec(tr + 16), forward = r.vec(tr + 32), up = r.vec(tr + 48),
             pivot = r.vec(part + 16);
        if (rig)
            rig->push_back({r.u16(part), transform, pivot, pos, scale, forward, up});
        Vec3 right{1, 0, 0};
        if (!rig && r.u16(part) != 0)
        {
            // 0x44FF90, with identity selection for flag==0 in 0x450150.
            if (std::abs(dot(up, forward)) > 0.99999f)
            {
                up.z += 1;
                up = normal(up);
            }
            up = normal(up - forward * dot(up, forward));
            right = cross(up, forward);
        }
        else
        {
            forward = {0, 0, 1};
            up = {0, 1, 0};
        }
        auto pc = r.u16(part + 0x24), cc = r.u16(part + 0x26);
        auto vc = r.u32(part + 0x28);
        auto cs = r.u32(part + 0x2c), vs = r.u32(part + 0x30), ps = r.u32(part + 0x34);
        require(std::uint64_t(cs) + cc <= sections[2].size / 16 && std::uint64_t(vs) + vc <= sections[3].size / 16 &&
                    std::uint64_t(ps) + pc <= sections[1].size / 8,
                "PMD part ranges exceed sections");
        for (unsigned j = 0; j < pc; ++j)
        {
            auto polygon = sections[1].at + (ps + j) * 8;
            auto packed = r.u32(polygon);
            unsigned n = packed & 255, material = (packed >> 8) & 255, start = r.u16(polygon + 6);
            require(n >= 3 && n <= 64 && material < result.textures.size() &&
                        std::uint64_t(start) + n <= sections[4].size / 2,
                    "PMD invalid polygon");
            std::vector<Vertex> face;
            face.reserve(n);
            for (unsigned c = 0; c < n; ++c)
            {
                auto corner_index = r.u16(sections[4].at + (start + c) * 2);
                require(corner_index < cc, "PMD corner outside part");
                auto corner = sections[2].at + (cs + corner_index) * 16;
                auto vertex_index = r.u16(corner + 8);
                require(vertex_index < vc, "PMD vertex outside part");
                auto v = r.vec(sections[3].at + (vs + vertex_index) * 16);
                // Static authored part transform from 0x44FBD0. No vehicle state is synthesized.
                if (!rig && result.parts > 1)
                {
                    v = {v.x * scale.x - pivot.x, v.y * scale.y - pivot.y, v.z * scale.z - pivot.z};
                    v = right * v.x + up * v.y + forward * v.z + pivot + pos;
                }
                require(finite(v), "PMD transformed vertex overflow");
                auto u = r.f32(corner), vv = r.f32(corner + 4);
                face.push_back({v, {}, u, vv, r.u32(corner + 12)});
            }
            auto norm = normal(cross(face[1].position - face[0].position, face[2].position - face[0].position));
            auto &out = batches[{material, (packed & 0x40000000) != 0, (packed & 0x80000000) != 0, rig ? p : 0}];
            for (unsigned c = 1; c + 1 < n; ++c)
                for (auto idx : {0u, c, c + 1})
                {
                    auto v = face[idx];
                    v.normal = norm;
                    out.push_back(v);
                }
        }
    }
    for (auto &[key, vertices] : batches)
    {
        require(result.vertices.size() + vertices.size() < std::numeric_limits<std::uint32_t>::max(),
                "PMD too many vertices");
        // 00429B70/00429D50: bit31 bypasses backface rejection; bit30
        // routes to the translucent list. Keep the aggregate field order.
        result.surfaces.push_back({static_cast<std::uint32_t>(result.vertices.size()),
                                   static_cast<std::uint32_t>(vertices.size()), std::get<0>(key), std::get<1>(key),
                                   std::get<2>(key), std::get<3>(key)});
        result.vertices.insert(result.vertices.end(), vertices.begin(), vertices.end());
    }
    require(!result.vertices.empty(), "PMD has no visible triangles");
    result.minimum = result.maximum = result.vertices[0].position;
    for (auto v : result.vertices)
    {
        result.minimum = {std::min(result.minimum.x, v.position.x), std::min(result.minimum.y, v.position.y),
                          std::min(result.minimum.z, v.position.z)};
        result.maximum = {std::max(result.maximum.x, v.position.x), std::max(result.maximum.y, v.position.y),
                          std::max(result.maximum.z, v.position.z)};
    }
    require(finite(result.minimum + result.maximum) &&
                std::isfinite(dot(result.maximum - result.minimum, result.maximum - result.minimum)),
            "PMD bounds overflow");
    return result;
}
} // namespace
Model decode_pmd(std::span<const std::uint8_t> bytes, unsigned lod)
{
    return decode_pmd_impl(bytes, lod, nullptr);
}
PmdRig decode_pmd_rig(std::span<const std::uint8_t> bytes, unsigned lod)
{
    PmdRig rig;
    rig.geometry = decode_pmd_impl(bytes, lod, &rig.parts);
    return rig;
}

AssetStore::AssetStore(std::filesystem::path root) : root_(std::move(root))
{
    SGN_LOAD_STAGE("load.asset_store");
    std::vector<std::filesystem::path> paths;
    for (auto &e : std::filesystem::directory_iterator(root_ / "wads"))
        if (canonical(e.path().extension().string()) == ".wad")
            paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    // NEW HOST (R30): every WAD is read and its directory parsed on its own task (the reads overlap each
    // other and the parsing). The results are taken in the sorted path order below, so the archives, the
    // index and the first error thrown are those of the former sequential loop; a task's exception is
    // rethrown by get() exactly where that loop would have thrown it. Remaining tasks finish (their
    // futures wait on destruction) before an error leaves the constructor.
    std::vector<std::future<Archive>> pending;
    pending.reserve(paths.size());
    {
        SGN_LOAD_SCOPE("load.asset_store.read_start");
        for (const auto &path : paths)
            pending.push_back(std::async(std::launch::async, [path] { return Archive(read_file(path)); }));
    }
    archives_.reserve(paths.size());
    for (std::size_t p = 0; p < paths.size(); ++p)
    {
        const auto &path = paths[p];
        auto a = archives_.size();
        archives_.push_back(pending[p].get());
        auto &archive = archives_.back();
        require(archive_names_.emplace(canonical(path.filename().string()), a).second, "ambiguous archive filename");
        for (std::size_t e = 0; e < archive.entries().size(); ++e)
        {
            const auto &entry = archive.entries()[e];
            auto key = canonical(entry.name);
            auto [it, added] = index_.emplace(key, Location{a, e});
            if (!added)
            {
                auto &old = archives_[it->second.archive];
                auto previous = old.data(old.entries()[it->second.entry]), now = archive.data(entry);
                require(std::ranges::equal(previous, now), "ambiguous cross-archive resource name");
            }
        }
    }
    require(!archives_.empty(), "no retail WADs installed");
    auto script = index_.find("config\\lvscript.cfg");
    require(script != index_.end(), "missing original level script");
    auto &a = archives_[script->second.archive];
    auto data = a.data(a.entries()[script->second.entry]);
    std::string raw(data.begin(), data.end()), clean;
    bool comment = false;
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        if (!comment && i + 1 < raw.size() && raw[i] == '/' && raw[i + 1] == '*')
        {
            comment = true;
            ++i;
            continue;
        }
        if (comment && i + 1 < raw.size() && raw[i] == '*' && raw[i + 1] == '/')
        {
            comment = false;
            ++i;
            continue;
        }
        if (!comment)
            clean += raw[i];
    }
    require(!comment, "unterminated level-script comment");
    // Metadata-only reader of L_TRACK/TRACKTEXSET. Does not execute level scripts.
    std::istringstream lines(clean);
    std::string line, mesh, set;
    while (std::getline(lines, line))
    {
        std::replace(line.begin(), line.end(), '=', ' ');
        std::istringstream tokens(line);
        std::string key, value;
        tokens >> key >> value;
        key = canonical(key);
        if (key == "l_track")
            mesh = canonical(value);
        else if (key == "tracktexset")
            set = canonical(value);
        else if (key == "-end")
        {
            if (!mesh.empty() && !set.empty())
            {
                auto [it, added] = track_sets_.emplace(mesh, set);
                require(added || it->second == set, "ambiguous script track set");
            }
            mesh.clear();
            set.clear();
        }
    }
}
Image AssetStore::texture(const std::string &name) const
{
    auto key = canonical(name);
    if (!key.ends_with(".pc"))
        key += ".pc";
    // Raw PMD names exclude the graphics24 root; WAD keys retain it.
    auto it = index_.find(key);
    if (it == index_.end())
        it = index_.find("graphics24\\" + key);
    if (it == index_.end())
        throw FormatError("missing texture: " + name);
    const auto &a = archives_[it->second.archive];
    return decode_tm(a.data(a.entries()[it->second.entry]));
}
Model AssetStore::model(const std::string &name, unsigned lod, std::string_view track_set) const
{
    require(name.find_first_of("/\\:") == std::string::npos && name.find("..") == std::string::npos,
            "invalid model name");
    auto result = decode_pmd(read_file(root_ / "meshdata" / (name + ".PMD")), lod);
    bind_textures(result, name, track_set);
    return result;
}
PmdRig AssetStore::rig(const std::string &name, unsigned lod, std::string_view track_set) const
{
    require(name.find_first_of("/\\:") == std::string::npos && name.find("..") == std::string::npos,
            "invalid model name");
    auto result = decode_pmd_rig(read_file(root_ / "meshdata" / (name + ".PMD")), lod);
    bind_textures(result.geometry, name, track_set);
    return result;
}
void AssetStore::bind_textures(Model &result, const std::string &name, std::string_view track_set) const
{
    for (auto &texture : result.textures)
    {
        auto file = texture.substr(texture.find_last_of("/\\") + 1);
        // 0x42BB70 + original TRACKTEXSET read at 0x42CC20.
        if (file.starts_with("Trk_"))
        {
            if (!track_set.empty())
            {
                require(track_set.size() == 10 && track_set.starts_with("trackset0") && track_set.back() >= '0' &&
                            track_set.back() <= '6',
                        "invalid explicit track set");
                texture = std::string(track_set) + "\\" + file;
                continue;
            }
            auto set = track_sets_.find(canonical(name));
            if (set == track_sets_.end())
                throw FormatError("no retail script texture-set binding for " + name);
            texture = set->second + "\\" + file;
        }
    }
}
std::vector<std::string> AssetStore::models() const
{
    std::vector<std::string> out;
    for (const auto &e : std::filesystem::directory_iterator(root_ / "meshdata"))
        if (canonical(e.path().extension().string()) == ".pmd")
            out.push_back(e.path().stem().string());
    std::sort(out.begin(), out.end());
    return out;
}
std::size_t AssetStore::image_count() const
{
    std::size_t count = 0;
    for (const auto &[key, location] : index_)
    {
        (void)location;
        if (key.ends_with(".pc"))
            ++count;
    }
    return count;
}
std::uint64_t AssetStore::validate_images() const
{
    std::uint64_t count = 0;
    for (const auto &a : archives_)
        for (const auto &e : a.entries())
        {
            auto b = a.data(e);
            if (b.size() >= 4 && Reader(b).u32(0) == 0x1a214d54)
            {
                auto image = decode_tm(b);
                count += std::uint64_t(image.width) * image.height;
            }
        }
    return count;
}
} // namespace sgn
