#include "sgn/playable_driver.hpp"
#include "sgn/playable_vehicle_stages.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/live_scene_geometry.hpp"
#include "sgn/live_draw_batch.hpp"
#include <cstring>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool ok,const char* reason){if(!ok)throw FormatError(reason);}
class Images final:public TextureReplacementDevice {
    Image image_;
public:
    explicit Images(const AssetStore& assets):image_(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(unsigned slot)const override{require(slot==15&&image_.width==256,"overlay binding");return 256;}
    void upload_to_destination(unsigned slot,const Image& image)override{require(slot==15,"overlay destination");image_=image;}
};
struct Totals {
    unsigned ticks{},cars{},shadows{},geometry_checks{},view_reads{};
    std::array<unsigned,4> lods{};
    std::array<unsigned,6> car_slots{},shadow_slots{};
};
void observe_geometry(RaceSession& session,const PreparedModelDraw& prepared,
                      unsigned lod,bool shadow,Totals& totals){
    const auto& command=prepared.command;
    const auto handle=session.models().resolve(command.model_identity);require(handle.has_value(),"unresolved draw identity");
    auto& model=session.models().at(*handle);
    const auto& views=prepared.views;
    auto geometry=model_live_geometry(model,lod);coalesce_live_draws(geometry);
    require(!geometry.model.vertices.empty(),"authored active draw has no geometry");
    require(views.parts.size()==model.part_count(lod)&&views.model_to_view.size()==views.parts.size(),"part view span mismatch");
    if(shadow){
        require(model.lod_count()==1&&model.source_lod_index(0)==0,"unexpected authored shadow LOD");
        require(!geometry.materials.empty()&&geometry.materials.front().blend==LiveBlend::multiply,"shadow lost source multiply material");
    }
    for(const auto& surface:geometry.model.surfaces)require(surface.part<views.parts.size(),"draw part outside view storage");
    ++totals.geometry_checks;
}
Totals run(const std::filesystem::path& root,SessionMode mode,unsigned ticks,unsigned quality=100){
    RaceSessionLaunch launch;launch.mode=mode;launch.render_quality=quality;launch.master_gain=0;launch.music_percent=0;
    RaceSession session(root,launch);auto& s=session.state();
    const auto initial_models=session.models().size();
    Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    PlayableDriver driver(session,textures,{});PlayableVehicleStages stages(session);Totals totals;
    const auto static_count=1u+unsigned(session.scene_resources().model_handle(3).has_value())+
        unsigned(session.scene_resources().model_handle(2).has_value());
    require(stages.regular_queue().cursor==0u&&stages.background_queue().cursor==static_count,"construction lost source scene seed");
    for(unsigned n=0;n<ticks;++n){
        auto world=camera_frame_world(session);camera_frame_begin_outer(world);
        PlayableInput input;input.throttle=true;
        driver.tick(input);
        const auto camera=camera_frame_normal_views(world,{},stages,nullptr);
        require(s.current_slot==0&&s.controls.current==0,"source iteration cursor not restored");
        require(s.view_matrices.cursor.has_value()&&*s.view_matrices.cursor>=128&&s.view_matrices.count>=2,"second camera and scene seed did not allocate matrices");
        require(s.model_draws.regular.cursor==0u&&s.model_draws.background.cursor==static_count,"source queues were not reseeded after consumption");
        unsigned active{},visible{};
        for(unsigned slot=0;slot<6;++slot){
            const auto* car=stages.car(slot);const auto* shadow=stages.shadow(slot);
            const bool expected=s.vehicles.race[slot].active==1;
            require(bool(car)==expected,"active physical slot was dropped or inactive slot drawn");
            require(bool(shadow)==(expected&&s.vehicles.presentation[slot].field_3928!=0),"shadow visibility gate differs");
            if(!expected)continue;
            ++active;++totals.cars;++totals.car_slots[slot];
            require(car->model_identity==session.models().identity(s.vehicles.models[slot].model),"car drawn with another physical slot model");
            require(car==&stages.regular_queue().records[active-1],"car queue is not source slot order");
            require(s.selected_lod[slot]<4,"source LOD outside four slots");
            ++totals.lods[s.selected_lod[slot]];
            const auto lod=session.models().at(*s.vehicles.models[slot].model).source_lod_index(s.selected_lod[slot]);
            if(n%120==0||n+1==ticks)observe_geometry(session,*stages.car_view(slot),lod,false,totals);
            if(shadow){
                ++visible;++totals.shadows;++totals.shadow_slots[slot];
                require(shadow->model_identity==session.models().identity(s.vehicles.models[slot].shadow),"shadow drawn with another physical slot resource");
                require(shadow==&stages.background_queue().records[static_count+visible-1],"shadow queue is not source slot order after static seed");
                const auto& actual=s.vehicles.shadows[slot];
                require(shadow->pose.position.x==actual.position.x&&shadow->pose.position.y==actual.position.y&&shadow->pose.position.z==actual.position.z,"shadow position was replaced");
                require(shadow->pose.scale.x==actual.factor_392c&&shadow->pose.scale.y==actual.factor_392c&&shadow->pose.scale.z==actual.factor_392c,"shadow scale was replaced");
                if(n%120==0||n+1==ticks)observe_geometry(session,*stages.shadow_view(slot),0,true,totals);
            }
        }
        require(stages.regular_queue().cursor==active&&stages.background_queue().cursor==static_count+visible,"draw count differs from active and static commands");
        require(stages.regular_queue().records[active].model_identity==0&&stages.background_queue().records[static_count+visible].model_identity==0,"draw terminator missing");
        if(mode==SessionMode::normal_race)require(active==6,"normal authored race must retain all six physical models");
        // Additional presentation reads cannot advance the world, modify the
        // source allocation arena or replace queue storage between ticks.
        const auto matrices=s.view_matrices;const auto regular=stages.regular_queue();const auto background=stages.background_queue();
        const auto random=s.random.state,source_tick=s.simulation_tick;
        for(unsigned frame=0;frame<4;++frame){
            const auto duplicate=camera_frame_snapshot(world);
            require(std::memcmp(duplicate.world_to_view().data(),camera.world_to_view().data(),sizeof(CameraMatrix))==0,"extra presentation read changed camera");
            for(unsigned slot=0;slot<6;++slot){(void)stages.car(slot);(void)stages.shadow(slot);}
            ++totals.view_reads;
        }
        require(s.random.state==random&&s.simulation_tick==source_tick,"presentation advanced simulation");
        require(s.view_matrices.matrices==matrices.matrices&&s.view_matrices.cursor==matrices.cursor&&s.view_matrices.count==matrices.count,"presentation changed source matrix arena");
        require(std::memcmp(stages.regular_queue().records.data(),regular.records.data(),sizeof(regular.records))==0&&
                std::memcmp(stages.background_queue().records.data(),background.records.data(),sizeof(background.records))==0,"presentation changed model commands");
        require(session.models().size()==initial_models,"drawing allocated another mutable resource");
        ++totals.ticks;
    }
    if(quality>=50&&!totals.shadows)std::cerr<<"no shadows: mode="<<static_cast<unsigned>(mode)<<" source_quality="<<s.impact_particles.rate_selector
        <<" active_lights="<<s.light_selection.active_count<<" slot0_visibility="<<s.vehicles.presentation[0].field_3924<<','
        <<s.vehicles.presentation[0].field_3928<<" slot0_lod="<<s.selected_lod[0]<<'\n';
    require(s.impact_particles.rate_selector==quality,"launch quality did not reach source owner");
    if(quality>=50)require(totals.shadows>0,"no actual source shadow reached draw staging");
    else require(totals.shadows==0,"low source quality shadow guard was bypassed");
    return totals;
}
void print(const Totals& t){
    std::cout<<"{\"ticks\":"<<t.ticks<<",\"cars\":"<<t.cars<<",\"shadows\":"<<t.shadows
        <<",\"geometry_checks\":"<<t.geometry_checks<<",\"extra_view_reads\":"<<t.view_reads<<",\"lods\":[";
    for(unsigned i=0;i<4;++i){if(i)std::cout<<',';std::cout<<t.lods[i];}
    std::cout<<"],\"car_slots\":[";for(unsigned i=0;i<6;++i){if(i)std::cout<<',';std::cout<<t.car_slots[i];}
    std::cout<<"],\"shadow_slots\":[";for(unsigned i=0;i<6;++i){if(i)std::cout<<',';std::cout<<t.shadow_slots[i];}
    std::cout<<"]}";
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw sgn::FormatError("expected retail root");
    const auto trial=run(argv[1],sgn::retail::SessionMode::time_trial,1200);
    const auto race=run(argv[1],sgn::retail::SessionMode::normal_race,1200);
    const auto low=run(argv[1],sgn::retail::SessionMode::time_trial,120,0);
    std::cout<<"{\"passed\":true,\"time_trial\":";print(trial);std::cout<<",\"normal_race\":";print(race);
    std::cout<<",\"quality_zero_control\":";print(low);
    std::cout<<",\"gpu_test\":false,\"original_exe_replay\":false,\"injected_vehicle_state\":false}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
