#pragma once
// Output-only fields from the six actual Session audio owners. Source evidence:
// reports/audio-work/series6/normal-session-vehicle-audio-codec-v1/HANDOFF.json.
// No game producer, reset, handle normalization, or whole-vehicle allocation.
#include "normal_session_race_export.hpp"
#include <cstddef>
#include <limits>

namespace normal_session_owner {
namespace vehicle_audio_export_detail {
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(std::uint32_t) == 4 && sizeof(float) == 4);
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(std::is_standard_layout_v<VehicleAudioState>);
static_assert(std::is_trivially_copyable_v<VehicleAudioState>);
static_assert(sizeof(VehicleAudioState) == 88);
static_assert(VehicleStorage::physical_slots == 6);
// These are native ABI guards, not retail offsets. The independent reader
// checks raw observed addresses against this complete, padding-free layout.
static_assert(offsetof(VehicleAudioState, horn_handle) == 0);
static_assert(offsetof(VehicleAudioState, fields_233c) == 4);
static_assert(offsetof(VehicleAudioState, effect_hold) == 20);
static_assert(offsetof(VehicleAudioState, field_2350) == 24);
static_assert(offsetof(VehicleAudioState, effect_pending) == 28);
static_assert(offsetof(VehicleAudioState, field_2358) == 32);
static_assert(offsetof(VehicleAudioState, effect_strength) == 36);
static_assert(offsetof(VehicleAudioState, impact_handle) == 40);
static_assert(offsetof(VehicleAudioState, previous_wheel_touched) == 44);
static_assert(offsetof(VehicleAudioState, grip_handle) == 48);
static_assert(offsetof(VehicleAudioState, scrape_handle) == 52);
static_assert(offsetof(VehicleAudioState, rolling_handle) == 56);
static_assert(offsetof(VehicleAudioState, boost_handle) == 60);
static_assert(offsetof(VehicleAudioState, idle_handle) == 64);
static_assert(offsetof(VehicleAudioState, rev_handle) == 68);
static_assert(offsetof(VehicleAudioState, motion_handle) == 72);
static_assert(offsetof(VehicleAudioState, field_7718) == 76);
static_assert(offsetof(VehicleAudioState, field_771c) == 80);
static_assert(offsetof(VehicleAudioState, field_7720) == 84);
}

inline void export_vehicle_audio_owners(RaceSession& session, Image& out,
    NativeAddresses& addresses) {
    const auto& bank = session.state().vehicles.audio;
    require(bank.size() == 6, "actual Session vehicle audio bank extent");
    for (unsigned slot = 0; slot < bank.size(); ++slot) {
        const auto base = race_export_detail::vehicle_address(slot);
        const auto key = "vehicle-audio/vehicle/" + std::to_string(slot) + "/";
        const auto& audio = bank[slot];
        const auto live = [&](const char* member, std::uint32_t offset, const auto& value) {
            const auto name = key + member;
            const auto guest = add(base, offset);
            race_export_detail::field(out, addresses, name, guest, value);
            // Resolve the address of the actual member through the registered
            // native span. This alias cannot point to a staged record.
            out.alias(name + "/actualNative", addresses.address(&value, sizeof(value)), narrow(sizeof(value)));
        };
        live("horn_handle", 0x2338, audio.horn_handle);
        live("fields_233c", 0x233c, audio.fields_233c);
        live("effect_hold", 0x234c, audio.effect_hold);
        live("field_2350", 0x2350, audio.field_2350);
        live("effect_pending", 0x2354, audio.effect_pending);
        live("field_2358", 0x2358, audio.field_2358);
        live("effect_strength", 0x235c, audio.effect_strength);
        live("impact_handle", 0x2370, audio.impact_handle);
        live("previous_wheel_touched", 0x2374, audio.previous_wheel_touched);
        live("grip_handle", 0x237c, audio.grip_handle);
        live("scrape_handle", 0x2380, audio.scrape_handle);
        live("rolling_handle", 0x2384, audio.rolling_handle);
        live("boost_handle", 0x2388, audio.boost_handle);
        live("idle_handle", 0x238c, audio.idle_handle);
        live("rev_handle", 0x2390, audio.rev_handle);
        live("motion_handle", 0x239c, audio.motion_handle);
        live("field_7718", 0x7718, audio.field_7718);
        live("field_771c", 0x771c, audio.field_771c);
        live("field_7720", 0x7720, audio.field_7720);
    }
}
}
