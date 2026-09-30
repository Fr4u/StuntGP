#include "sgn/source_input_dispatch.hpp"
#include "source_input_data.hpp"
#include <bit>
#include <type_traits>

namespace sgn::retail
{
static_assert(std::endian::native == std::endian::little);
static_assert(std::is_same_v<std::uint8_t, unsigned char>);
SourceInputStorage::SourceInputStorage() : profiles(source_input_data::profiles) {}
std::span<const std::uint8_t, 13> source_input_profile(const SourceInputProfiles &p, unsigned bank, std::uint32_t index)
{
    if (bank >= p.size() || index >= 4)
        throw FormatError("source input profile outside four imported records");
    const auto *bytes = reinterpret_cast<const unsigned char *>(p[bank].data());
    return std::span<const std::uint8_t, 13>{bytes + index * 13u, 13};
}
MenuDeviceSelectorsView source_menu_selectors(const SourceInputProfiles &p) noexcept
{
    const auto *bytes = reinterpret_cast<const unsigned char *>(p[2].data());
    return {bytes[10], bytes[11], p[2][3]}; // 473982/83/84, actual overlapping owners
}
} // namespace sgn::retail
