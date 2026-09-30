#pragma once
#include "sgn/source_allocation_bank.hpp"
#include <optional>
#include <variant>
namespace sgn::retail::contact
{
// A normalized PMD offset is not an x86 address. Preserve a computed out-of-
// range offset until its reached read, so publication cannot wrap to file0.
struct SourceContactAddress
{
    SourceAllocationRef allocation;
    std::uint64_t offset{};
    bool operator==(const SourceContactAddress &) const = default;
};
// One pointer owner, with disjoint native-index/source-allocation domains.
// Existing preview callers retain their optional-index API. They cannot read
// a source pointer as an invented polygon/normal index.
class ContactGeometryPointer
{
    std::variant<std::optional<std::uint32_t>, SourceContactAddress> value_;
    const std::optional<std::uint32_t> &native() const
    {
        if (value_.index() != 0)
            throw SourceAllocationError(0x420c40, "source contact pointer requires actual allocation receiver");
        return std::get<0>(value_);
    }

  public:
    ContactGeometryPointer() = default;
    ContactGeometryPointer(std::optional<std::uint32_t> v) : value_(v) {}
    ContactGeometryPointer &operator=(std::optional<std::uint32_t> v)
    {
        value_ = v;
        return *this;
    }
    ContactGeometryPointer &operator=(std::uint32_t v)
    {
        value_ = std::optional<std::uint32_t>{v};
        return *this;
    }
    ContactGeometryPointer &operator=(std::nullopt_t)
    {
        reset();
        return *this;
    }
    void reset() noexcept
    {
        value_ = std::optional<std::uint32_t>{};
    }
    bool has_value() const noexcept
    {
        if (value_.index() == 0)
            return std::get<0>(value_).has_value();
        const auto &p = std::get<1>(value_);
        return bool(p.allocation) || p.offset != 0;
    }
    explicit operator bool() const noexcept
    {
        return has_value();
    }
    std::uint32_t value() const
    {
        return native().value();
    }
    std::uint32_t value_or(std::uint32_t v) const
    {
        return native().value_or(v);
    }
    std::uint32_t operator*() const
    {
        return native().value();
    }
    operator std::optional<std::uint32_t>() const
    {
        return native();
    }
    void bind_source(SourceContactAddress address)
    {
        value_ = address;
    }
    SourceContactAddress source(std::uint32_t site) const
    {
        if (value_.index() != 1)
            throw SourceAllocationError(site, "contact global has no source pointer publication");
        return std::get<1>(value_);
    }
};
} // namespace sgn::retail::contact
