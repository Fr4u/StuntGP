#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
std::span<const std::uint8_t> SourceApplicationLiteralRef::camera_startup_literal(std::uint32_t address) const &
{
    const auto identity = identity_.lock();
    if (!identity)
        throw std::logic_error("camera App literal owner expired");
    const auto &owner = *identity->owner;
    owner.require_owner_thread();
    if (address >= 0x472ad8 && address < 0x472b78)
        return std::span(owner.camera_literals_472ad8_).subspan(address - 0x472ad8);
    if (address >= 0x478b5c && address < 0x478b68)
        return std::span(owner.camera_equals_478b5c_).subspan(address - 0x478b5c);
    throw std::out_of_range("camera literal outside its canonical App regions");
}
std::span<std::uint8_t> SourceApplicationLiteralData::mutable_camera_startup_literal(std::uint32_t address) &
{
    require_owner_thread();
    if (address >= 0x472ad8 && address < 0x472b78)
        return std::span(camera_literals_472ad8_).subspan(address - 0x472ad8);
    if (address >= 0x478b5c && address < 0x478b68)
        return std::span(camera_equals_478b5c_).subspan(address - 0x478b5c);
    throw std::out_of_range("camera literal outside its canonical App regions");
}
} // namespace sgn::retail
