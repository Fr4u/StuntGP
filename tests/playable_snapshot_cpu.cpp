#include "sgn/playable_scene.hpp"
#include "sgn/playable_camera.hpp"
#include "sgn/playable_driver.hpp"
#include "sgn/race_overlay_textures.hpp"
#include <cstring>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool ok,const char* why){if(!ok)throw FormatError(why);}
class Images final:public TextureReplacementDevice {
public:std::uint16_t destination_width(unsigned)const override{return 256;}
void upload_to_destination(unsigned,const Image&)override{}
};
std::uint64_t digest(const PresentationSnapshot& frame){
    std::uint64_t h=1469598103934665603ull;
    const auto bytes=[&](const void* p,std::size_t n){const auto* b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i)h=(h^b[i])*1099511628211ull;};
    bytes(&frame.tick,sizeof(frame.tick));bytes(&frame.camera_continuity,sizeof(frame.camera_continuity));
    if(frame.view.world_to_view)bytes(frame.view.world_to_view->data(),64);
    for(const auto& object:frame.objects){
        bytes(&object.identity,sizeof(object.identity));bytes(&object.continuity,sizeof(object.continuity));bytes(&object.mesh,sizeof(object.mesh));
        bytes(&object.pose,sizeof(object.pose));bytes(object.model_to_view.data(),object.model_to_view.size()*64);
        for(const auto& part:object.parts){bytes(&part.pose,sizeof(part.pose));bytes(&part.visible,sizeof(part.visible));}
        require(bool(object.geometry),"snapshot has no owned geometry");const auto& model=object.geometry->model;
        bytes(model.vertices.data(),model.vertices.size()*sizeof(Vertex));
        for(const auto& name:model.textures)bytes(name.data(),name.size());
        for(const auto& surface:model.surfaces){bytes(&surface.first,sizeof(surface.first));bytes(&surface.count,sizeof(surface.count));bytes(&surface.texture,sizeof(surface.texture));}
        for(const auto& material:object.geometry->materials)bytes(&material.blend,sizeof(material.blend));
    }
    return h;
}
}
int main(int argc,char** argv)try{
    require(argc==2,"expected retail root");RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;launch.music_percent=0;
    RaceSession session(argv[1],launch);Images images;RaceOverlayTextures textures(session.assets(),images,session.shared_platform());PlayableDriver driver(session,textures,{});
    PlayableScene scene(session);require(!scene.snapshot(),"construction invented a source frame");
    ImmutablePresentationSnapshot previous;std::uint64_t retained=0,reads=0,checksum=0;unsigned objects=0;
    unsigned reset_cuts=0,following_camera_cuts=0,resumed=0;
    bool previous_reset=false,cut_finished=false;
    for(unsigned tick=0;tick<1100;++tick){
        const auto target=session.state().controls.controllers[0].vehicle;
        const auto old_epoch=session.state().presentation_vehicle_continuity.at(target);
        playable_camera_begin_tick(session);PlayableInput input;input.throttle=true;input.reset=tick==1020;
        driver.tick(input);scene.update_after_tick(session);
        auto frame=scene.snapshot();require(frame&&frame->tick==session.state().simulation_tick,"wrong published source tick");
        const bool reset=session.state().presentation_vehicle_continuity.at(target)!=old_epoch;
        if(previous&&(reset||previous_reset))require(frame->camera_continuity!=previous->camera_continuity,
            "reset or following source camera snapshot was left interpolable");
        if(reset)++reset_cuts;
        if(previous_reset)++following_camera_cuts;
        if(cut_finished&&!reset){require(frame->camera_continuity==previous->camera_continuity,
            "camera continuity kept cutting after the placement was observed");++resumed;}
        cut_finished=previous_reset&&!reset;previous_reset=reset;
        if(previous)require(digest(*previous)==retained,"source tick mutated previous immutable snapshot");
        const auto owner=session.state().view_matrices;const auto random=session.state().random.state;
        const auto hash=digest(*frame);for(unsigned i=0;i<12;++i){
            require(scene.snapshot()==frame&&digest(*scene.snapshot())==hash,"repeated read changed immutable frame");
            (void)scene.view();(void)scene.instances();(void)scene.live_views();++reads;
        }
        const auto& after=session.state().view_matrices;
        require(after.cursor==owner.cursor&&after.count==owner.count&&std::memcmp(after.matrices.data(),owner.matrices.data(),sizeof(owner.matrices))==0,"snapshot read changed source arena");
        require(session.state().random.state==random,"snapshot read consumed world RNG");
        previous=std::move(frame);retained=hash;checksum^=hash;objects=static_cast<unsigned>(previous->objects.size());
    }
    bool rejected=false;try{scene.update_after_tick(session);}catch(const FormatError&){rejected=true;}
    require(rejected&&digest(*previous)==retained,"duplicate source stage changed current frame");
    require(reset_cuts>0&&following_camera_cuts==reset_cuts&&resumed>0,"actual reset/camera catch-up sequence was not exercised");
    std::cout<<"{\"passed\":true,\"ticks\":1100,\"reads\":"<<reads<<",\"retained_checks\":1099,\"objects_last\":"<<objects
        <<",\"reset_cuts\":"<<reset_cuts<<",\"following_camera_cuts\":"<<following_camera_cuts<<",\"blend_resumed_checks\":"<<resumed
        <<",\"checksum\":"<<checksum<<",\"gpu_test\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
