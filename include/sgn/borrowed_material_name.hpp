#pragma once
#include "sgn/source_pmd_model.hpp"
#include <variant>

namespace sgn::retail
{
// Host lifetime projection of a source char*. No string payload is owned here.
// PMD references capture the pointed-to bytes, not a table index to re-resolve.
class BorrowedMaterialName
{
    std::variant<std::monostate, const std::string *, SourcePmdByteView> value_;

  public:
    BorrowedMaterialName() = default;
    BorrowedMaterialName(const std::string *value)
    {
        if (value)
            value_ = value;
    }
    BorrowedMaterialName(const std::string &value) : value_(&value) {}
    BorrowedMaterialName(std::string &&) = delete;
    explicit BorrowedMaterialName(SourcePmdByteView bytes) : value_(std::move(bytes)) {}
    explicit operator bool() const noexcept
    {
        return !std::holds_alternative<std::monostate>(value_);
    }
    std::uint8_t byte(std::size_t index) const;
    std::string text_snapshot() const; // synchronous file/path transport only
};
using BorrowedMaterialNames = std::span<const BorrowedMaterialName>;
std::int32_t compare_material_names(const BorrowedMaterialName &, const BorrowedMaterialName &);

// Capture descriptor+60 once at42C559; read each next DWORD after42BF60.
// A zero DWORD returns a null reference. No eager name-count/string scan.
class SourcePmdMaterialNames
{
    SourcePmdByteView file_, table_;

  public:
    explicit SourcePmdMaterialNames(const SourcePmdModelView &);
    BorrowedMaterialName at(std::uint32_t index) const;
};
} // namespace sgn::retail
