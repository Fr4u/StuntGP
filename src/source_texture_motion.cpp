#include "sgn/source_texture_motion.hpp"
#include "sgn/race_session.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/retail_camera_tables.hpp"
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>

extern "C" std::uint32_t sgn_source_uv_first_4258de(const std::uint32_t *, const void *, float *) noexcept;
extern "C" void sgn_source_uv_record_425904(void *, const float *, std::uint32_t) noexcept;
extern "C" void sgn_source_uv_sky_4259e3(float *) noexcept;

namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
static_assert(offsetof(SkyTextureMotion, u) == 4 && offsetof(SkyTextureMotion, v) == 8 &&
              offsetof(SkyTextureMotion, step_u) == 12 && offsetof(SkyTextureMotion, step_v) == 16);
class MotionPointer
{
    SourceAllocationBank &bank_;
    std::variant<SourcePmdLocation, SourceSceneStaticPointer> captured_;

  public:
    MotionPointer(SourceAllocationBank &bank, decltype(captured_) captured)
        : bank_(bank), captured_(std::move(captured))
    {
    }
    std::span<std::uint8_t> bytes(W offset, W size, W site) const
    {
        const auto *p = std::get_if<SourcePmdLocation>(&captured_);
        if (!p)
            throw SourceAllocationError(site, "4258A0 reached unrepresented static texture-motion bytes");
        const auto all = bank_.bytes(p->allocation);
        const auto at = std::uint64_t(p->offset) + offset;
        if (at > all.size() || size > all.size() - at)
            throw SourceAllocationError(site, "4258A0 texture-motion access leaves actual PMD allocation");
        return all.subspan(static_cast<std::size_t>(at), size);
    }
    W kind(W offset, W site) const
    {
        if (const auto *p = std::get_if<SourceSceneStaticPointer>(&captured_))
            return p->read<W>(offset); // actual represented bytes, not inferred sentinel
        const auto b = bytes(offset, 4, site);
        W value;
        std::memcpy(&value, b.data(), 4);
        return value;
    }
    W first_kind(const W *wave, float &oscillation) const
    {
        if (const auto *p = std::get_if<SourceSceneStaticPointer>(&captured_))
        {
            // This binding is immutable. A missing static byte is a reached
            // host boundary before oscillation; no invented sentinel.
            const W word = p->read<W>();
            return sgn_source_uv_first_4258de(wave, &word, &oscillation);
        }
        const auto first = bytes(0, 4, 0x4258eb);
        // Actual PMD kind read stays between FMUL and FSTP, as in4258EB/F0.
        return sgn_source_uv_first_4258de(wave, first.data(), &oscillation);
    }
};
} // namespace
void advance_source_texture_motion_425ad0(const std::shared_ptr<RaceSessionPlatform> &platform)
{
    if (!platform || !platform->source_construction_only_entry())
        throw SourceAllocationError(0x4258a0, "4258A0 requires the actual source platform");
    platform->require_owner_thread();
    auto &s = platform->state();
    if (s.frame_pause == 1)
        return; // 4258A6..AD, no motion pointer or phase reads
    const W old = s.texture_animation.phase;
    const W next = old + ((old >= 0x800u && old < 0x1800u) ? 0x10u : 0x20u);
    const MotionPointer pointer(
        platform->source_allocations(),
        platform->source_scene_resources().models().source_loading_state().track.texture_motion); // 4258CE
    s.texture_animation.phase = next & 0x1fffu;                                                   // 4258D4/D9
    W offset = 0, kind = pointer.first_kind(
                      &camera_detail::trig_bits[s.texture_animation.phase],
                      s.texture_animation.oscillation); // 4258DE..F0; owner failure retains phase, not oscillation
    while (kind != 0xffffffffu)
    {
        if (kind != 0)
        {
            // Complete admitted record extent, no payload copy. kind2 never
            // reads step_v; do not require the ordinary path's final DWORD.
            const auto record = pointer.bytes(offset, kind == 2 ? 16u : 20u, kind == 2 ? 0x425909u : 0x425942u);
            sgn_source_uv_record_425904(record.data(), &s.texture_animation.oscillation, kind);
        }
        if (offset > std::numeric_limits<W>::max() - 20u)
            throw SourceAllocationError(0x4259ca, "4258A0 motion pointer increment exceeds represented offset");
        kind = pointer.kind(offset + 20u, 0x4259ca); // read next BEFORE source ECX increment
        offset += 20u;
    }
    for (auto &sky : s.texture_animation.sky)
        sgn_source_uv_sky_4259e3(&sky.u);
}
} // namespace sgn::retail
