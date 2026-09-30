#include "sgn/retail_level_lighting.hpp"
#include <bit>

namespace sgn::retail
{
namespace
{
std::uint32_t atoi32(std::string_view text)
{ // 4631F4: signed result, modulo32-bit digit accumulation
    std::size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || (text[i] >= '\t' && text[i] <= '\r')))
        ++i;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-'))
        negative = text[i++] == '-';
    std::uint32_t result = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        result = result * 10u + std::uint32_t(text[i++] - '0');
    return negative ? 0u - result : result;
}
std::string_view section(std::string_view text, std::string_view key)
{
    const auto at = text.find(key);
    if (at == std::string_view::npos)
        throw FormatError("missing level lighting section");
    return text.substr(at);
}
std::uint32_t value(std::string_view text, std::string_view key)
{ // complete434A40 strstr/strstr/atoi semantics
    text = section(text, key);
    const auto equal = text.find('=');
    if (equal == std::string_view::npos)
        throw FormatError("level lighting value lacks equals sign");
    return atoi32(text.substr(equal + 1));
}
} // namespace
void apply_level_lighting(std::string_view config, RetailAmbientLight &ambient, std::array<std::uint32_t, 3> &fog)
{
    if (config.size() > 1024 * 1024 || config.find('\0') != std::string_view::npos)
        throw FormatError("level lighting input outside bounded text domain");
    const auto a = section(config, "-Ambience[] ="), f = section(config, "-FogColour[] =");
    const auto intensity = value(a, "I");
    const std::array rgb{value(a, "R"), value(a, "G"), value(a, "B")};
    const std::array next_fog{value(f, "R"), value(f, "G"), value(f, "B")};
    // 4468B4..4468DE: FILD signed dword; preserve each original float store.
    ambient.strength = vehicle_f32(double(std::bit_cast<std::int32_t>(intensity)) * std::bit_cast<float>(0x3b808081u));
    for (unsigned i = 0; i < 3; ++i)
        ambient.color[i] = vehicle_f32(double(std::bit_cast<std::int32_t>(rgb[i])));
    update_ambient_light(ambient);
    fog = next_fog;
}
} // namespace sgn::retail
