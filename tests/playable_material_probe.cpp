#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/live_scene_geometry.hpp"
#include "sgn/retail_model_matrices.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <tuple>

namespace {
using namespace sgn;using namespace sgn::retail;
class Images final:public TextureReplacementDevice {
public:
    std::uint16_t destination_width(unsigned)const override{return 256;}
    void upload_to_destination(unsigned,const Image&)override{}
};
void inspect(ModelResource& model,unsigned lod,const AssetStore& assets){
    std::cout<<"{\"name\":"<<std::quoted(std::string(model.source_name()))<<",\"lod\":"<<lod
        <<",\"model_flags\":"<<model.source_model_flags_02()<<",\"groups\":[";
    const auto indices=model.source_corner_indices();bool comma=false;
    for(unsigned p=0;p<model.part_count(lod);++p){
        const auto visibility=model.part(lod,p).flags;const auto flags=model.source_part_flags_02(lod,p);
        const bool forced=(model.source_model_flags_02()&8)&&(flags&0x80)&&(flags>>8);
        const auto coords=model.source_coordinates(lod,p);
        using Key=std::tuple<unsigned,unsigned,unsigned>;
        struct Group {unsigned faces{};std::map<unsigned,unsigned> input_alpha,output_alpha;};
        std::map<Key,Group> groups;
        for(const auto& a:model.source_attributes(lod,p)){
            auto& g=groups[{a[1],a[3],a[2]}];++g.faces;
            const auto first=unsigned(a[6])|(unsigned(a[7])<<8);
            for(unsigned i=0;i<a[0];++i){const auto color=coords[indices[first+i]][3];
                ++g.input_alpha[color>>24];++g.output_alpha[source_polygon_color(color,a[2],a[3],forced)>>24];}
        }
        for(const auto& [key,g]:groups){const auto [texture,attribute,alpha]=key;
            if(comma)std::cout<<',';comma=true;
            std::cout<<"{\"part\":"<<p<<",\"visible\":"<<(p==0||visibility==1)<<",\"part_flags\":"<<flags
                <<",\"forced\":"<<forced<<",\"texture\":"<<std::quoted(model.geometry(lod).textures.at(texture))
                <<",\"attribute_flags\":"<<attribute<<",\"attribute_alpha\":"<<alpha<<",\"faces\":"<<g.faces
                <<",\"blend\":"<<unsigned(source_polygon_blend(static_cast<std::uint8_t>(attribute),forced));
            const auto histogram=[](const auto& values){std::cout<<'[';bool sep=false;for(const auto& [a,n]:values){if(sep)std::cout<<',';sep=true;std::cout<<'['<<a<<','<<n<<']';}std::cout<<']';};
            std::cout<<",\"corner_alpha\":";histogram(g.input_alpha);std::cout<<",\"output_alpha\":";histogram(g.output_alpha);std::cout<<'}';
        }
    }
    std::cout<<"],\"textures\":[";comma=false;
    for(const auto& name:model.geometry(lod).textures){if(comma)std::cout<<',';comma=true;const auto image=assets.texture(name);
        std::map<unsigned,unsigned> alpha;for(std::size_t i=3;i<image.rgba.size();i+=4)++alpha[image.rgba[i]];
        std::cout<<"{\"name\":"<<std::quoted(name)<<",\"width\":"<<image.width<<",\"height\":"<<image.height<<",\"alpha\":[";
        bool sep=false;for(const auto& [a,n]:alpha){if(sep)std::cout<<',';sep=true;std::cout<<'['<<a<<','<<n<<']';}std::cout<<"]}";
    }
    std::cout<<"]}";
}
void light_faces(ModelResource& model,const CameraFrameSnapshot& snapshot){
    ModelTransform pose{{0,0,0},0,{1,1,1},0,{0,0,-1},0,{0,1,0},0,{0,0,0,2}};
    ViewMatrixArena scratch;reset_view_matrix_cursor(scratch);allocate_outer_model_matrices(pose,scratch);
    const auto& pivot=model.source_model_vector_10();
    const CameraModelTransform transform{{0,0,0,0},{1,1,1,0},{pivot.x,pivot.y,pivot.z,pivot.w},resolve_model_matrix(scratch,vehicle_identity(),pose.tail[1])};
    const auto views=model_live_views(model,0,snapshot.camera(),transform);
    const auto& projection=snapshot.camera().projection;
    std::cout<<"{\"projection\":["<<projection[0]<<','<<projection[5]<<','<<projection[10]<<','<<projection[14]
        <<"],\"clip_near\":"<<snapshot.near_clip()<<",\"clip_far\":"<<snapshot.far_clip()<<",\"polygons\":[";
    bool comma=false;const auto indices=model.source_corner_indices();
    for(unsigned p=0;p<model.part_count(0);++p){
        if(!views.parts[p].visible)continue;
        const auto attributes=model.source_attributes(0,p);const auto coords=model.source_coordinates(0,p);const auto vertices=model.source_vertices(0,p);const auto& m=views.model_to_view[p];
        for(unsigned f=0;f<attributes.size();++f){const auto& a=attributes[f];
            if(model.geometry(0).textures[a[1]]!="level1\\lightray")continue;
            if(comma)std::cout<<',';comma=true;std::cout<<"{\"part\":"<<p<<",\"face\":"<<f<<",\"view_corners\":[";
            const auto first=unsigned(a[6])|(unsigned(a[7])<<8);
            for(unsigned i=0;i<a[0];++i){if(i)std::cout<<',';const auto& c=coords[indices[first+i]];const auto& v=vertices[c[2]&0xffff];
                std::cout<<'[';for(unsigned k=0;k<3;++k){if(k)std::cout<<',';
                    std::cout<<(double(v.x)*m[k]+double(v.y)*m[4+k]+double(v.z)*m[8+k]+m[12+k]);}
                std::cout<<','<<(c[3]>>24)<<']';}
            std::cout<<"]}";
        }
    }
    std::cout<<"]}";
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw sgn::FormatError("expected retail root");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.width=1440;launch.height=900;launch.master_gain=0;launch.music_percent=0;
    RaceSession session(argv[1],launch);auto& s=session.state();Images images;RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    PlayableDriver driver(session,textures,{});PlayableVehicleStages stages(session);
    std::cout<<std::setprecision(9)<<"{\"gpu_test\":false,\"original_exe_replay\":false,\"initial_car\":";
    inspect(session.models().at(*s.vehicles.models[0].model),0,session.assets());
    std::cout<<",\"tail_ticks\":[";
    for(unsigned n=0;n<1200;++n){
        auto world=camera_frame_world(session);camera_frame_begin_outer(world);
        PlayableInput input;input.throttle=n<1140;input.brake=n>=1140;input.left=n>=900&&n<930;input.right=n>=960&&n<990;input.turbo=n>=840&&n<900;
        driver.tick(input);(void)camera_frame_normal_views(world,{},stages,nullptr);
        if(n>=1190){if(n>1190)std::cout<<',';std::cout<<"{\"tick\":"<<n+1<<",\"collision_hold\":"<<s.vehicles.race[0].collision_hold
            <<",\"draw_flags\":"<<s.vehicles.models[0].field_2708<<",\"lod\":"<<s.selected_lod[0]<<'}';}
    }
    const auto& position=s.vehicles.physics[0].position;
    std::cout<<"],\"position\":["<<position.x<<','<<position.y<<','<<position.z<<"],\"final_car\":";
    auto& car=session.models().at(*s.vehicles.models[0].model);inspect(car,car.source_lod_index(s.selected_lod[0]),session.assets());
    std::cout<<",\"track\":";
    const auto track=session.scene_resources().model_handle(3);
    if(!track)throw FormatError("track source handle absent");
    inspect(session.models().at(*track),0,session.assets());
    std::cout<<",\"light_projection\":";
    light_faces(session.models().at(*track),camera_frame_snapshot(camera_frame_world(session)));
    std::cout<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
