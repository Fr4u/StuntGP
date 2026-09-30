#pragma once
// Independent raw observations: no exporter, retail-offset map, source address
// helper, reset, or game logic. Every ref borrows the actual Session member.
#include "normal_session_native_observer.hpp"

namespace normal_native_observer {
inline void capture_vehicle_audio_owners(RaceSession& session,
    const std::filesystem::path& output) {
    const auto& bank = session.state().vehicles.audio;
    Observations out;
    out.value("bank/address", reinterpret_cast<std::uint64_t>(bank.data()));
    out.value("bank/count", static_cast<std::uint32_t>(bank.size()));
    out.value("bank/native-stride", static_cast<std::uint32_t>(sizeof(bank[0])));
    for (std::size_t slot = 0; slot < bank.size(); ++slot) {
        const auto key = "vehicle/" + std::to_string(slot) + "/";
        const auto& audio = bank[slot];
        const auto raw = [&](const char* member, const auto& value) { out.ref(key + member, value); };
        raw("horn_handle", audio.horn_handle);
        raw("fields_233c", audio.fields_233c);
        raw("effect_hold", audio.effect_hold);
        raw("field_2350", audio.field_2350);
        raw("effect_pending", audio.effect_pending);
        raw("field_2358", audio.field_2358);
        raw("effect_strength", audio.effect_strength);
        raw("impact_handle", audio.impact_handle);
        raw("previous_wheel_touched", audio.previous_wheel_touched);
        raw("grip_handle", audio.grip_handle);
        raw("scrape_handle", audio.scrape_handle);
        raw("rolling_handle", audio.rolling_handle);
        raw("boost_handle", audio.boost_handle);
        raw("idle_handle", audio.idle_handle);
        raw("rev_handle", audio.rev_handle);
        raw("motion_handle", audio.motion_handle);
        raw("field_7718", audio.field_7718);
        raw("field_771c", audio.field_771c);
        raw("field_7720", audio.field_7720);
    }
    out.save(output);
}
}
