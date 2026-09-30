#pragma once
// Private output-only continuation. No gameplay producer is called here.
// Vehicle physics/presentation, fonts and remaining source gaps are separate
// required portions of the full normal-parent image, not implicit zero bytes.
#include "normal_session_frame_export.hpp"
#include <bit>

namespace normal_session_owner {
namespace race_export_detail {
inline constexpr std::uint32_t vehicles=0x49a1f8,vehicle_stride=0x7724;
inline constexpr std::uint32_t controllers=0x498248,controller_stride=0x74c;
inline constexpr std::uint32_t replays=0x4d5cc0,text_arena=0x60f6a0;
template<class T>void field(Image& image,NativeAddresses& addresses,
    const std::string& name,std::uint32_t at,const T& value){
    static_assert(std::is_trivially_copyable_v<T>);
    addresses.array(name,std::span(&value,1),at);
    image.create_live(name,at,std::span(&value,1));
}
inline void pointer(Image& image,const std::string& name,std::uint32_t at,
    std::uint32_t target,std::uint32_t extent){
    require(target||extent==0,"null race reference has an extent");
    if(target)image.alias(name+"/target",target,extent);
    const auto data=std::span(reinterpret_cast<const std::uint8_t*>(&target),4);
    image.create_encoded(name,at,data,{{0,4,Origin::relocation,name}});
}
inline std::uint32_t vehicle_address(unsigned slot){
    require(slot<VehicleStorage::physical_slots,"race reference outside physical vehicle bank");
    return add(vehicles,std::size_t(slot)*vehicle_stride);
}
}

// The eight actual format headers include the two presentation-only slots.
// This must precede Camera's +AC aliases. A prefix is not a whole7724-byte
// source record; unrepresented bytes remain absent from Image.
inline void export_vehicle_prefix_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    using namespace race_export_detail;
    const auto& bank=session.state().vehicles.configuration_identity;
    static_assert(sizeof(VehicleConfigurationIdentity)==32);
    for(unsigned slot=0;slot<bank.size();++slot)
        field(out,addresses,"race/vehicle/"+std::to_string(slot)+"/configuration-header",
            add(vehicles,std::size_t(slot)*vehicle_stride),bank[slot].header);
}

// Actual contiguous ReplayBank, including all retained inactive recordings.
// Cursor zero denotes the untouched null source word in this loaded-session
// domain. Nonzero cursors must name this actual bank; no synthetic frames.
inline void export_replay_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    using namespace race_export_detail;
    const auto& s=session.state();const auto bytes=s.replays.bytes();
    require(bytes.size()==ReplayBank::size,"actual replay bank extent");
    addresses.array("race/replay-bank",bytes,replays);
    //Headers now retain actual source words; no output-only rebasing.
    const auto owner=out.create_live("race/replay-bank",replays,bytes);
    for(unsigned i=0;i<ReplayBank::slots;++i){
        const auto target=word(bytes,i*ReplayBank::stride+0x10);
        require(target==0||(target>=replays+ReplayBank::header_bytes&&target<=replays+bytes.size()),
            "replay cursor outside observed bank");
        if(target){
            const auto offset=target-replays;
            require(addresses.address(bytes.data()+offset,0,offset==bytes.size())==target,"replay cursor native alias");
            out.alias_in("race/replay-cursor/"+std::to_string(i)+"/target",owner,offset,0);
        }
    }
}

