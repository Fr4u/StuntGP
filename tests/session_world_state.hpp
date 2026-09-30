#pragma once
#include "vehicle_tick_full_state.hpp"
#include "audio_initialize_state.hpp"
#include "race_initialization_state.hpp"
#include "session_camera_source_capture.hpp"
#include <limits>

// Qualification-only, output-only serialization. Session is constructed from
// real assets before this object exists. No bytes are imported into the game.
namespace sgn::retail::session_world_test {
using namespace whole_tick_test;
inline unsigned narrow(std::size_t n){if(n>std::numeric_limits<unsigned>::max())throw FormatError("session capture size");return static_cast<unsigned>(n);}
struct Snapshot {Bytes bytes,defined;};
struct Capture {
    RaceSession& session;
    std::vector<Region> regions;
    std::vector<Bytes> authored;
    unsigned size=global_size;
    std::unique_ptr<StateCodec> codec;
    bool impact_list_published{};
    explicit Capture(RaceSession& owner,const std::filesystem::path& root):session(owner){
        regions.push_back({0,0,global_base,global_size,0,0,"globals"});
        const auto track_handle=session.scene_resources().model_handle(3);
        if(!track_handle)throw FormatError("actual session has no track model");
        for(unsigned i=0;i<session.models().size();++i){
            const std::string name(session.models().source_name(i));authored.push_back(read_file(root/"meshdata"/(name+".PMD")));
            const auto& raw=authored.back();const unsigned address=i==*track_handle?0x11400000u:0x60000000u+i*0x200000u;
            if(raw.size()>=0x200000&&i!=*track_handle)throw FormatError("capture model reservation too small");
            const auto descriptor=address+328+get(raw,0x4c);
            regions.push_back({1,i,address,narrow(raw.size()),descriptor,size,name+".PMD"});size+=narrow(raw.size());
        }
        constexpr std::array<unsigned,6> sizes{1024,1024,8192,8192,64,104};
        for(unsigned i=0;i<sizes.size();++i){regions.push_back({2,i,0x70000000u+i*0x10000u,sizes[i],0,size,"strip"+std::to_string(i)});size+=sizes[i];}
        regions.push_back({3,0,session_camera_test::light_vertices_base,narrow(session.state().light_vertices.size()),0,size,"light_vertices"});size+=narrow(session.state().light_vertices.size());
        codec=std::make_unique<StateCodec>(session.state(),session.models(),session.splines(),session.track_pmd(),regions);
    }
    unsigned offset(unsigned address,std::size_t count)const{
        for(const auto& r:regions)if(address>=r.address&&std::uint64_t(address)-r.address+count<=r.length)return r.offset+address-r.address;
        throw FormatError("session capture source address outside registered regions");
    }
    template<class T>void field(Bytes& b,unsigned address,const T& value)const{
        static_assert(std::is_trivially_copyable_v<T>);std::memcpy(b.data()+offset(address,sizeof(value)),&value,sizeof(value));
    }
    void blob(Bytes& b,unsigned address,std::span<const std::uint8_t> value)const{
        std::memcpy(b.data()+offset(address,value.size()),value.data(),value.size());
    }
    void supplement(Bytes& b){
        auto& s=session.state();
        field(b,0x474630,car_base+s.load_selection.template_vehicle*car_stride);field(b,0x472c74,s.load_selection.configuration_id);
        field(b,0x61b740,controller_base+s.selected_hud*0x74c+0x210);field(b,0x60f020,s.selected_camera_preset);
        field(b,0x60d678,s.trial_start.reference_at_start);field(b,0x60d67c,s.trial_start.announcement_ticks);field(b,0x60f2c4,s.trial_start.phase_ticks);
        field(b,0x60d754,s.trial_selection.split_selection);field(b,0x60d758,s.other_selected_vehicles);
        field(b,0x612be8,s.scene_mode);field(b,0x61294c,s.frame_pause);field(b,0x612960,s.render_b);
        field(b,0x612990,s.demo_ticks);field(b,0x60f018,s.control_kind);
        field(b,0x60f6a0,s.game_text.arena);field(b,0x4835dc,s.game_text.remaining_bytes);field(b,0x612944,s.game_text.language);
        for(unsigned i=0;i<s.game_text.offsets.size();++i){const auto text=s.game_text.offsets[i];
            if(text&&*text>=s.game_text.arena.size())throw FormatError("capture game text offset");field(b,0x6127a0+i*4,text?0x60f6a0u+*text:0u);}
        if(!s.track_part_animations)throw FormatError("capture missing actual animation bank");
        const auto records=s.track_part_animations->records();blob(b,0x576cd0,{reinterpret_cast<const std::uint8_t*>(records.data()),records.size_bytes()});
        for(unsigned i=2;i<16;++i){const auto handle=session.scene_resources().model_handle(i);
            field(b,0x636418+i*16,handle?codec->model_address(session.models().identity(handle)):0u);}
        field(b,0x636428,codec->strip(5).address);
        for(unsigned i=0;i<6;++i)field(b,0x571300+i*4,codec->strip(i).address);
        for(unsigned i=0;i<4;++i){const unsigned at=controller_base+i*0x74c;
            field(b,at+0x1e8,s.overlays[i]);field(b,at+0x300,s.controls.controllers[i].respawn_transition.state);
            field(b,at+0x308,s.controls.controllers[i].respawn_transition.phase);
        }
        // Only actual HUD offsets. The legacy camera diagnostic bytes200+
        // describe extra snapshots, not this original controller allocation.
        race_initialization_test::Controllers hud;
        const auto ca=offset(controller_base,hud.size());std::copy_n(b.begin()+ca,hud.size(),hud.begin());
        race_initialization_test::controllers(hud,{s.controls,s.hud_countdown,s.hud_layout,s.selected_hud,s.hud_margins},false);
        for(unsigned i=0;i<4;++i)std::copy_n(hud.begin()+i*0x74c+0x210,0x29c,b.begin()+ca+i*0x74c+0x210);
        if(impact_list_published){
            using Codec=contact::effects::ImpactParticleDrawList;
            const auto cursor=Codec::encode_cursor(s.impact_draw_list.count);
            // Reset defines all32 NULL slots. Normal publication retains the
            // previous uncaptured suffix, as this serializer always did.
            const auto captured=s.impact_draw_list.cursor_is_null()?32u:s.impact_draw_list.count;
            for(unsigned i=0;i<captured;++i)field(b,0x61b5a4+i*4,Codec::encode_slot(s.impact_draw_list.slots[i]));
            field(b,0x61b624,cursor);
        }
        // Use the accepted field serializer, preserving its uncaptured bytes.
        // Native audio IDs are exported as explicit tokens; the source COM
        // bridge must translate them before original code dereferences them.
        namespace at=sgn::test::audio_initialize;
        at::Record audio;
        std::memcpy(audio.data(),b.data()+offset(0x62df00,0x16c0),0x16c0);
        std::memcpy(reinterpret_cast<std::uint8_t*>(audio.data())+0x1700,b.data()+offset(0x489790,0x250),0x250);
        at::transfer(audio,s.audio_driver,s.audio_initialization,s.audio_maintenance,s.audio_stream,s.audio_level,s.options,false);
        blob(b,0x62df00,{reinterpret_cast<const std::uint8_t*>(audio.data()),0x16c0});
        blob(b,0x489790,{reinterpret_cast<const std::uint8_t*>(audio.data())+0x1700,0x250});
        for(unsigned i=0;i<72;++i)field(b,0x4899e0+i*12+8,s.audio_initialization.descriptor_types[i]);
        field(b,0x60d734,s.options.audio_a);field(b,0x60d738,s.options.audio_b);
        Bytes camera_globals(b.begin(),b.begin()+global_size);
        const auto camera=session_camera_test::capture_session_camera_source(s,session.splines(),session.track_pmd(),camera_globals);
        std::copy(camera_globals.begin(),camera_globals.end(),b.begin());
        for(const auto& extra:camera.extra_blocks)blob(b,extra.address,extra.bytes);
    }
    Bytes encode(std::uint8_t fill){
        Bytes b(size,fill);
        // The existing codec supports no external contact objects. Actual
        // Session has none; this precondition does not load a native field.
        for(unsigned i=0;i<6;++i)put(b,car_base-global_base+i*car_stride+0x726c,0);
        try{codec->transfer(b,false);}catch(const FormatError& error){
            const auto& s=session.state();std::string detail=error.what();
            const auto id=[&](const std::string& name,unsigned value){if(value>=6)detail+=" "+name+"="+std::to_string(value);};
            id("current",s.current_slot);
            for(unsigned i=0;i<4;++i){id("controller"+std::to_string(i),s.controls.controllers[i].vehicle);id("ghost"+std::to_string(i),s.ghosts.roles[i].vehicle);
                for(unsigned j=0;j<8;++j)id("notice"+std::to_string(i)+"/"+std::to_string(j),s.controls.controllers[i].notices[j].source_vehicle);}
            for(unsigned i=0;i<16;++i)id("popup"+std::to_string(i),s.score_popups.popups[i].vehicle);
            throw FormatError(detail);
        }
        for(const auto& r:regions)if(r.kind==1){
            auto& model=session.models().at(r.index);const auto& raw=authored.at(r.index);
            for(const auto& [at,value]:tick_test::capture_model_visual(model,raw))put(b,r.offset+at,value);
            const auto descriptor=r.offset+r.descriptor-r.address;
            put(b,descriptor+0x64,model.previous_detail_level());
            const auto flags=model.source_model_flags_02(),ambience=model.source_model_ambience();
            std::memcpy(b.data()+descriptor+2,&flags,2);std::memcpy(b.data()+descriptor+4,&ambience,2);
            std::memcpy(b.data()+descriptor+0x10,&model.source_model_vector_10(),16);
            const auto attributes=model.source_attribute_table();std::memcpy(b.data()+r.offset+328+get(raw,0x24),attributes.data(),attributes.size_bytes());
        }
        for(unsigned i=0;i<6;++i)if(const auto model=session.state().vehicles.models[i].model)
            field(b,codec->model_address(session.models().identity(model))+0x5c,session.state().selected_lod[i]);
        supplement(b);return b;
    }
    Snapshot snapshot(){
        auto a=encode(0xa5),b=encode(0x5a);Bytes mask(size);
        for(unsigned i=0;i<size;++i){mask[i]=a[i]==b[i]?1:0;if(!mask[i])a[i]=0;}
        return {std::move(a),std::move(mask)};
    }
};
}
