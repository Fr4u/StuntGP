#include "sgn/source_material_files.hpp"
#include <limits>

namespace sgn::retail
{
std::string SourceMaterialFiles::source_path(const MaterialResourcePath &path) const
{
    const SourceGraphicsRoot *root = nullptr;
    switch (path.root)
    {
    case MaterialResourceRoot::graphics24:
        root = &roots_.normal;
        break;
    case MaterialResourceRoot::graphics24_max:
        root = &roots_.maximum;
        break;
    default:
        throw FormatError("unknown source material root selector");
    }
    // Source sprintf%s%s: retain spelling/separators; each argument ends at NUL.
    // 44B050 performs the actual source split later, inside the existing reader.
    return std::string(root->text()) + path.relative_name.substr(0, path.relative_name.find('\0'));
}
std::uint32_t SourceMaterialFiles::source_file_size(const MaterialResourcePath &path)
{
    return source_boot_file_size_44b3b0(config_, files_, source_path(path));
}
MaterialFileRead SourceMaterialFiles::read_source_file(const MaterialResourcePath &path, std::span<std::uint8_t> bytes)
{
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw FormatError("source material destination exceeds positive signed read domain");
    std::size_t initialized = 0;
    const auto result = source_boot_file_read_44b260(config_, files_, source_path(path),
                                                     static_cast<std::uint32_t>(bytes.size()), bytes, initialized);
    return {result, initialized}; // source EAX and actual written prefix are distinct
}
} // namespace sgn::retail
