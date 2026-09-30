#include "sgn/retail_world_view_prefix.hpp"
#include "sgn/retail_camera_tables.hpp"
#include <bit>
#include <cstring>

namespace sgn::retail
{
namespace
{
float f32(double value)
{
    return static_cast<float>(value);
}
std::uint32_t word(std::span<const std::uint8_t> b, std::size_t offset)
{
    if (offset > b.size() || b.size() - offset < 4)
        throw FormatError("truncated texture animation PMD word");
    std::uint32_t value;
    std::memcpy(&value, b.data() + offset, 4);
    return value;
}
void wrap(float &value)
{
    // Single corrections, not modulo: exact +-1 stays, excursions over2 remain.
    // x87 unordered comparison also takes the source negative correction.
    if (value > 1)
        value = f32(double(value) - 1);
    if (!(value >= -1))
        value = f32(double(value) + 1);
}
} // namespace
TextureAnimationBank::TextureAnimationBank(std::span<const std::uint8_t> pmd)
{
    if (pmd.size() < 328 || std::string_view(reinterpret_cast<const char *>(pmd.data()), 9) != "PMD V1.83")
        throw FormatError("texture animation requires original PMD V1.83");
    const std::uint64_t start = 328ull + word(pmd, 0x5c), end = 328ull + word(pmd, 0x60);
    if (start <= 328 || end <= start || end > pmd.size() || end - start > 20ull * 65536)
        throw FormatError("texture animation section is absent or outside PMD");
    source_offset_ = static_cast<std::size_t>(start);
    for (std::size_t at = source_offset_; at < end; at += 20)
    {
        if (end - at < 4)
            break;
        if (word(pmd, at) == 0xffffffffu)
            return;
        if (end - at < 20)
            break;
        TextureMotion record;
        std::memcpy(&record, pmd.data() + at, 20);
        records_.push_back(record);
    }
    throw FormatError("texture animation lacks terminator before next PMD section");
}
void reset_view_matrix_cursor(ViewMatrixArena &s) noexcept
{
    s.cursor = 0;
    s.count = 0;
}
void update_world_view_quality(WorldViewQuality &s, std::uint32_t rate) noexcept
{
    s.first = 0;
    s.second = 0;
    if (rate > 80)
    {
        s.first = 1;
        s.second = 1;
    }
    else if (rate > 50)
        s.first = 1;
    if (s.legacy_driver_restriction == 1)
        s.second = 0;
}
void initialize_legacy_driver_restriction(WorldViewQuality &s) noexcept
{
    s.legacy_driver_restriction = 0;
}
void clear_sky_texture_motion(std::array<SkyTextureMotion, 2> &sky) noexcept
{
    sky = {};
}
void initialize_sky_texture_motion(std::array<SkyTextureMotion, 2> &sky, std::optional<std::string_view> name)
{
    if (!name)
        return;
    if (name->find('\0') != name->npos)
        throw FormatError("sky name contains embedded NUL");
    clear_sky_texture_motion(sky);
    if (name->find("AnimSkyGround") != name->npos)
    {
        sky[0].step_u = 0;
        sky[0].step_v = 0;
        sky[1].step_u = std::bit_cast<float>(0x3a378034u);
        sky[1].step_v = std::bit_cast<float>(0x3a378034u);
    }
}
void advance_texture_animation(TextureAnimationBank &bank, TextureAnimationState &s, std::uint32_t paused)
{
    if (paused == 1)
        return;
    s.phase = (s.phase + (s.phase >= 0x800 && s.phase < 0x1800 ? 0x10u : 0x20u)) & 8191u;
    s.oscillation = f32(double(std::bit_cast<float>(camera_detail::trig_bits[s.phase])) * 0.046875);
    for (auto &t : bank.records())
    {
        // A live -1 written by another source producer ends traversal too.
        if (t.kind == 0xffffffffu)
            break;
        if (t.kind == 0)
            continue;
        if (t.kind == 2)
        {
            t.u = f32(double(t.step_u) + t.u);
            t.v = s.oscillation;
            if (t.u > 1)
                t.u = f32(double(t.u) - 1);
        }
        else
        {
            t.u = f32(double(t.u) + t.step_u);
            t.v = f32(double(t.step_v) + t.v);
            wrap(t.u);
            wrap(t.v);
        }
    }
    for (auto &t : s.sky)
    {
        t.u = f32(double(t.step_u) + t.u);
        t.v = f32(double(t.v) + t.step_v);
        wrap(t.u);
        wrap(t.v);
    }
}
void prepare_world_view_prefix(WorldViewPrefix w)
{
    update_world_view_quality(w.quality, w.rate_selector);
    reset_view_matrix_cursor(w.matrices);
    advance_texture_animation(w.textures, w.animation, w.frame_pause);
}
} // namespace sgn::retail
