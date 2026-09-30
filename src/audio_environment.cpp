#include "sgn/audio_environment.hpp"
#include "sgn/format_require.hpp"
#include <bit>
#include <charconv>
#include <limits>

namespace sgn::retail
{
namespace
{
constexpr std::array<std::uint32_t, 14> outdoor{0xfffffc18, 0xfffffce0, 0,          0x3fbeb852, 0x3f2b851f,
                                                0xfffff71f, 0x3be56042, 0xfffff965, 0x3c343958, 0x10,
                                                0x40f00000, 0x3f000000, 0xc0a00000, 0x2f};
constexpr std::array<std::uint32_t, 14> large{0xfffffce0, 0xfffffed4, 0, 0x40a00000, 0x3f23d70a, 0xfffffd39, 0x3c449ba6,
                                              0x53,       0x3c8b4396, 5, 0x4139999a, 0x3f800000, 0xc0a00000, 0x2f};
constexpr std::array<std::uint32_t, 14> enclosed{0xffffff38, 0xfffffc18, 0,          0x4033d70a, 0x3e0f5c29,
                                                 0x1ad,      0x3c656042, 0x3ff,      0x3cac0831, 0x15,
                                                 0x3fd9999a, 0x3f4ccccd, 0xc0a00000, 0x2f};
constexpr std::uint32_t sentinel = 0xffffffffu;
using sgn::format_check::require;
std::size_t field_start(std::string_view text, std::string_view key)
{
    auto at = text.find(key);
    require(at != text.npos, "missing level box field");
    at = text.find('=', at);
    require(at != text.npos, "missing level box equals sign");
    return at + 1;
}
double number(std::string_view text, std::size_t at)
{
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n'))
        ++at;
    if (at < text.size() && text[at] == '+')
        ++at;
    double value{};
    const auto r = std::from_chars(text.data() + at, text.data() + text.size(), value, std::chars_format::general);
    require(r.ec == std::errc{} && std::isfinite(value), "invalid level box number");
    return value;
}
void vector_field(std::string_view text, std::string_view key, VehicleVector &destination)
{
    auto at = field_start(text, key);
    const auto next = [&] { // 4349B0 advances by literal spaces, not arbitrary whitespace
        while (at < text.size() && text[at] == ' ')
            ++at;
        at = text.find(' ', at);
        require(at != text.npos, "unterminated level box vector");
        ++at;
    };
    destination.x = vehicle_f32(number(text, at));
    next();
    destination.y = vehicle_f32(number(text, at));
    next();
    destination.z = vehicle_f32(number(text, at));
    require(std::isfinite(destination.x) && std::isfinite(destination.y) && std::isfinite(destination.z),
            "level box vector exceeds binary32");
    // 4349B0 writes XYZ only. The fourth word is not an authored field.
}
} // namespace
AudioEnvironmentState initial_audio_environment_state()
{
    AudioEnvironmentState state{};
    state.regions[0] = {0, 0, {1000, 1000, 1000, 0}, {1, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    state.regions[1].type = sentinel;
    return state;
}
bool audio_environment_contains(const AudioEnvironmentRegion &r, Vec3 point)
{
    // 4278A0 never initializes cross.w at [EBP-18];44F050 copies it and
    // 44EE50 exposes it as translation.x. New native policy: zero this undefined
    // stack lane (vehicle_cross). Retain authored/persistent v.w and n.w.
    const auto matrix = vehicle_transpose(vehicle_basis(vehicle_cross(r.v, r.n), r.v, r.n));
    const double x = double(point.x) - r.position.x, y = double(point.y) - r.position.y,
                 z = double(point.z) - r.position.z;
    const float tx = vehicle_f32(
        double(vehicle_f32(((double(matrix[8]) * z + double(matrix[4]) * y) + double(matrix[0]) * x) + matrix[12])) +
        r.position.x);
    const float ty = vehicle_f32(
        double(vehicle_f32(((double(matrix[9]) * z + double(matrix[5]) * y) + double(matrix[1]) * x) + matrix[13])) +
        r.position.y);
    const float tz = vehicle_f32(
        (((double(matrix[10]) * z + double(matrix[6]) * y) + double(matrix[2]) * x) + matrix[14]) + r.position.z);
    constexpr std::array<Vec3, 6> normals{{{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}, {1, 0, 0}, {0, -1, 0}, {0, 0, 1}}};
    for (unsigned side = 0; side < 2; ++side)
    {
        const double sign = side == 0 ? -0.5 : 0.5;
        const float dx = vehicle_f32(double(tx) - (double(r.scale.x) * sign + r.position.x));
        const float cy = vehicle_f32(double(vehicle_f32(double(r.scale.y) * -sign)) + r.position.y);
        const float cz = vehicle_f32(double(vehicle_f32(double(r.scale.z) * sign)) + r.position.z);
        const float dy = vehicle_f32(double(ty) - cy), dz = vehicle_f32(double(tz) - cz);
        for (unsigned i = side * 3; i < side * 3 + 3; ++i)
        {
            const auto n = normals[i];
            const float dot = vehicle_f32((double(n.z) * dz + double(n.y) * dy) + double(n.x) * dx);
            // Signed integer TEST/JL of the stored float. Do not replace by dot<0.
            if ((std::bit_cast<std::uint32_t>(dot) & 0x80000000u) == 0)
                return false;
        }
    }
    return true;
}
bool audio_environment_support(AudioEnvironmentWorld &w, AudioPropertyId property, std::uint32_t identifier)
{
    std::uint32_t support{};
    if (w.device.audio_property_support(property, audio_environment_property_set, identifier, support) != 0)
        return false;
    if ((support & 3) != 3)
        return false;
    w.state.supported_properties |= std::uint32_t{1} << (identifier & 31);
    return true;
}
bool set_audio_environment(AudioEnvironmentWorld &w, std::uint32_t type)
{
    if (w.driver.initialized != 1 || w.spatial_enabled != 1 || type > 6)
        return true;
    w.state.properties = type == 0 ? outdoor : type <= 3 ? large : enclosed;
    if (type == 5)
        w.state.properties[0] = 0xfffffbb4;
    if (type == 6)
        w.state.properties[0] = 0xfffffe0c;
    AudioPropertyId property{};
    if (w.device.query_audio_property(w.driver.samples[1].spatial, audio_property_interface, property) != 0)
        return false;
    if (!audio_environment_support(w, property, 1))
        return false;
    static_assert(std::endian::native == std::endian::little);
    const auto *data = reinterpret_cast<const std::uint8_t *>(w.state.properties.data());
    // 460AC5 ignores Set's HRESULT. No Release is issued by the original tail.
    (void)w.device.set_audio_property(property, audio_environment_property_set, 1, {}, std::span(data, 56));
    return true;
}
void initialize_audio_environments(AudioEnvironmentWorld &w)
{
    std::uint32_t count = 0;
    while (count < w.state.regions.size() && w.state.regions[count].type != sentinel)
        ++count;
    require(count < w.state.regions.size(), "level box list has no sentinel");
    w.state.count = count;
    if (count != 0)
        (void)set_audio_environment(w, w.state.regions[0].type);
}
void update_audio_environment(AudioEnvironmentWorld &w, Vec3 camera_position)
{
    std::uint32_t selected = 0;
    for (std::uint32_t i = 1;; ++i)
    {
        require(i < w.state.regions.size(), "level box selector escaped sentinel bank");
        if (w.state.regions[i].type == sentinel)
            break;
        if (audio_environment_contains(w.state.regions[i], camera_position))
        {
            selected = i;
            break;
        }
    }
    if (selected == w.state.current_region)
        return;
    const auto type = w.state.regions[selected].type;
    if (type > 6)
        return;
    (void)set_audio_environment(w, type);
    w.state.current_region = selected;
}
void load_audio_environments(std::span<const std::uint8_t> bytes, AudioEnvironmentWorld &w)
{
    require(!bytes.empty() && bytes.size() <= 1024 * 1024, "invalid level box configuration size");
    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    require(text.find('\0') == text.npos, "NUL in level box configuration");
    auto at = text.find("-LevelBoxList[] =");
    require(at != text.npos, "missing authored level box list");
    at = text.find("-Type", at + 16);
    std::size_t count = 0;
    // Parse into a transactional copy: malformed source memory accesses are new
    // boundary errors, never partially published successful level transitions.
    auto regions = w.state.regions;
    while (at != text.npos)
    {
        require(count + 1 < regions.size(), "level box list exceeds sentinel capacity");
        const auto block = text.substr(at);
        auto &r = regions[count++];
        auto start = field_start(block, "-Type");
        while (start < block.size() &&
               (block[start] == ' ' || block[start] == '\t' || block[start] == '\r' || block[start] == '\n'))
            ++start;
        if (start < block.size() && block[start] == '+')
            ++start;
        std::int32_t type{};
        const auto parsed = std::from_chars(block.data() + start, block.data() + block.size(), type);
        require(parsed.ec == std::errc{}, "invalid level box type");
        r.type = std::bit_cast<std::uint32_t>(type);
        vector_field(block, "-Position", r.position);
        vector_field(block, "-Scale", r.scale);
        vector_field(block, "-V", r.v);
        vector_field(block, "-N", r.n);
        at = text.find("-Type", at + 16);
    }
    regions[count].type = sentinel;
    w.state.regions = regions;
    initialize_audio_environments(w);
}
} // namespace sgn::retail
