#pragma once
#include "sgn/source_allocation_bank.hpp"
#include <cstdint>
#include <memory>

namespace sgn::retail
{
// NEW typed operand representation for the fixed, statically translated
// 409B60/420FE0 call graph. Address identities and opaque copies are never numbers.
// There is no runtime decoder, guest executable, geometry copy or Session.
struct SourceContactDifference;
struct SourceContactQueryWord
{
    enum class Kind : std::uint8_t
    {
        unknown,
        number,
        partial,
        opaque,
        stack,
        global,
        pmd,
        vehicle,
        difference,
        object_mode
    };
    Kind kind{Kind::unknown};
    std::uint32_t bits{}, mask{}, transport{}, slot{};
    std::uint64_t identity{};
    std::int64_t offset{};
    SourceAllocationRef allocation;
    std::shared_ptr<const SourceContactDifference> difference;
    std::shared_ptr<const SourceTransportWord> source_bytes; // same bank byte origins, never replacement numbers
    static SourceContactQueryWord number(std::uint32_t n)
    {
        SourceContactQueryWord v;
        v.kind = Kind::number;
        v.bits = n;
        v.mask = 0xffffffffu;
        return v;
    }
    static SourceContactQueryWord opaque(std::uint32_t n)
    {
        SourceContactQueryWord v;
        v.kind = Kind::opaque;
        v.transport = n;
        return v;
    }
    static SourceContactQueryWord address(Kind k, std::int64_t offset)
    {
        SourceContactQueryWord v;
        v.kind = k;
        v.offset = offset;
        return v;
    }
    bool address() const noexcept
    {
        return kind == Kind::stack || kind == Kind::global || kind == Kind::pmd || kind == Kind::vehicle ||
               kind == Kind::object_mode;
    }
};
struct SourceContactDifference
{
    SourceContactQueryWord positive, negative;
};
} // namespace sgn::retail
