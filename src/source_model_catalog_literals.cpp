#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_model_catalog_word_444db0(std::uint32_t address,
                                                                          std::uint32_t site) const
{
    switch (site)
    {
    case 0x444f4e:
    case 0x444f5a:
    case 0x444f9f:
    case 0x445034:
    case 0x44507b:
    case 0x444f75:
    case 0x444fc6:
    case 0x44504b:
    case 0x4450a6:
        break;
    default:
        throw SourceLiteralReadError(site, address, "not a444DB0 table/MOVSD source DWORD read");
    }
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired model catalog App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "model catalog App belongs to another thread");
    const auto offset = static_cast<std::uint32_t>(address - 0x482b88u);
    const auto &bytes = identity->owner->catalog_bytes_482b88_;
    if (offset > bytes.size() - 4)
        throw SourceLiteralReadError(site, address, "reached unowned model catalog DWORD");
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}
std::uint8_t SourceApplicationLiteralRef::read_model_catalog_byte_444db0(std::uint32_t address,
                                                                         std::uint32_t site) const
{
    switch (site)
    {
    case 0x444f66:
    case 0x444fb0:
    case 0x44503c:
    case 0x445097:
    case 0x444f7e:
    case 0x444fcf:
    case 0x445054:
    case 0x4450af:
        break;
    default:
        throw SourceLiteralReadError(site, address, "not a444DB0 SCASB/MOVSB source BYTE read");
    }
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired model catalog App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "model catalog App belongs to another thread");
    const auto offset = static_cast<std::uint32_t>(address - 0x482b88u);
    const auto &bytes = identity->owner->catalog_bytes_482b88_;
    if (offset >= bytes.size())
        throw SourceLiteralReadError(site, address, "reached unowned model catalog BYTE");
    return bytes[offset];
}
} // namespace sgn::retail

namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_model_memory_budget_444ea2() const
{
    constexpr std::uint32_t site = 0x444ea2, address = 0x477c30;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired model memory-budget App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "model memory-budget App belongs to another thread");
    return identity->owner->model_memory_budget_477c30_;
}
std::uint32_t &SourceApplicationLiteralData::mutable_model_memory_budget_477c30() &
{
    require_owner_thread();
    return model_memory_budget_477c30_;
}
} // namespace sgn::retail