inline void export_vehicle_race_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    using namespace race_export_detail;
    const auto& s=session.state();const auto& bank=s.vehicles;
    for(unsigned slot=0;slot<VehicleStorage::physical_slots;++slot){
        const auto base=vehicle_address(slot);const auto key="race/vehicle/"+std::to_string(slot)+"/";
        const auto live=[&](const char* name,std::uint32_t offset,const auto& value){field(out,addresses,key+name,add(base,offset),value);};
        const auto& r=bank.race[slot];
        live("active",0x1ba8,r.active);live("control-enabled",0x2278,r.control_enabled);live("control-kind",0x227c,r.control_kind);
        live("saved-camera-mode",0x22c8,r.saved_camera_mode);
        live("race-index",0x23a4,r.race_index);live("previous-progress",0x23a8,r.previous_progress);live("current-progress",0x23ac,r.current_progress);
        live("progress",0x23b0,r.progress);live("previous-fraction",0x23b8,r.previous_fraction);
        live("lap-index",0x23bc,r.lap_index);live("lap-timer",0x23c4,r.lap_timer);live("split-timer",0x23c8,r.split_timer);live("race-timer",0x23cc,r.race_timer);
        live("stunt-score",0x23d0,r.stunt_score);live("best-lap-ticks",0x23d4,r.best_lap_ticks);live("total-ticks",0x23d8,r.total_ticks);
        live("lap-times",0x23dc,r.lap_times);live("spline-index",0x24a4,r.spline_index);live("progress-crossing",0x24a8,r.progress_crossing);live("place",0x24ac,r.place);
        live("respawn-position",0x24b0,r.respawn_position);live("respawn-heading",0x24c0,r.respawn_heading);live("route-right",0x24d0,r.route_right);
        for(unsigned j=0;j<3;++j){
            field(out,addresses,key+"route-index/"+std::to_string(j),base+0x2644+j*8,r.route_indices[j]);
            field(out,addresses,key+"route-fraction/"+std::to_string(j),base+0x2648+j*8,r.route_fractions[j]);
        }
        live("finish-state",0x24e4,r.finish_state);live("stopped",0x24e8,r.stopped);live("timing26a8",0x26a8,r.timing26a8);
        live("event7678",0x7678,r.event7678);live("automatic-drive",0x767c,r.automatic_drive);live("start-countdown",0x7688,r.start_countdown);
        live("notice-active",0x76f0,r.notice_active);live("notice-ticks",0x76f4,r.notice_ticks);
        live("respawn-requested",0x763c,r.respawn_requested);live("collision-hold",0x7640,r.collision_hold);live("input-hold",0x7644,r.input_hold);live("pending-reset",0x7680,r.pending_reset);
        live("opposing-direction-ticks",0x7634,r.opposing_direction_ticks);live("unsafe-contact-ticks",0x7638,r.unsafe_contact_ticks);
        const auto target=r.recording?add(replays,ReplayBank::base(*r.recording)):0;
        if(r.recording)require(addresses.address(s.replays.bytes().data()+ReplayBank::base(*r.recording),ReplayBank::header_bytes)==target,"vehicle recording uses another bank");
        pointer(out,key+"recording",base+0x7268,target,target?ReplayBank::header_bytes:0);
        const auto& p=bank.progression[slot];
        live("previous-projection",0x23b4,p.previous_projection);live("record-improved",0x23c0,p.record_improved);live("lap-time24",0x243c,p.lap_time_24);
        live("lap-scores",0x2440,p.lap_scores);live("checkpoint-index",0x24ec,p.checkpoint_index);
        live("checkpoint-times",0x24f0,p.checkpoint_times);live("personal-checkpoint-times",0x2518,p.personal_checkpoint_times);live("record-checkpoint-times",0x2540,p.record_checkpoint_times);
        live("startup2280",0x2280,bank.startup[slot].field_2280);live("startup768c",0x768c,bank.startup[slot].fields_768c);
        const auto& camera=bank.camera_settings[slot];
        live("camera-orbit",0x22cc,camera.orbit_offset);live("camera-target",0x22dc,camera.target_offset);live("camera-angles",0x22ec,camera.angles);
        live("camera-distance",0x22fc,camera.distance);live("camera-approach-scale",0x2300,camera.approach_scale);live("camera-adaptive-distance",0x2304,camera.adaptive_distance);
    }
}

// HUD elements0 and6 borrow countdown/respawn fields. Their source words are
// never duplicated in the layout arrays. Controller holes218..21F/4A8..4AB
// remain absent. Notice +4C has an unresolved nullable identity representation;
// exporting a native slot0 as a source pointer would erase that distinction.
inline void export_controller_hud_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    using namespace race_export_detail;
    const auto& s=session.state();
    static_assert(sizeof(ControllerOverlayTransition)==40&&sizeof(HudElementLayout)==24);
    static_assert(std::is_standard_layout_v<contact::player::Notice>&&sizeof(contact::player::Notice)==84);
    static_assert(offsetof(contact::player::Notice,source_vehicle)==76&&offsetof(contact::player::Notice,audience)==80);
    for(unsigned slot=0;slot<s.controls.controllers.size();++slot){
        const auto base=add(controllers,slot*controller_stride);const auto key="race/hud/"+std::to_string(slot)+"/";
        field(out,addresses,key+"overlay",base+0x1e8,s.overlays[slot]);
        const auto& countdown=s.hud_countdown[slot];const auto& layout=s.hud_layout[slot];const auto& controller=s.controls.controllers[slot];
        field(out,addresses,key+"active",base+0x210,countdown.active);field(out,addresses,key+"ticks",base+0x214,countdown.ticks);
        for(unsigned element=0;element<18;++element){
            const auto at=base+0x220+element*36;const auto prefix=key+"element/"+std::to_string(element)+"/";const auto& item=layout.elements[element];
            const auto live=[&](const char* name,unsigned offset,const auto& value){field(out,addresses,prefix+name,at+offset,value);};
            live("x",0,item.x);live("y",4,item.y);
            if(element==0){live("active",8,countdown.enabled);live("value",12,countdown.value);live("phase",16,countdown.phase);}
            else if(element==6){live("active",8,controller.respawn_transition.state);live("value",12,layout.value[5]);live("phase",16,controller.respawn_transition.phase);}
            else{const auto index=element<6?element-1:element-2;live("active",8,layout.active[index]);live("value",12,layout.value[element-1]);live("phase",16,layout.phase[index]);}
            live("word14",20,item.word_14);live("word18",24,item.word_18);live("word1c",28,item.word_1c);live("word20",32,item.word_20);
        }
        for(unsigned i=0;i<controller.notices.size();++i){
            const auto& n=controller.notices[i];const auto prefix=key+"notice/"+std::to_string(i)+"/";const auto at=base+0x4ac+i*84;
            field(out,addresses,prefix+"remaining",at,n.remaining);field(out,addresses,prefix+"initial",at+4,n.initial);field(out,addresses,prefix+"tick",at+8,n.tick);
            field(out,addresses,prefix+"text",at+12,n.text);field(out,addresses,prefix+"audience",at+80,n.audience);
        }
    }
    require(s.selected_hud<4,"selected HUD outside actual controllers");
    const auto target=add(controllers,s.selected_hud*controller_stride+0x210);
    require(addresses.address(&s.hud_countdown[s.selected_hud].active,4)==target,"selected HUD aliases another owner");
    pointer(out,"race/selected-hud",0x61b740,target,4);
    field(out,addresses,"race/hud-margin-x",0x6244fc,s.hud_margins.x);field(out,addresses,"race/hud-margin-y",0x624500,s.hud_margins.y);
}

