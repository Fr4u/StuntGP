#include "sgn/borrowed_material_name.hpp"
#include <limits>

namespace sgn::retail
{
namespace
{
SourcePmdByteView section(const SourcePmdByteView &file, std::uint32_t header)
{
    const auto relative = file.field<std::uint32_t>(header).value();
    if (relative > std::numeric_limits<std::uint32_t>::max() - 0x148u)
        throw FormatError("PMD material section offset overflow");
    return file.subview(relative + 0x148u, file.field<std::uint32_t>(header + 0x94).value());
}
} // namespace
std::uint8_t BorrowedMaterialName::byte(std::size_t index) const
{
    if (const auto p = std::get_if<const std::string *>(&value_))
        return index < (*p)->size() ? static_cast<std::uint8_t>((**p)[index]) : std::uint8_t{0};
    if (const auto p = std::get_if<SourcePmdByteView>(&value_))
    {
        if (index >= p->size())
            throw FormatError("PMD material name has no terminator within its owner");
        return p->field<std::uint8_t>(static_cast<std::uint32_t>(index)).value();
    }
    throw FormatError("dereferenced null material name");
}
std::string BorrowedMaterialName::text_snapshot() const
{
    std::string out;
    for (std::size_t i = 0;; ++i)
    {
        const auto c = byte(i);
        if (c == 0)
            return out;
        out.push_back(static_cast<char>(c));
    }
}
std::int32_t compare_material_names(const BorrowedMaterialName &a, const BorrowedMaterialName &b)
{
    for (std::size_t i = 0;; ++i)
    {
        const auto x = a.byte(i), y = b.byte(i);
        if (x != y)
            return x < y ? -1 : 1;
        if (x == 0)
            return 0;
    }
}
SourcePmdMaterialNames::SourcePmdMaterialNames(const SourcePmdModelView &model) : file_(model.bytes())
{
    const auto pointer = model.descriptor().field<std::uint32_t>(0x60).value();
    const auto storage = section(file_, 0x38);
    const auto start = storage.location().offset;
    if (pointer < start || pointer - start > storage.size() || (pointer - start) % 4 != 0)
        throw FormatError("PMD material name table is outside its relocated owner");
    table_ = storage.subview(pointer - start, storage.size() - (pointer - start));
}
BorrowedMaterialName SourcePmdMaterialNames::at(std::uint32_t index) const
{
    if (std::uint64_t(index) * 4 + 4 > table_.size())
        throw FormatError("PMD material name table has no reached terminal DWORD");
    const auto pointer = table_.field<std::uint32_t>(index * 4).value();
    if (pointer == 0)
        return {};
    const auto strings = section(file_, 0x34);
    const auto start = strings.location().offset;
    if (pointer < start || pointer - start >= strings.size())
        throw FormatError("PMD material name points outside its string owner");
    return BorrowedMaterialName(strings.subview(pointer - start, strings.size() - (pointer - start)));
}
} // namespace sgn::retail
