#include "sgn/source_application_literals.hpp"

namespace sgn::retail
{
std::uint16_t SourceApplicationLiteralRef::read_u16_4602b1() const
{
    return read_audio_level_word(0x4897f8, 0x4602b1);
}
std::uint16_t SourceApplicationLiteralRef::read_audio_level_word(std::uint32_t address, std::uint32_t site) const
{
    const auto identity = identity_.lock();
    if (!identity)
        throw SourceLiteralReadError(site, address, "expired music volume data owner");
    if (std::this_thread::get_id() != identity->thread)
        throw SourceLiteralReadError(site, address, "music volume data belongs to another thread");
    const auto &bytes = identity->owner->volume_word_4897f8_;
    const auto offset = address - 0x4897f8u;
    if (offset > 256u)
        throw SourceLiteralReadError(site, address, "music level read outside recovered table");
    return static_cast<std::uint16_t>(std::uint16_t(bytes[offset]) | (std::uint16_t(bytes[offset + 1]) << 8));
}
std::span<std::uint8_t, 2> SourceApplicationLiteralData::mutable_audio_volume_4897f8() &
{
    require_owner_thread();
    return std::span<std::uint8_t, 258>(volume_word_4897f8_).first<2>();
}
std::span<std::uint8_t, 258> SourceApplicationLiteralData::mutable_audio_levels_4897f8() &
{
    require_owner_thread();
    return volume_word_4897f8_;
}
} // namespace sgn::retail
