#include "sgn/source_application_literals.hpp"
#include <algorithm>
namespace sgn::retail
{
std::span<const std::uint8_t> SourceApplicationLiteralRef::frontend_csv_literal(std::uint32_t address) const &
{
    const auto id = identity_.lock();
    if (!id)
        throw SourceLiteralReadError(0x434b50, address, "expired frontend CSV literal App owner");
    if (std::this_thread::get_id() != id->thread)
        throw SourceLiteralReadError(0x434b50, address, "frontend CSV literals belong to another thread");
    std::span<const std::uint8_t> bytes;
    if (address >= 0x478b70 && address < 0x4792b0)
        bytes = std::span{id->owner->frontend_literals_478b70_}.subspan(address - 0x478b70);
    if (address >= 0x479420 && address < 0x479454)
        bytes = std::span{id->owner->frontend_literals_479420_}.subspan(address - 0x479420);
    if (address >= 0x4794d8 && address < 0x4794e0)
        bytes = std::span{id->owner->frontend_literals_4794d8_}.subspan(address - 0x4794d8);
    if (bytes.empty())
        throw SourceLiteralReadError(0x434b50, address, "unowned frontend CSV literal address");
    const auto end = std::find(bytes.begin(), bytes.end(), 0);
    if (end == bytes.end())
        throw SourceLiteralReadError(0x434b50, address, "frontend CSV literal lacks a bounded terminator");
    return bytes.first(static_cast<std::size_t>(end - bytes.begin()) + 1);
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint8_t SourceApplicationLiteralRef::read_frontend_decimal_point() const
{
    const auto id = identity_.lock();
    if (!id)
        throw SourceLiteralReadError(0x43c7d2, 0x495fc0, "expired decimal point App owner");
    if (std::this_thread::get_id() != id->thread)
        throw SourceLiteralReadError(0x43c7d2, 0x495fc0, "decimal point belongs to another thread");
    return id->owner->formatter_locale_.decimal_point_495fc0;
}
} // namespace sgn::retail

namespace sgn::retail
{
std::span<const std::uint8_t> SourceApplicationLiteralRef::frontend_csv_float_literal(std::uint32_t address) const &
{
    const auto id = identity_.lock();
    if (!id)
        throw SourceLiteralReadError(0x43c7d2, address, "expired float coefficient App owner");
    if (std::this_thread::get_id() != id->thread)
        throw SourceLiteralReadError(0x43c7d2, address, "float coefficients belong to another thread");
    switch (address)
    {
    case 0x4966d0:
        return id->owner->frontend_float_4966d0_;
    case 0x496830:
        return id->owner->frontend_float_496830_;
    case 0x4962c0:
        return id->owner->frontend_float_4962c0_;
    case 0x4962a8:
        return id->owner->decimal_double_4962a8_;
    default:
        throw SourceLiteralReadError(0x43c7d2, address, "unowned float coefficient table");
    }
}
} // namespace sgn::retail
