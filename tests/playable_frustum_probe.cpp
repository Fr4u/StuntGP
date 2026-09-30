#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/live_scene_geometry.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <iomanip>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
class Images final:public TextureReplacementDevice {
public:
    std::uint16_t destination_width(unsigned)const override{return 256;}
    void upload_to_destination(unsigned,const Image&)override{}
};
void frame(RaceSession& session,const PlayableVehicleStages& stages,const CameraFrameSnapshot& snapshot,unsigned tick){
    const auto& s=session.state();const auto& projection=snapshot.camera().projection;
    std::cout<<"{\"tick\":"<<tick<<",\"projection\":["<<projection[0]<<','<<projection[5]<<','<<projection[10]<<','<<projection[14]
        <<"],\"clip_near\":"<<snapshot.near_clip()<<",\"clip_far\":"<<snapshot.far_clip()<<",\"cars\":[";
    for(unsigned slot=0;slot<6;++slot){
        if(slot)std::cout<<',';const auto* command=stages.car(slot);if(!command)throw FormatError("active normal car absent from queue");
        auto& model=session.models().at(*s.vehicles.models[slot].model);const auto lod=model.source_lod_index(s.selected_lod[slot]);
        const auto& p=command->pose;const auto& pivot=model.source_model_vector_10();
        const CameraModelTransform transform{{p.position.x,p.position.y,p.position.z,0},{p.scale.x,p.scale.y,p.scale.z,0},
            {pivot.x,pivot.y,pivot.z,pivot.w},resolve_model_matrix(s.view_matrices,s.shared_model_identity,p.tail[1])};
        const auto views=model_live_views(model,lod,snapshot.camera(),transform);const auto geometry=model_live_geometry(model,lod);
        const auto& pos=s.vehicles.physics[slot].position;
        std::cout<<"{\"slot\":"<<slot<<",\"name\":"<<std::quoted(std::string(model.source_name()))<<",\"model_identity\":"<<command->model_identity
            <<",\"active\":"<<s.vehicles.race[slot].active<<",\"source_lod\":"<<s.selected_lod[slot]<<",\"resident_lod\":"<<lod
            <<",\"collision_hold\":"<<s.vehicles.race[slot].collision_hold<<",\"draw_flags\":"<<s.vehicles.models[slot].field_2708
            <<",\"position\":["<<pos.x<<','<<pos.y<<','<<pos.z<<"],\"triangles\":[";
        bool comma=false;
        for(const auto& surface:geometry.model.surfaces){
            if(!views.parts[surface.part].visible)continue;const auto& m=views.model_to_view[surface.part];
            for(unsigned at=surface.first;at<surface.first+surface.count;at+=3){
                if(comma)std::cout<<',';comma=true;std::cout<<'[';
                for(unsigned j=0;j<3;++j){if(j)std::cout<<',';const auto& v=geometry.model.vertices[at+j].position;std::cout<<'[';
                    for(unsigned k=0;k<3;++k){if(k)std::cout<<',';std::cout<<(double(v.x)*m[k]+double(v.y)*m[4+k]+double(v.z)*m[8+k]+m[12+k]);}
                    std::cout<<']';}
                std::cout<<']';
            }
        }
        std::cout<<"]}";
    }
    std::cout<<"]}";
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw sgn::FormatError("expected retail root");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.width=1440;launch.height=900;launch.master_gain=0;launch.music_percent=0;
    RaceSession session(argv[1],launch);Images images;RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    PlayableDriver driver(session,textures,{});PlayableVehicleStages stages(session);
    std::cout<<std::setprecision(12)<<"{\"width\":1440,\"height\":900,\"gpu_test\":false,\"original_exe_replay\":false,\"frames\":[";
    bool comma=false;
    for(unsigned n=0;n<500;++n){auto world=camera_frame_world(session);camera_frame_begin_outer(world);PlayableInput input;input.throttle=true;
        driver.tick(input);const auto snapshot=camera_frame_normal_views(world,{},stages,nullptr);
        if(n+1==240||n+1==300||n+1==400||n+1==500){if(comma)std::cout<<',';comma=true;frame(session,stages,snapshot,n+1);}
    }
    std::cout<<"]}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
