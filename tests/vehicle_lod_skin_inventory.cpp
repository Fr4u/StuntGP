#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/live_scene_geometry.hpp"
#include "sgn/live_draw_batch.hpp"
#include <iomanip>
#include <iostream>
#include <map>

namespace {
using namespace sgn;using namespace sgn::retail;
class Images final:public TextureReplacementDevice {
public:
    std::uint16_t destination_width(unsigned)const override{return 256;}
    void upload_to_destination(unsigned,const Image&)override{}
};
void materials(RaceSession& session){
    auto& s=session.state();std::cout<<'[';
    for(unsigned slot=0;slot<6;++slot){
        if(slot)std::cout<<',';auto& model=session.models().at(*s.vehicles.models[slot].model);
        std::cout<<"{\"slot\":"<<slot<<",\"name\":"<<std::quoted(std::string(model.source_name()))<<",\"livery\":"<<s.cars[slot].settings.livery
            <<",\"tyre\":"<<s.cars[slot].settings.tyre<<",\"lods\":[";
        for(unsigned source=0;source<4;++source){
            if(source)std::cout<<',';const auto lod=model.source_lod_index(source);auto live=model_live_geometry(model,lod);coalesce_live_draws(live);
            if(live.model.textures!=model.geometry(lod).textures)throw FormatError("live LOD changed model texture namespace");
            std::map<std::string,unsigned> names;
            for(const auto& a:model.source_attributes(lod,0))++names[live.model.textures.at(a[1])];
            std::cout<<"{\"source_lod\":"<<source<<",\"resident_lod\":"<<lod<<",\"vertices\":"<<live.model.vertices.size()<<",\"body_materials\":[";
            bool comma=false;for(const auto& [name,count]:names){if(comma)std::cout<<',';comma=true;std::cout<<'['<<std::quoted(name)<<','<<count<<']';}
            std::cout<<"],\"textures\":[";comma=false;for(const auto& name:live.model.textures){if(comma)std::cout<<',';comma=true;std::cout<<std::quoted(name);}std::cout<<"]}";
        }
        std::cout<<"]}";
    }
    std::cout<<']';
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw sgn::FormatError("expected retail root");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.width=1440;launch.height=900;launch.master_gain=0;launch.music_percent=0;
    RaceSession session(argv[1],launch);Images images;RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    PlayableDriver driver(session,textures,{});PlayableVehicleStages stages(session);auto& s=session.state();
    std::cout<<"{\"gpu_test\":false,\"cars\":";materials(session);std::cout<<",\"transitions\":[";
    std::array<unsigned,6> previous{};previous.fill(99);bool comma=false;
    for(unsigned n=0;n<1800;++n){auto world=camera_frame_world(session);camera_frame_begin_outer(world);PlayableInput input;input.throttle=true;
        driver.tick(input);(void)camera_frame_normal_views(world,{},stages,nullptr);
        for(unsigned slot=0;slot<6;++slot)if(previous[slot]!=s.selected_lod[slot]){
            if(comma)std::cout<<',';comma=true;auto& model=session.models().at(*s.vehicles.models[slot].model);const auto source=s.selected_lod[slot],lod=model.source_lod_index(source);
            const auto* command=stages.car(slot);if(!command||command->model_identity!=session.models().identity(s.vehicles.models[slot].model))throw FormatError("LOD transition changed physical model identity");
            std::cout<<"{\"tick\":"<<n+1<<",\"slot\":"<<slot<<",\"from\":"<<previous[slot]<<",\"to\":"<<source<<",\"body_texture\":"
                <<std::quoted(model.geometry(lod).textures.at(model.source_attributes(lod,0).front()[1]))<<'}';previous[slot]=source;
        }
    }
    std::cout<<"]}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
