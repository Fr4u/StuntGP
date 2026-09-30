#pragma once
// Raw observations independent of source offsets, pointer normalization and
// the race exporter. No state is repaired, refreshed or copied into Session.
#include "normal_session_native_observer.hpp"

namespace normal_native_observer {
inline void capture_race_owners(RaceSession& session,const std::filesystem::path& output){
    const auto& s=session.state();const auto& bank=s.vehicles;Observations out;
    for(unsigned slot=0;slot<bank.configuration_identity.size();++slot)
        out.ref("vehicle/"+std::to_string(slot)+"/configuration",bank.configuration_identity[slot].header);
    out.span("replay-bank",s.replays.bytes());
    for(unsigned slot=0;slot<bank.race.size();++slot){
        const auto key="vehicle/"+std::to_string(slot)+"/";
        const auto raw=[&](const char* name,const auto& value){out.ref(key+name,value);};
        const auto& r=bank.race[slot];
        raw("active",r.active);raw("control_enabled",r.control_enabled);raw("control_kind",r.control_kind);raw("saved_camera_mode",r.saved_camera_mode);
        raw("race_index",r.race_index);raw("previous_progress",r.previous_progress);raw("current_progress",r.current_progress);
        raw("progress",r.progress);raw("previous_fraction",r.previous_fraction);raw("lap_index",r.lap_index);
        raw("lap_timer",r.lap_timer);raw("split_timer",r.split_timer);raw("race_timer",r.race_timer);
        raw("stunt_score",r.stunt_score);raw("best_lap_ticks",r.best_lap_ticks);raw("total_ticks",r.total_ticks);raw("lap_times",r.lap_times);
        raw("spline_index",r.spline_index);raw("progress_crossing",r.progress_crossing);raw("place",r.place);
        raw("respawn_position",r.respawn_position);raw("respawn_heading",r.respawn_heading);raw("route_right",r.route_right);
        raw("route_indices",r.route_indices);raw("route_fractions",r.route_fractions);
        raw("finish_state",r.finish_state);raw("stopped",r.stopped);raw("timing26a8",r.timing26a8);
        raw("event7678",r.event7678);raw("automatic_drive",r.automatic_drive);raw("start_countdown",r.start_countdown);
        raw("notice_active",r.notice_active);raw("notice_ticks",r.notice_ticks);raw("respawn_requested",r.respawn_requested);
        raw("collision_hold",r.collision_hold);raw("input_hold",r.input_hold);raw("pending_reset",r.pending_reset);
        raw("opposing_direction_ticks",r.opposing_direction_ticks);raw("unsafe_contact_ticks",r.unsafe_contact_ticks);
        out.value(key+"recording/present",std::uint32_t(r.recording.has_value()));out.value(key+"recording/slot",r.recording?r.recording->slot:0u);
        const auto& p=bank.progression[slot];
        raw("previous_projection",p.previous_projection);raw("record_improved",p.record_improved);raw("lap_time_24",p.lap_time_24);raw("lap_scores",p.lap_scores);
        raw("checkpoint_index",p.checkpoint_index);raw("checkpoint_times",p.checkpoint_times);raw("personal_checkpoint_times",p.personal_checkpoint_times);raw("record_checkpoint_times",p.record_checkpoint_times);
        raw("startup_2280",bank.startup[slot].field_2280);raw("startup_768c",bank.startup[slot].fields_768c);
        const auto& camera=bank.camera_settings[slot];
        raw("camera/orbit",camera.orbit_offset);raw("camera/target",camera.target_offset);raw("camera/angles",camera.angles);
        raw("camera/distance",camera.distance);raw("camera/approach_scale",camera.approach_scale);raw("camera/adaptive_distance",camera.adaptive_distance);
    }
    for(unsigned slot=0;slot<s.controls.controllers.size();++slot){
        const auto key="hud/"+std::to_string(slot)+"/";const auto& layout=s.hud_layout[slot];const auto& countdown=s.hud_countdown[slot];const auto& controller=s.controls.controllers[slot];
        out.ref(key+"overlay",s.overlays[slot]);out.ref(key+"countdown_active",countdown.active);out.ref(key+"countdown_ticks",countdown.ticks);
        out.ref(key+"countdown_enabled",countdown.enabled);out.ref(key+"countdown_value",countdown.value);out.ref(key+"countdown_phase",countdown.phase);
        out.ref(key+"elements",layout.elements);out.ref(key+"active",layout.active);out.ref(key+"value",layout.value);out.ref(key+"phase",layout.phase);
        out.ref(key+"respawn_state",controller.respawn_transition.state);out.ref(key+"respawn_phase",controller.respawn_transition.phase);
        for(unsigned i=0;i<controller.notices.size();++i){
            const auto prefix=key+"notice/"+std::to_string(i)+"/";const auto& n=controller.notices[i];
            out.ref(prefix+"remaining",n.remaining);out.ref(prefix+"initial",n.initial);out.ref(prefix+"tick",n.tick);out.ref(prefix+"text",n.text);
            // Observed even while the source null-versus-slot0 mapping remains
            // unresolved. A decoder must label this gap, not compare a guess.
            out.ref(prefix+"source_vehicle",n.source_vehicle);out.ref(prefix+"audience",n.audience);
        }
    }
    out.ref("selected_hud",s.selected_hud);out.ref("hud_margin_x",s.hud_margins.x);out.ref("hud_margin_y",s.hud_margins.y);
    out.ref("clock/game_mode",s.clock.game_mode);out.ref("clock/phase",s.clock.phase);out.ref("clock/countdown_pending",s.clock.countdown_pending);out.ref("clock/rolling_state",s.clock.rolling_state);
    out.ref("clock/race_mode",s.clock.race_mode);out.ref("clock/remaining_ticks",s.clock.remaining_ticks);out.ref("clock/lap_count",s.clock.lap_count);out.ref("clock/course_length",s.clock.course_length);
    out.ref("record/previous_lap_ticks",s.records.previous_lap_ticks);out.ref("record/lap_ticks",s.records.lap_ticks);out.ref("record/score",s.records.score);
    out.ref("tick/simulation",s.simulation_tick);out.ref("tick/scheduled",s.tick_clock.scheduled);out.ref("tick/observed",s.tick_clock.observed);out.ref("tick/increment",s.tick_clock.increment);
    out.ref("ranking/count",s.ranking.count);
    for(unsigned i=0;i<s.ranking.slots.size();++i){const auto key="ranking/"+std::to_string(i)+"/";const auto slot=s.ranking.slots[i];out.value(key+"present",std::uint32_t(slot.has_value()));out.value(key+"slot",slot.value_or(0));}
    for(unsigned i=0;i<s.cars.size();++i){
        const auto key="car/"+std::to_string(i)+"/";const auto& car=s.cars[i];
        out.ref(key+"word00_unmapped",car.source_word00);out.ref(key+"word04_unmapped",car.source_word04);
        out.ref(key+"model",car.model);out.ref(key+"settings",car.settings);out.ref(key+"name",car.name);
    }
    out.ref("current_vehicle",s.current_slot);
    out.ref("text/arena",s.game_text.arena);out.ref("text/remaining",s.game_text.remaining_bytes);out.ref("text/language",s.game_text.language);
    out.ref("text/descriptor_order_unmapped",s.game_text.descriptor_order);
    for(unsigned i=0;i<s.game_text.offsets.size();++i){
        const auto key="text/handle/"+std::to_string(i)+"/";const auto offset=s.game_text.offsets[i];
        out.value(key+"present",std::uint32_t(offset.has_value()));out.value(key+"offset",offset.value_or(0));
        if(offset&&*offset>=s.game_text.arena.size())throw std::runtime_error("raw text observation outside native arena");
        out.value(key+"address",offset?reinterpret_cast<std::uint64_t>(s.game_text.arena.data()+*offset):std::uint64_t{0});
    }
    out.save(output);
}
}
