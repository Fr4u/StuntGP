#include "sgn/retail_lighting.hpp"
#include "sgn/retail_time_trial.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace sgn::retail
{
namespace
{
float f32(double x)
{
    return vehicle_f32(x);
}
std::uint32_t trunc_word(double x)
{ // 462860 returns the low word of FISTP qword.
    if (!std::isfinite(x) || x >= 9223372036854775808.0 || x < -9223372036854775808.0)
        return 0;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(x)));
}
std::uint32_t word(std::span<const std::uint8_t> b, std::size_t at)
{
    if (at > b.size() || b.size() - at < 4)
        throw FormatError("truncated PMD lighting field");
    return std::uint32_t(b[at]) | (std::uint32_t(b[at + 1]) << 8) | (std::uint32_t(b[at + 2]) << 16) |
           (std::uint32_t(b[at + 3]) << 24);
}
double dot_zyx(const contact::Vector4 &p, const CameraVector &n)
{
    return (double(p.z) * n.z + double(p.y) * n.y) + double(p.x) * n.x;
}
void check_bank(const PmdLightBank &bank)
{
    if (bank.render_count > bank.records.size())
        throw FormatError("PMD light view count exceeds total count");
}
} // namespace
PmdLightBank decode_pmd_lights(std::span<const std::uint8_t> raw)
{
    static_assert(std::endian::native == std::endian::little);
    if (raw.size() < 328 || std::string_view(reinterpret_cast<const char *>(raw.data()), 9) != "PMD V1.83")
        throw FormatError("lighting requires PMD V1.83");
    const auto count = word(raw, 0xfc), render_count = word(raw, 0x100);
    const std::uint64_t lights = 328ull + word(raw, 0x70), ambient = 328ull + word(raw, 0x64);
    if (count > 65536 || render_count > count || lights + std::uint64_t(count) * 0x90 > raw.size() ||
        ambient + 40 > raw.size())
        throw FormatError("PMD lighting section outside input");
    PmdLightBank out;
    out.render_count = render_count;
    out.records.resize(count);
    if (count)
        std::memcpy(out.records.data(), raw.data() + lights, std::size_t(count) * 0x90);
    std::memcpy(&out.authored_ambient, raw.data() + ambient, 40);
    return out;
}
void select_shadow_light(PmdLightBank &bank)
{
    bank.selected = bank.records.empty() ? std::nullopt : std::optional<std::uint32_t>{0};
    for (std::uint32_t i = 0; i < bank.records.size(); ++i)
        if (bank.records[i].type == 3)
        {
            bank.selected = std::optional<std::uint32_t>{i};
            break;
        }
}
void initialize_pmd_lights(PmdLightBank &bank, RetailAmbientLight &ambient)
{
    check_bank(bank);
    for (auto &light : bank.records)
    {
        if (light.active == 1 && light.kind == 1)
        {
            const double value = double(light.strength) * 4.75;
            light.strength = f32(value * value);
        }
        else
        {
            const double r = double(light.color[0]) * light.strength, g = double(light.color[1]) * light.strength;
            const float b = f32(double(light.color[2]) * light.strength);
            light.scaled_color = {trunc_word(r), trunc_word(g), trunc_word(b)};
        }
    }
    ambient = bank.authored_ambient;
    update_ambient_light(ambient);
    select_shadow_light(bank);
}
void update_ambient_light(RetailAmbientLight &ambient)
{
    const double r = double(ambient.strength) * ambient.color[0], g = double(ambient.color[1]) * ambient.strength;
    const float b = f32(double(ambient.color[2]) * ambient.strength);
    ambient.scaled = {f32(r), f32(g), b};
    ambient.packed = {trunc_word(r) << 8, trunc_word(f32(g)) << 8, trunc_word(b) << 8};
}
void set_light_gamma(LightFadeState &state, float factor, LightGammaDevice *device)
{
    if (!device || !(std::abs(double(factor) - state.last_gamma) >= 0.01))
        return;
    state.last_gamma = factor;
    double power = (double(factor) - 1.0) * 0.2;
    if (power > 0.98)
        power = double(0.98f);
    const auto base = trunc_word((power * power) * 65280);
    const std::uint32_t step = ((base >> 6) - base + 0xff00u) >> 8;
    std::uint32_t value = base;
    for (unsigned i = 0; i < 256; ++i)
    {
        const auto entry = static_cast<std::uint16_t>(std::min(value, 0xfffeu));
        state.ramp[i] = entry;
        state.ramp[256 + i] = entry;
        state.ramp[512 + i] = entry;
        value += step;
    }
    device->apply(state.ramp);
}
void update_light_fade(LightFadeState &state, const ControllerOverlayTransition &overlay, std::uint32_t split,
                       LightGammaDevice *device)
{
    if (overlay.active == 1 || split != 0)
    {
        state.current = 1;
        state.target = 1;
        set_light_gamma(state, 1, device);
        return;
    }
    const double delta = double(state.target) - state.current;
    if (std::abs(delta) > double(0.2f))
        state.current = f32(double(state.current) + (delta > 0 ? .5 : double(-0.05f)));
    if (state.current > 3)
    {
        const double value = double(state.blend) + double(0.0075f);
        state.blend = value > 1 ? 1 : f32(value);
    }
    else
    {
        const double value = double(state.blend) - double(0.05f);
        state.blend = !(value >= 0) ? 0 : f32(value);
    }
    set_light_gamma(state, f32(double(state.current) - state.blend), device);
}
void prepare_light_view(PmdLightBank &bank, LightViewSelection &selection, LightFadeState &fade, LightCameraView view,
                        std::uint32_t split, LightGammaDevice *device)
{
    check_bank(bank);
    if (selection.ordered.size() < bank.render_count)
        selection.ordered.resize(bank.render_count);
    update_light_fade(fade, view.overlay, split, device);
    selection.active_count = 0;
    for (std::uint32_t i = 0; i < bank.render_count; ++i)
    {
        auto &light = bank.records[i];
        if (light.active == 0 || (light.kind != 0 && light.kind != 1))
            continue;
        auto &relative = light.relative;
        relative.x = f32(double(light.position.x) - view.camera.position.x);
        relative.y = f32(double(light.position.y) - view.camera.position.y);
        relative.z = f32(double(light.position.z) - view.camera.position.z);
        const double depth = dot_zyx(relative, view.presentation.forward_axis);
        if (light.kind == 1)
        {
            const float radius = f32((light.strength >= 1 ? double(light.strength) : 1.0) * 500);
            bool visible = !(depth > view.lifecycle.clip_far);
            for (const auto &plane : view.presentation.planes)
                if (dot_zyx(relative, plane) > radius)
                    visible = false;
            light.visible = light.type == 3 && visible ? 1u : 0u;
            if (!visible)
                continue;
        }
        else
        {
            bool visible = light.type == 3 && !(depth > view.lifecycle.clip_far) && depth >= view.lifecycle.clip_near;
            for (const auto &plane : view.presentation.planes)
                if (dot_zyx(relative, plane) > 5000)
                    visible = false;
            light.visible = visible ? 1u : 0u;
        }
        // 624534..624573 contains16 pointers. The next word is the separate
        // render-buffer cursor624574, not a seventeenth safe list element.
        if (selection.active_count == 16)
            throw FormatError("active lights exceed original 16-pointer list");
        selection.ordered[selection.active_count++] = i;
    }
}
std::optional<VehicleShadowLighting> shadow_lighting(const PmdLightBank &bank, const LightViewSelection &selection)
{
    if (const auto *pointer = std::get_if<SourceSceneAllocationPointer>(&bank.selected))
        return VehicleShadowLighting{selection.active_count, pointer->read<contact::Vector4>(0x10, 0x41c01e)};
    if (const auto *pointer = std::get_if<SourceSceneStaticPointer>(&bank.selected))
    {
        if (pointer->source_address() != 0x589628)
            throw FormatError("unknown source shadow-light static alias");
        return VehicleShadowLighting{selection.active_count, pointer->read<contact::Vector4>(0x10)};
    }
    const auto index = std::get<std::optional<std::uint32_t>>(bank.selected);
    if (!index)
        return std::nullopt;
    return VehicleShadowLighting{selection.active_count, bank.records.at(*index).position};
}
} // namespace sgn::retail
