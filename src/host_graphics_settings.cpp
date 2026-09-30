#include "sgn/host_graphics_settings.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

// NEW HOST graphics settings store (R22); see the header.
namespace sgn::host_graphics
{
namespace
{
struct Store
{
    std::mutex mutex;
    Settings settings;
    std::uint64_t revision{1};
};
Store &store()
{
    static Store s;
    return s;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}
bool parse_bool(std::string_view v, bool &out)
{
    if (v == "1" || v == "true" || v == "on")
    {
        out = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "off")
    {
        out = false;
        return true;
    }
    return false;
}
bool parse_u32(std::string_view v, std::uint32_t &out)
{
    std::uint32_t value{};
    const auto [end, error] = std::from_chars(v.data(), v.data() + v.size(), value);
    if (error != std::errc{} || end != v.data() + v.size())
        return false;
    out = value;
    return true;
}
std::string_view name(TextureFilter f)
{
    switch (f)
    {
    case TextureFilter::original:
        return "original";
    case TextureFilter::trilinear:
        return "trilinear";
    case TextureFilter::anisotropic:
        return "anisotropic";
    }
    return "anisotropic";
}
std::string_view name(PostAntiAliasing a)
{
    return a == PostAntiAliasing::fxaa ? "fxaa" : "off";
}
std::string_view name(DisplayMode m)
{
    switch (m)
    {
    case DisplayMode::windowed:
        return "windowed";
    case DisplayMode::borderless:
        return "borderless";
    case DisplayMode::exclusive:
        return "exclusive";
    }
    return "windowed";
}
void apply(Settings &s, std::string_view key, std::string_view value)
{
    bool b{};
    std::uint32_t u{};
    if (key == "interpolation")
    {
        if (parse_bool(value, b))
            s.interpolation = b;
    }
    else if (key == "vsync")
    {
        if (parse_bool(value, b))
            s.vsync = b;
    }
    else if (key == "original_4x3")
    {
        if (parse_bool(value, b))
            s.original_4x3 = b;
    }
    else if (key == "msaa_samples")
    {
        if (parse_u32(value, u))
            s.msaa_samples = u;
    }
    else if (key == "texture_filter")
    {
        if (value == "original")
            s.texture_filter = TextureFilter::original;
        else if (value == "trilinear")
            s.texture_filter = TextureFilter::trilinear;
        else if (value == "anisotropic")
            s.texture_filter = TextureFilter::anisotropic;
    }
    else if (key == "anisotropy")
    {
        if (parse_u32(value, u))
            s.anisotropy = u;
    }
    else if (key == "generate_mipmaps")
    {
        if (parse_bool(value, b))
            s.generate_mipmaps = b;
    }
    else if (key == "alpha_to_coverage")
    {
        if (parse_bool(value, b))
            s.alpha_to_coverage = b;
    }
    else if (key == "post_anti_aliasing")
    {
        if (value == "off")
            s.post_anti_aliasing = PostAntiAliasing::off;
        else if (value == "fxaa")
            s.post_anti_aliasing = PostAntiAliasing::fxaa;
    }
    else if (key == "smooth_gradients")
    {
        if (parse_bool(value, b))
            s.smooth_gradients = b;
    }
    else if (key == "texture_edge_fix")
    {
        if (parse_bool(value, b))
            s.texture_edge_fix = b;
    }
    else if (key == "frame_rate_limit")
    {
        if (parse_u32(value, u))
            s.frame_rate_limit = u;
    }
    else if (key == "high_precision_textures")
    {
        if (parse_bool(value, b))
            s.high_precision_textures = b;
    }
    else if (key == "sharp_hud")
    {
        if (parse_bool(value, b))
            s.sharp_hud = b;
    }
    else if (key == "smooth_magnification")
    {
        if (parse_bool(value, b))
            s.smooth_magnification = b;
    }
    else if (key == "bloom")
    {
        if (parse_bool(value, b))
            s.bloom = b;
    }
    else if (key == "smooth_effects")
    {
        if (parse_bool(value, b))
            s.smooth_effects = b;
    }
    else if (key == "texture_sharpness")
    {
        std::int32_t v{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (error == std::errc{} && end == value.data() + value.size())
            s.texture_sharpness = v;
    }
    else if (key == "low_latency")
    {
        if (parse_bool(value, b))
            s.low_latency = b;
    }
    else if (key == "display_mode")
    {
        if (value == "windowed")
            s.display_mode = DisplayMode::windowed;
        else if (value == "borderless")
            s.display_mode = DisplayMode::borderless;
        else if (value == "exclusive")
            s.display_mode = DisplayMode::exclusive;
    }
}
} // namespace

Settings sanitize(Settings s) noexcept
{
    if (s.msaa_samples != 1 && s.msaa_samples != 2 && s.msaa_samples != 4 && s.msaa_samples != 8)
        s.msaa_samples = s.msaa_samples > 8 ? 8u : (s.msaa_samples > 4 ? 4u : (s.msaa_samples > 2 ? 2u : 1u));
    if (static_cast<std::uint8_t>(s.texture_filter) > 2)
        s.texture_filter = TextureFilter::anisotropic;
    s.anisotropy = std::clamp<std::uint32_t>(s.anisotropy, 2u, 16u);
    if (static_cast<std::uint8_t>(s.post_anti_aliasing) > 1)
        s.post_anti_aliasing = PostAntiAliasing::off;
    if (static_cast<std::uint8_t>(s.display_mode) > 2)
        s.display_mode = DisplayMode::windowed;
    if (s.frame_rate_limit != 0)
        s.frame_rate_limit = std::clamp<std::uint32_t>(s.frame_rate_limit, 30u, 1000u);
    s.texture_sharpness = std::clamp<std::int32_t>(s.texture_sharpness, -3, 3);
    return s;
}
Settings current() noexcept
{
    auto &s = store();
    std::lock_guard lock(s.mutex);
    return s.settings;
}
std::uint64_t revision() noexcept
{
    auto &s = store();
    std::lock_guard lock(s.mutex);
    return s.revision;
}
void set(const Settings &value) noexcept
{
    const auto next = sanitize(value);
    auto &s = store();
    std::lock_guard lock(s.mutex);
    if (s.settings == next)
        return;
    s.settings = next;
    ++s.revision;
}
std::filesystem::path default_path(const std::filesystem::path &game_root)
{
    return game_root / "settings" / "stuntgp-new-graphics.cfg";
}
bool load(const std::filesystem::path &path) noexcept
{
    try
    {
        std::error_code error;
        if (!std::filesystem::exists(path, error))
            return !error;
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;
        Settings next = current();
        std::string line;
        while (std::getline(in, line))
        {
            const std::string_view view = trim(line);
            if (view.empty() || view.front() == '#' || view.front() == ';')
                continue;
            const auto equals = view.find('=');
            if (equals == std::string_view::npos)
                continue;
            apply(next, trim(view.substr(0, equals)), trim(view.substr(equals + 1)));
        }
        set(next);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
bool save(const std::filesystem::path &path) noexcept
{
    return save(path, current());
}
bool save(const std::filesystem::path &path, const Settings &value) noexcept
{
    try
    {
        const auto s = sanitize(value);
        std::ostringstream text;
        text << "# Stunt GP NEW graphics settings (host presentation only; not a save game).\n"
             << "interpolation=" << (s.interpolation ? 1 : 0) << "\n"
             << "vsync=" << (s.vsync ? 1 : 0) << "\n"
             << "original_4x3=" << (s.original_4x3 ? 1 : 0) << "\n"
             << "msaa_samples=" << s.msaa_samples << "\n"
             << "texture_filter=" << name(s.texture_filter) << "\n"
             << "anisotropy=" << s.anisotropy << "\n"
             << "generate_mipmaps=" << (s.generate_mipmaps ? 1 : 0) << "\n"
             << "alpha_to_coverage=" << (s.alpha_to_coverage ? 1 : 0) << "\n"
             << "post_anti_aliasing=" << name(s.post_anti_aliasing) << "\n"
             << "display_mode=" << name(s.display_mode) << "\n"
             << "smooth_gradients=" << (s.smooth_gradients ? 1 : 0) << "\n"
             << "texture_edge_fix=" << (s.texture_edge_fix ? 1 : 0) << "\n"
             << "frame_rate_limit=" << s.frame_rate_limit << "\n"
             << "low_latency=" << (s.low_latency ? 1 : 0) << "\n"
             << "high_precision_textures=" << (s.high_precision_textures ? 1 : 0) << "\n"
             << "sharp_hud=" << (s.sharp_hud ? 1 : 0) << "\n"
             << "texture_sharpness=" << s.texture_sharpness << "\n"
             << "smooth_magnification=" << (s.smooth_magnification ? 1 : 0) << "\n"
             << "bloom=" << (s.bloom ? 1 : 0) << "\n"
             << "smooth_effects=" << (s.smooth_effects ? 1 : 0) << "\n";
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return false;
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            if (!out)
                return false;
            const auto body = text.str();
            out.write(body.data(), static_cast<std::streamsize>(body.size()));
            if (!out.flush())
                return false;
        }
        std::filesystem::rename(temporary, path, error); // replaces an existing file
        return !error;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace sgn::host_graphics
