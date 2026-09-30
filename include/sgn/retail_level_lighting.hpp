#pragma once
#include "sgn/retail_lighting.hpp"
#include <string_view>

namespace sgn::retail
{
// Scalar section of446840: four signed ambience values,42D7E0,three fog words.
// Borrow the existing scene ambient and fog owners; curves/triggers are separate.
void apply_level_lighting(std::string_view config, RetailAmbientLight &, std::array<std::uint32_t, 3> &fog);
} // namespace sgn::retail
