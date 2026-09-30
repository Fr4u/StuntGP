#include "sgn/retail_material_filesystem.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <string_view>

namespace sgn::retail
{
namespace
{
struct Route
{
    std::string_view directory, archive;
};
#include "retail_material_filesystem_routes.inc"
std::uint32_t bounded_size(std::size_t size)
{
    if (size > std::numeric_limits<std::int32_t>::max())
        throw FormatError("material file extent exceeds supported positive source size");
    return static_cast<std::uint32_t>(size);
}
std::uint32_t stream_size(std::ifstream &file)
{
    const auto end = file.tellg();
    if (end < 0 || end > std::numeric_limits<std::int32_t>::max())
        throw FormatError("invalid loose material file extent");
    return static_cast<std::uint32_t>(end);
}
} // namespace
RetailMaterialFileSystem::RetailMaterialFileSystem(const AssetStore &assets, std::filesystem::path root)
    : assets_(assets), root_(std::filesystem::absolute(std::move(root)))
{
}
std::string RetailMaterialFileSystem::key(const MaterialResourcePath &path) const
{
    std::string value;
    switch (path.root)
    {
    case MaterialResourceRoot::graphics24:
        value = "graphics24\\";
        break;
    case MaterialResourceRoot::graphics24_max:
        value = "graphics24max\\";
        break;
    default:
        throw FormatError("unknown source material graphics root");
    }
    if (path.relative_name.empty())
        throw FormatError("empty source material name");
    // These are explicit host-domain checks, before filesystem access/writes.
    // Original44B050 uses fixed 64-byte output banks and signed byte offsets.
    for (unsigned char c : path.relative_name)
        if (c < 32 || c >= 127 || c == ':' || c == '/')
            throw FormatError("unsupported source material path spelling");
    value += path.relative_name;
    if (value.find("\\\\\\") != std::string::npos)
        throw FormatError("unsupported repeated source path separator");
    value = canonical(std::move(value));
    if (value.size() >= 64 || value.back() == '\\')
        throw FormatError("source material path exceeds supported bank");
    for (std::size_t at = 0; at < value.size();)
    {
        auto end = value.find('\\', at);
        if (end == std::string::npos)
            end = value.size();
        const auto part = std::string_view(value).substr(at, end - at);
        if (part == "." || part == "..")
            throw FormatError("source material path escapes supplied root");
        at = end + 1;
    }
    return value;
}
const Archive *RetailMaterialFileSystem::routed_archive(std::string_view name, bool exact) const
{
    const auto directory = name.substr(0, name.rfind('\\'));
    const Route *selected = nullptr;
    for (const auto &route : routes)
    {
        if (exact)
        {
            if (route.directory == directory)
                return assets_.archive(route.archive);
            continue;
        }
        // 44B380 compares until EITHER string ends. This is deliberately not
        // a path-component-aware prefix test.44B2C2 prefers the first exact.
        const auto n = std::min(directory.size(), route.directory.size());
        if (directory.substr(0, n) != route.directory.substr(0, n))
            continue;
        if (directory.size() == route.directory.size())
        {
            selected = &route;
            break;
        }
        if (!selected || route.directory.size() > selected->directory.size())
            selected = &route;
    }
    return selected ? assets_.archive(selected->archive) : nullptr;
}
std::optional<std::span<const std::uint8_t>> RetailMaterialFileSystem::archived(const Archive &archive,
                                                                                std::string_view name) const
{
    // NEW HOST (R30): the archive's sorted canonical-name index instead of canonicalising every entry
    // name on every probe (a material load probes the same archive 4-6 times). Names are unique per
    // archive, so this is the entry the former first-match scan returned.
    if (const auto *entry = archive.find(name))
        return archive.data(*entry);
    return {};
}
std::filesystem::path RetailMaterialFileSystem::loose_path(std::string_view name) const
{
    auto path = root_;
    std::size_t at = 0;
    while (at < name.size())
    {
        auto end = name.find('\\', at);
        if (end == name.npos)
            end = name.size();
        path /= std::string(name.substr(at, end - at));
        at = end + 1;
    }
    return path;
}
std::uint32_t RetailMaterialFileSystem::source_file_size(const MaterialResourcePath &path)
{
    const auto name = key(path);
    if (const auto *archive = routed_archive(name, true))
    {
        const auto bytes = archived(*archive, name);
        return bytes ? bounded_size(bytes->size()) : 0; // valid archive + miss =0, no disk retry
    }
    std::ifstream file(loose_path(name), std::ios::binary | std::ios::ate);
    return file ? stream_size(file) : 0;
}
MaterialFileRead RetailMaterialFileSystem::read_source_file(const MaterialResourcePath &path,
                                                            std::span<std::uint8_t> output)
{
    const auto name = key(path);
    const auto requested = bounded_size(output.size());
    if (const auto *archive = routed_archive(name, false))
    {
        const auto bytes = archived(*archive, name);
        if (bytes)
        {
            const auto total = bounded_size(bytes->size());
            const auto count = std::min(output.size(), bytes->size());
            std::copy_n(bytes->begin(), count, output.begin());
            return {total, count}; // 44B358 EAX=full entry size
        }
    }
    std::ifstream file(loose_path(name), std::ios::binary | std::ios::ate);
    if (!file)
        return {};
    const auto total = stream_size(file);
    if (requested == 0 && total != 0)
        throw FormatError("zero-size loose read would invoke original 2MB fallback outside destination span");
    const auto count = std::min(requested, total);
    file.seekg(0);
    if (count && !file.read(reinterpret_cast<char *>(output.data()), count))
        throw FormatError("incomplete loose material read");
    return {count, count}; // 44AFCC/44AFDC EAX=actual fread byte count
}
} // namespace sgn::retail
