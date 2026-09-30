#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
std::uint32_t SourceApplicationLiteralRef::read_texture_override_pointer_445439(std::uint32_t address) const
{
    constexpr std::uint32_t site = 0x445439;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired texture-override App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "texture-override App belongs to another thread");
    const auto offset = static_cast<std::uint32_t>(address - 0x482b88u);
    const auto &bytes = identity->owner->catalog_bytes_482b88_;
    if (offset > bytes.size() - 4)
        throw SourceLiteralReadError(site, address, "reached unowned texture table DWORD");
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}
std::uint8_t SourceApplicationLiteralRef::read_texture_override_byte(std::uint32_t address, std::uint32_t site) const
{
    if (site != 0x44544au && site != 0x445463u)
        throw SourceLiteralReadError(site, address, "not a4453A0 SCASB/MOVSB source read");
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired texture-override App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "texture-override App belongs to another thread");
    const auto offset = static_cast<std::uint32_t>(address - 0x482b88u);
    const auto &bytes = identity->owner->catalog_bytes_482b88_;
    if (offset >= bytes.size())
        throw SourceLiteralReadError(site, address, "reached unowned texture text BYTE");
    return bytes[offset];
}
std::uint32_t SourceApplicationLiteralRef::read_texture_override_word_44545c(std::uint32_t address) const
{
    constexpr std::uint32_t site = 0x44545c;
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired texture-override App owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "texture-override App belongs to another thread");
    const auto offset = static_cast<std::uint32_t>(address - 0x482b88u);
    const auto &bytes = identity->owner->catalog_bytes_482b88_;
    // Validate the one full reached DWORD read before exposing its value.
    if (offset > bytes.size() - 4)
        throw SourceLiteralReadError(site, address, "reached unowned texture text DWORD");
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}
} // namespace sgn::retail
