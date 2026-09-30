#include "session_world_geometry_fixture.hpp"
#include "sgn/retail_world_view_prefix_session.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle_draw_pose.hpp"
#include "sgn/vehicle_model_prepare.hpp"
#include <iostream>
#include <thread>
#include <limits>

using namespace geometry_fixture;
namespace {
void prepare_inputs(Context& context){
    auto& session=context.session;auto& s=session.state();auto& v=s.vehicles;const auto current=s.controls.current;
    // Real preparation calls generate this tail's input. Their outer caller
    // composition is deliberately outside the tail equivalence claim.
    prepare_world_view_prefix(world_view_prefix(session));CameraEffectTargets effects;
    for(unsigned i=0;i<6;++i){effects.vehicles[i]=&v.physics[i];effects.active[i]=std::bit_cast<unsigned>(v.race[i].active);}
    camera_prepare_view(s.controls,s.camera_lifecycle,s.camera_presentation,s.camera_session,session.splines(),{},camera_vehicle_reset_adapter(effects),s.shared_model_identity,&s.view_matrices);
    auto& camera=s.controls.controllers[current].camera;s.frame_projection=camera.projection;s.inverse_depth_scale=camera.inverse_depth_scale;s.depth_offset=camera.depth_offset;initialize_model_matrix_identity(s.shared_model_identity);
    for(auto& m:context.models)for(unsigned lod=0;lod<=m.model->source_maximum_detail_level();++lod)allocate_model_part_matrices(*m.model,lod,s.view_matrices);
    for(unsigned i=0;i<3;++i)allocate_outer_model_matrices(s.model_draws.background.records[i].pose,s.view_matrices);
    auto& model=*context.models[2].model;
    prepare_vehicle_model(v.physics[0],v.trail_inputs[0],v.wheels[0],v.presentation[0],v.anchors[0],v.segments[0],model,s.selected_lod[0],s.model_basis,v.rail[0].effect_position);
    auto pose=vehicle_draw_pose(v.physics[0],v.contact[0].rig.origin_y_offset,s.model_draws.template_pose);
    append_model_draw(s.model_draws.regular,model,s.selected_lod[0],context.identities[2],pose,v.models[0].field_2708,s.view_matrices);
    LightVertexCursor cursor{s.light_vertices,s.light_vertex_offset};
    prepare_animated_light_view(s.lights,s.light_selection,s.light_fade,s.light_animation,{camera,s.camera_lifecycle[current],s.camera_presentation[current],s.overlays[current]},cursor,s.split_setting,nullptr);
    s.light_vertex_offset=cursor.offset;
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected retail root and output path or --guards");const bool guards=std::string_view(argv[2])=="--guards";
    RaceSessionLaunch launch;launch.master_gain=0;launch.sound_percent=0;launch.music_percent=0;std::uint64_t calls{},checks{};
    if(guards){
        RaceSession session(argv[1],launch);auto context=std::make_unique<Context>(session);auto& s=session.state();const auto ticket=capture_world_polygon_level(s.polygon_frame);const auto original=s.random.state;
        bool rejected=false;std::thread foreign([&]{try{finish_session_world_geometry(session);}catch(const std::exception&){rejected=true;}});foreign.join();require(rejected&&s.random.state==original,"foreign-thread tail changed RNG");validate_world_polygon_level(s.polygon_frame,ticket);++checks;
        s.controls.current=4;rejected=false;try{finish_session_world_geometry(session);}catch(const std::exception&){rejected=true;}require(rejected&&s.random.state==original,"invalid camera preflight changed RNG");s.controls.current=0;validate_world_polygon_level(s.polygon_frame,ticket);++checks;
        prepare_inputs(*context);s.light_vertex_offset=137;s.viewport.scale_x=std::numeric_limits<float>::quiet_NaN();const auto stamp=s.polygon_frame.frame_stamp;
        rejected=false;try{finish_session_world_geometry(session);}catch(const std::exception&){rejected=true;}require(rejected,"post-producer projection failure was accepted");require(s.light_vertex_offset!=137,"exception discarded actual light cursor writes");require(s.polygon_frame.frame_stamp==stamp,"failed tail advanced frame stamp");++checks;
        const auto failed_rng=s.random.state,failed_cursor=s.light_vertex_offset;rejected=false;try{finish_session_world_geometry(session);}catch(const std::exception&){rejected=true;}require(rejected&&s.random.state==failed_rng&&s.light_vertex_offset==failed_cursor,"failed tail was retried");++checks;
        rejected=false;try{validate_world_polygon_level(ticket);}catch(const std::exception&){rejected=true;}require(rejected,"failed tail left old presentation ticket live");++checks;
        std::cout<<"{\"passed\":true,\"guard_checks\":"<<checks<<"}\n";return 0;
    }
    Writer out(argv[2]);out.raw("SGNSGT01",8);out.integer(4);
    const std::array levels{"TRACK1","TRACK7","TRACK11","TRACK24"};const std::array widths{1280,1920,3440,5120};const std::array heights{720,1080,1440,1440};
    for(unsigned group=0;group<levels.size();++group){
        launch.level_name=levels[group];launch.width=widths[group];launch.height=heights[group];launch.model=group+1;launch.configuration=group+1;
        RaceSession session(argv[1],launch);auto context=std::make_unique<Context>(session);auto& s=session.state();out.name(launch.level_name);for(auto& model:context->models)out.name(model.name);out.integer(8);
        s.polygon_frame.frame_stamp=group%2?0xffffffffu:65534u;
        for(unsigned step=0;step<8;++step){prepare_inputs(*context);out.snapshot(context->snapshot());const auto before=s.polygon_frame.frame_stamp;
            finish_session_world_geometry(session,{{&context->rotation,nullptr},&context->clip_audit});require(s.polygon_frame.frame_stamp==((before+1u)&0xffffu),"source stamp wrap");out.snapshot(context->snapshot());++calls;++checks;}
        std::cout<<levels[group]<<" captured "<<calls<<" actual Session tails\n";
    }
    std::cout<<"{\"passed\":true,\"sessions\":4,\"calls\":"<<calls<<",\"native_retained_calls\":"<<calls-4<<",\"host_checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