inline void export_race_clock_text_owners(RaceSession& session,Image& out,NativeAddresses& addresses){
    using namespace race_export_detail;
    const auto& s=session.state();
    const auto live=[&](const char* name,std::uint32_t at,const auto& value){field(out,addresses,std::string("race/")+name,at,value);};
    live("clock/game-mode",0x60f2b0,s.clock.game_mode);live("clock/phase",0x60f2b4,s.clock.phase);live("clock/countdown-pending",0x60f2c8,s.clock.countdown_pending);live("clock/rolling-state",0x60f2cc,s.clock.rolling_state);
    live("clock/race-mode",0x60eff0,s.clock.race_mode);live("clock/remaining",0x60eff4,s.clock.remaining_ticks);live("clock/laps",0x60eff8,s.clock.lap_count);live("clock/course-length",0x60f010,s.clock.course_length);
    live("record/previous-lap",0x60effc,s.records.previous_lap_ticks);live("record/lap",0x60f000,s.records.lap_ticks);live("record/score",0x60f008,s.records.score);
    live("tick/simulation",0x612cfc,s.simulation_tick);live("tick/scheduled",0x612d00,s.tick_clock.scheduled);live("tick/observed",0x612d04,s.tick_clock.observed);live("tick/increment",0x612d08,s.tick_clock.increment);
    live("ranking-count",0x612d10,s.ranking.count);require(s.ranking.count<=s.ranking.slots.size(),"actual ranking count exceeds physical slots");
    for(unsigned i=0;i<s.ranking.slots.size();++i){const auto slot=s.ranking.slots[i];pointer(out,"race/ranking/"+std::to_string(i),0x612d14+i*4,slot?vehicle_address(*slot):0,slot?4:0);}
    for(unsigned i=0;i<s.cars.size();++i){
        const auto key="race/car/"+std::to_string(i)+"/";const auto at=0x60f02c+i*100;const auto& car=s.cars[i];
        field(out,addresses,key+"model",at+8,car.model);field(out,addresses,key+"settings",at+12,car.settings);field(out,addresses,key+"name",at+36,car.name);
        // The first two native source_word values have no assigned pointer
        // semantics. They are not silently translated through a model table.
    }
    // At a completed normal-parent boundary this cursor names a physical car.
    // A between-loop value6 is a different source domain and rejected here.
    const auto current=vehicle_address(s.current_slot);
    require(addresses.address(s.vehicles.configuration_identity[s.current_slot].header.data(),4)==current,"physical cursor has no actual prefix");
    pointer(out,"race/current-vehicle",0x4d5bb8,current,4);
    live("text/arena",text_arena,s.game_text.arena);live("text/remaining",0x4835dc,s.game_text.remaining_bytes);live("text/language",0x612944,s.game_text.language);
    // The descriptor-order table at60F350 is a separate source structure;
    // do not write native order indices into its pointer/descriptor records.
    for(unsigned i=0;i<s.game_text.offsets.size();++i){
        const auto offset=s.game_text.offsets[i];std::uint32_t target=0;
        if(offset){require(*offset<s.game_text.arena.size(),"text handle outside actual arena");target=addresses.address(s.game_text.arena.data()+*offset,1);require(target==add(text_arena,*offset),"text handle changed owner");}
        pointer(out,"race/text/handle/"+std::to_string(i),0x6127a0+i*4,target,target?1:0);
    }
}
}
