#include "sgn/source_score_popup_update.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/source_menu_sample_native.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace sgn::retail
{
// Native64 host ABI only. The assembler keeps the source x87 operations,
// store widths, comparison flags and __ftol control-word save/restore.
extern "C" void sgn_source_score_prefix_40179a(VehicleScorePopup *, const Vec3 *);
extern "C" std::uint32_t sgn_source_score_overlay_401a10(VehicleScorePopup *, float *xy);
extern "C" void sgn_source_score_tail_401a93(VehicleScorePopup *);
extern "C" void sgn_source_score_sparkle_401064(VehicleScorePopup *, std::uint32_t *random_state, const float *xy);
namespace
{
using Popup = VehicleScorePopup;
static_assert(sizeof(Popup) == 0x138);
static_assert(offsetof(Popup, active) == 0 && offsetof(Popup, variant) == 8);
static_assert(offsetof(Popup, flags) == 0x10 && offsetof(Popup, offset) == 0x14);
static_assert(offsetof(Popup, growth) == 0x24 && offsetof(Popup, age) == 0x28);
static_assert(offsetof(Popup, lifetime) == 0x2c && offsetof(Popup, points) == 0x34);
static_assert(offsetof(Popup, enabled) == 0x40 && offsetof(Popup, scale) == 0x44);
static_assert(offsetof(Popup, angle) == 0x48 && offsetof(Popup, depth) == 0x4c);
static_assert(offsetof(Popup, reserved_50) == 0x50 && offsetof(Popup, color_animation) == 0x54);
static_assert(offsetof(Popup, reserved_60) == 0x60 && offsetof(Popup, motion) == 0x64);
static_assert(offsetof(Popup, motion_flags) == 0x70 && offsetof(Popup, text_size) == 0x74);
static_assert(offsetof(Popup, colors) == 0x7c && offsetof(Popup, font) == 0x88);
static_assert(offsetof(Popup, alignment) == 0x8c && offsetof(Popup, style) == 0x90);
static_assert(offsetof(Popup, reserved_94) == 0x94 && offsetof(Popup, overlay_enabled) == 0x98);
static_assert(offsetof(Popup, overlay_age) == 0x9c && offsetof(Popup, overlay_size) == 0xa0);
static_assert(offsetof(Popup, presentation_flags) == 0xac && offsetof(Popup, position) == 0xb0);
static_assert(offsetof(Popup, reserved_b8) == 0xb8 && offsetof(Popup, dimensions) == 0xbc);
static_assert(offsetof(Popup, texture) == 0xd4 && offsetof(Popup, presentation_lifetime) == 0xd8);
static_assert(offsetof(Popup, presentation_x) == 0xdc && offsetof(Popup, presentation_y) == 0xe0);
static_assert(offsetof(Popup, presentation_size) == 0xe4 && offsetof(Popup, presentation_scale) == 0xec);
static_assert(offsetof(Popup, reserved_f4) == 0xf4);
static_assert(sizeof(Vec3) == 12 && offsetof(Vec3, y) == 4 && offsetof(Vec3, z) == 8);
void delayed_audio_4011f0(VehicleScoreState &state, FrontendState &frontend, SourceMenuSampleNative &samples)
{
    const auto delay = state.sound_class; // 4011F0, retained before decrement
    if (delay == 0)
        return;
    const auto remaining = delay - 1u;
    state.sound_class = remaining; // 4011FA, before the synchronous461150 child
    if (remaining != 0)
        return;
    const auto volume = state.sound_volume; // 401201
    const auto sample = state.sound_id;     // 401206
    samples.start_461150(frontend, sample, 0, 0, volume, -255, 0, nullptr);
}
void sparkle_401030(RaceSessionState &world, const std::array<float, 2> &xy)
{
    // 40103F/40105F skip only active==1, first of the actual16 shared slots.
    // No clearing: omitted fields, including offset.Z and colors, survive.
    for (auto &slot : world.score_popups.popups)
    {
        if (slot.active == 1)
            continue;
        sgn_source_score_sparkle_401064(&slot, &world.random.state, xy.data());
        return;
    }
}
} // namespace

void advance_source_score_popups_401770(FrontendState &frontend, SourceMenuSampleNative &samples)
{
    const auto platform = frontend.shared_platform();
    if (!platform)
        throw std::logic_error("source401770 requires the actual frontend platform");
    platform->require_owner_thread();
    if (!platform->source_construction_only_entry() || &frontend.world() != &platform->state())
        throw std::logic_error("source401770 requires the same source platform/frontend world");
    auto &world = platform->state();
    auto &state = world.score_popups;
    delayed_audio_4011f0(state, frontend, samples); // 401779; no popup reads before callback
    for (auto &popup : state.popups)
    {
        if (popup.active != 1)
            continue; // 401791, reread each slot on arrival
        sgn_source_score_prefix_40179a(&popup, &world.gravity.vector);
        const auto age = popup.age;             // 4019F1
        const auto alignment = popup.alignment; // 4019F4
        if (age == alignment)
        {
            const auto volume = popup.style;          // 4019FB, +90
            const auto frequency = popup.reserved_94; // 4019FE, +94
            const auto sample = popup.font;           // 401A06, +88
            samples.start_461150(frontend, sample, 0, frequency, volume, 0, 1, nullptr);
        }
        // The callback may change any popup, gravity or RNG. No age/lifetime,
        // enable or animation value from before461150 is reused below.
        std::array<float, 2> xy; // written by401A73..401A87 only when return==1
        if (sgn_source_score_overlay_401a10(&popup, xy.data()) == 1)
        {
            sparkle_401030(world, xy); // same bank; later slots update in this invocation
        }
        // 401A93 reloads after401030, including when a callback made the current
        // slot free and401030 reused it. Do not restore the old popup fields.
        sgn_source_score_tail_401a93(&popup);
    }
}
} // namespace sgn::retail
