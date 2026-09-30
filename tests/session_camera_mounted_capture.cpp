#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "retail_camera_lifecycle_state.hpp"
#include <exception>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
namespace codec=sgn::retail::camera_test;
void require(bool ok,const char* why){if(!ok)throw FormatError(why);}
void write(std::ofstream& f,const auto& value){f.write(reinterpret_cast<const char*>(&value),sizeof(value));require(bool(f),"mounted capture write");}
void bytes(std::ofstream& f,const codec::Bytes& value){f.write(reinterpret_cast<const char*>(value.data()),static_cast<std::streamsize>(value.size()));require(bool(f),"mounted capture payload");}
void string(std::ofstream& f,std::string_view value){write(f,static_cast<unsigned>(value.size()));f.write(value.data(),static_cast<std::streamsize>(value.size()));require(bool(f),"mounted capture name");}
class Images final:public TextureReplacementDevice {
public:std::uint16_t destination_width(unsigned)const override{return 256;}
void upload_to_destination(unsigned,const Image&)override{}
};
constexpr unsigned camera_base=0x498248,camera_stride=0x74c,vehicle_base=0x10010000,descriptor=0x10400000;
codec::Bytes cameras(const RaceSessionState& s){
    codec::Bytes result;
    for(unsigned i=0;i<4;++i){
        auto camera=s.controls.controllers[i].camera;auto life=s.camera_lifecycle[i];CameraVehicle unused;
        codec::Bytes c(codec::camera_size),v(codec::vehicle_size);codec::transfer(c,v,camera,unused,false);codec::lifecycle(c,life,false);
        codec::put(c,0,s.controls.controllers[i].active);codec::put(c,0xac,vehicle_base);
        // This corpus exercises pending-record/lens guards only; never encode a
        // native optional index as if it were an original source pointer.
        if(s.controls.controllers[i].active==1)require(!life.pending_record&&camera.pending_lens==0,"active mounted Session capture requires no pending record/lens");
        codec::put(c,0x1cc,life.current_record?0x4722d8+*life.current_record*64:0);
        codec::put(c,0x1d8,life.pending_record?0x4722d8+*life.pending_record*64:0);
        codec::put(c,0x4c,camera.world_to_camera_reference?0x61c7cc+camera.world_to_camera_reference-2:0);
        codec::put(c,0x50,camera.camera_to_world_reference?0x61c7cc+camera.camera_to_world_reference-2:0);
        result.insert(result.end(),c.begin(),c.end());
    }
    return result;
}
struct Audit {
    RaceSession& session;std::ofstream& file;unsigned target{},frames{};std::exception_ptr error;
    codec::Bytes previous;std::uint64_t retained{},moving{},nonzero_lod{},live_axes_changed{};
    ModelTransform last_pose{};VehicleVector last_position{};
    Audit(RaceSession& s,std::ofstream& f,unsigned t):session(s),file(f),target(t){}
    void before()noexcept{try{
        auto& s=session.state();require(s.controls.controllers[0].active==1&&s.controls.controllers[0].vehicle==target,"mounted active target changed");
        for(unsigned i=1;i<4;++i)require(s.controls.controllers[i].active!=1,"unexpected additional actual player");
        require(s.controls.controllers[0].camera.mode==6,"actual mounted selection lost before405210");
        auto world=camera_frame_world(session);require(world.models==&session.models(),"actual mounted binder lost model owner");
        const auto c=cameras(s);
        if(!previous.empty()){
            // 402E90 updates derived matrices/side/lens outside405210. The
            // mounted eye/target/up remain authoritative across the two calls.
            for(const auto offset:{8u,0x28u,0x38u})require(std::memcmp(c.data()+offset,previous.data()+offset,16)==0,"mounted continuation was reseeded");
            ++retained;
        }
        auto observed=s.vehicles.camera_snapshot(target);CameraState dummy;
        codec::Bytes v(codec::vehicle_size),unused(codec::camera_size);codec::transfer(unused,v,dummy,observed,false);codec::put(v,0x26b0,descriptor);
        const auto handle=s.vehicles.models[target].model;require(handle.has_value(),"mounted actual model handle missing");
        auto& model=session.models().at(*handle);const auto& pose=model.part(model.source_lod_index(0),0).transform;
        if(frames){if(std::memcmp(&pose,&last_pose,80)!=0)++live_axes_changed;if(std::memcmp(&s.vehicles.physics[target].position,&last_position,16)!=0)++moving;}
        if(s.selected_lod[target]!=0)++nonzero_lod;
        last_pose=pose;last_position=s.vehicles.physics[target].position;
        write(file,s.simulation_tick);write(file,s.controls.current);write(file,s.selected_lod[target]);bytes(file,c);bytes(file,v);write(file,pose);
    }catch(...){error=std::current_exception();}}
    void after()noexcept{try{
        if(error)return;auto& s=session.state();previous=cameras(s);bytes(file,previous);
        write(file,s.vehicles.camera_settings[target].orbit_offset);write(file,s.controls.current);++frames;
    }catch(...){error=std::current_exception();}}
};
void select_mounted(RaceSessionState& s){
    CameraEffectTargets targets;
    for(unsigned i=0;i<6;++i){targets.vehicles[i]=&s.vehicles.physics[i];targets.active[i]=s.vehicles.race[i].active;}
    camera_select_mode(s.controls.controllers[0].camera,6,camera_vehicle_reset_adapter(targets));
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and new capture file");require(!std::filesystem::exists(argv[2]),"preserve prior mounted capture");
    std::ofstream file(argv[2],std::ios::binary);file.write("SGNMCSP1",8);write(file,3u);
    unsigned scenario=0;
    for(const auto level:{"TRACK1","TRACK7","TRACK24"}){
        RaceSessionLaunch launch;launch.level_name=level;launch.mode=scenario==0?SessionMode::time_trial:SessionMode::normal_race;
        launch.model=scenario==0?1u:(scenario==1?16u:20u);launch.master_gain=0;launch.music_percent=0;
        RaceSession session(argv[1],launch);auto& s=session.state();Images images;RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
        PlayableDriver driver(session,textures,{});PlayableVehicleStages stages(session);
        const auto advance=[&](unsigned tick,SessionWorldTickAudit* hook){
            auto world=camera_frame_world(session);camera_frame_begin_outer(world);
            PlayableInput input;input.throttle=true;input.left=tick>=400&&tick<430;input.right=tick>=550&&tick<580;
            driver.tick(input,hook);
            (void)camera_frame_normal_views(world,{},stages,nullptr);
        };
        for(unsigned tick=0;tick<8;++tick)advance(tick,nullptr);
        select_mounted(s);const auto target=s.controls.controllers[0].vehicle;
        const auto handle=s.vehicles.models[target].model;require(handle.has_value(),"mounted target has no loaded model");
        string(file,level);string(file,session.models().source_name(*handle));write(file,720u);
        Audit capture(session,file,target);SessionWorldTickAudit hook;hook.context=&capture;
        hook.before_stage=[](void* p,unsigned entry)noexcept{if(entry==0x405210)static_cast<Audit*>(p)->before();};
        hook.after_stage=[](void* p,unsigned entry)noexcept{if(entry==0x405210)static_cast<Audit*>(p)->after();};
        for(unsigned tick=8;tick<728;++tick){advance(tick,&hook);if(capture.error)std::rethrow_exception(capture.error);}
        require(capture.frames==720&&capture.retained==719&&capture.moving>0,"actual mounted retained driving coverage missing");
        std::cout<<level<<" frames="<<capture.frames<<" retained="<<capture.retained<<" moving="<<capture.moving
            <<" nonzero_lod="<<capture.nonzero_lod<<" live_model_changes="<<capture.live_axes_changed<<'\n';++scenario;
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
