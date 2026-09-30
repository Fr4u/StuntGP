#include "sgn/playable_camera.hpp"
#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool ok,const char* why){if(!ok)throw FormatError(why);}
void write(std::ofstream& f,const auto& v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));require(bool(f),"model view capture write");}
class Images final:public TextureReplacementDevice {
public:std::uint16_t destination_width(unsigned)const override{return 256;}
void upload_to_destination(unsigned,const Image&)override{}
};
struct PartInput {ModelTransform pose;Vec3 pivot;};
struct DrawInput {ModelDrawCommand command;CameraVector pivot;std::uint32_t lod{};std::vector<PartInput> parts;};
struct Stages final:CameraFrameStages {
    RaceSession& session;PlayableVehicleStages producer;std::ofstream& file;
    bool capture{};unsigned mode{};std::uint64_t frames{},draws{},parts{};
    Stages(RaceSession& s,std::ofstream& f,unsigned m):session(s),producer(s),file(f),mode(m){}
    void stage_entities_442500(const CameraFrameSnapshot& camera)override{producer.stage_entities_442500(camera);}
    void prepare_world_42e870()override{producer.prepare_world_42e870();}
    std::vector<DrawInput> inputs(const ModelDrawQueue& queue){
        std::vector<DrawInput> result;auto& s=session.state();require(queue.cursor.has_value(),"uninitialized source queue");
        for(unsigned i=0;i<*queue.cursor;++i){const auto& c=queue.records[i];DrawInput d;d.command=c;
            if(c.model_identity==sprite_model_identity)d.parts.push_back({s.model_draws.sprite_part_transform,{}});
            else {const auto handle=session.models().resolve(c.model_identity);require(handle.has_value(),"captured queue resource");
                auto& model=session.models().at(*handle);
                for(unsigned slot=0;slot<6;++slot)if(c.model_identity==session.models().identity(s.vehicles.models[slot].model))d.lod=s.selected_lod[slot];
                const auto& pivot=model.source_model_vector_10();d.pivot={pivot.x,pivot.y,pivot.z,pivot.w};
                const auto lod=model.source_lod_index(d.lod);
                for(unsigned p=0;p<model.part_count(lod);++p){const auto part=model.part(lod,p);d.parts.push_back({part.transform,part.pivot});}}
            result.push_back(std::move(d));}
        return result;
    }
    const PreparedModelDraw& prepared(unsigned index,bool background){
        const auto& queue=background?producer.background_queue():producer.regular_queue();
        for(unsigned slot=0;slot<6;++slot){const auto* command=background?producer.shadow(slot):producer.car(slot);
            if(command==&queue.records[index])return *(background?producer.shadow_view(slot):producer.car_view(slot));}
        const auto* result=producer.static_view(queue.records[index].model_identity);require(result!=nullptr,"captured static draw missing");return *result;
    }
    void consume_model_views(const CameraFrameSnapshot& camera)override{
        if(!capture){producer.consume_model_views(camera);return;}
        auto& s=session.state();const auto arena=s.view_matrices;
        const auto background=inputs(s.model_draws.background),regular=inputs(s.model_draws.regular);
        const auto camera_ref=camera.camera().world_to_camera_reference;
        write(file,s.simulation_tick);write(file,mode);write(file,camera.camera().position);write(file,camera_ref);
        write(file,s.shared_model_identity);write(file,arena.matrices);
        write(file,static_cast<std::uint32_t>(background.size()+regular.size()));
        producer.consume_model_views(camera);
        const auto output=[&](const std::vector<DrawInput>& values,bool bg){
            for(unsigned i=0;i<values.size();++i){const auto& d=values[i];const auto& actual=prepared(i,bg);
                require(actual.command.model_identity==d.command.model_identity&&actual.source_lod==d.lod,"source queue capture selection differs");
                require(actual.views.model_to_view.size()==d.parts.size(),"model view capture part count");
                write(file,d.command.model_identity);write(file,d.lod);write(file,d.command.pose);write(file,d.pivot);
                write(file,static_cast<std::uint32_t>(d.parts.size()));
                for(unsigned p=0;p<d.parts.size();++p){write(file,d.parts[p].pose);write(file,d.parts[p].pivot);write(file,actual.views.model_to_view[p]);++parts;}
                ++draws;}
        };
        output(background,true);output(regular,false);++frames;
    }
};
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and output corpus");std::ofstream file(argv[2],std::ios::binary);
    file.write("SGNMSV01",8);write(file,std::uint32_t{});std::uint64_t frames{},draws{},parts{};
    for(unsigned mode=0;mode<3;++mode){RaceSessionLaunch launch;launch.mode=static_cast<SessionMode>(mode);launch.master_gain=0;launch.music_percent=0;
        RaceSession session(argv[1],launch);Images images;RaceOverlayTextures textures(session.assets(),images,session.shared_platform());PlayableDriver driver(session,textures,{});
        Stages stages(session,file,mode);
        for(unsigned tick=0;tick<1200;++tick){playable_camera_begin_tick(session);PlayableInput input;input.throttle=true;input.reset=tick==1020;driver.tick(input);
            stages.capture=tick<4||tick%120==0||tick==1199||(tick>=1019&&tick<=1021);
            const auto camera=playable_camera_after_tick(session,stages);(void)camera;}
        frames+=stages.frames;draws+=stages.draws;parts+=stages.parts;
    }
    file.seekp(8);write(file,static_cast<std::uint32_t>(frames));file.close();require(bool(file),"capture file close");
    std::cout<<"{\"passed\":true,\"ticks\":3600,\"frames\":"<<frames<<",\"draws\":"<<draws<<",\"parts\":"<<parts<<",\"gpu_test\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
