#pragma once
#include <array>
#include <string_view>

namespace sgn::retail::contact
{
// 0044693C..0044694D loads this authored level curve through 00434A70.
// Missing, duplicate, malformed or nonfinite data is rejected, never replaced.
std::array<float, 128> decode_contact_restitution(std::string_view level_config);
} // namespace sgn::retail::contact
