#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/retail_model_matrices.hpp"
#include "sgn/retail_camera_mounted.hpp"
#include "retail_camera_lifecycle_state.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <type_traits>

namespace {
using namespace sgn;
using namespace sgn::retail;
std::size_t checks{},source_frames{},gpu_reads{},exact_words{},collision_cases{};
void require(bool condition,const char* message){if(!condition)throw FormatError(message);++checks;}
void add(Bytes& out,std::uint32_t value){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
void add(Bytes& out,float value){add(out,std::bit_cast<std::uint32_t>(value));}
void add(Bytes& out,const CameraVector& v){add(out,v.x);add(out,v.y);add(out,v.z);add(out,v.w);}
void add(Bytes& out,const CameraMatrix& matrix){for(auto value:matrix)add(out,value);}
void add(Bytes& out,const contact::Vector4& v){add(out,v.x);add(out,v.y);add(out,v.z);add(out,v.opaque);}
void add(Bytes& out,const Vec3& v){add(out,v.x);add(out,v.y);add(out,v.z);}
Bytes camera_bytes(CameraState camera,CameraLifecycle life){
    Bytes c(camera_test::camera_size),v(camera_test::vehicle_size);CameraVehicle dummy;
    camera_test::transfer(c,v,camera,dummy,false);camera_test::lifecycle(c,life,false);return c;
}
struct Fixture {
    const SplineSet& splines;const contact::TrackGeometry& track;
    std::unique_ptr<RaceSessionState> state=std::make_unique<RaceSessionState>();
    Fixture(const SplineSet& path,const contact::TrackGeometry& geometry,const Bytes& pmd):splines(path),track(geometry){
        auto& s=*state;s.light_vertices.resize(0x4800);s.camera_bank.count=1;
        reset_view_matrix_cursor(s.view_matrices);
        s.viewport.width=641;s.viewport.height=481;s.viewport.depth_bits=32;initialize_viewport(s.viewport,s.split_setting);
        s.lights=decode_pmd_lights(pmd);initialize_pmd_lights(s.lights,s.ambient_light);
        s.gravity.unit={0,-1,0,0};s.clock.course_length=100000;s.ranking.count=6;
        for(unsigned i=0;i<6;++i){auto& p=s.vehicles.physics[i];auto& r=s.vehicles.race[i];
            r.active=1;r.place=i;r.progress=0.02f*float(i);s.ranking.slots[i]=i;
            p.position={100000+float(i)*1000,200000,300000,1};p.basis_x={1,0,0,0};p.up={0,1,0};p.basis_z={0,0,1,0};
            p.orientation={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};p.speed=200+float(i)*10;
            p.camera_effects.values.fill(0x12345678);s.vehicles.parameters[i].height=100;
            initialize_vehicle_camera_settings(s.vehicles.camera_settings[i]);s.vehicles.camera_settings[i].approach_scale=1;
        }
        for(unsigned i=0;i<4;++i){auto& c=s.controls.controllers[i];
            c.active=i==0?1:0;c.vehicle=i;camera_initialize_values(c.camera,s.camera_lifecycle[i],s.shared_model_identity);
            c.camera.position={100000+float(i)*1000,200000,299000,1};c.camera.target={100000+float(i)*1000,200000,300000,1};
            c.camera.collision_bypass_ticks=600;c.camera.pending_follow_setup=0;s.camera_presentation[i].spline=splines.find(1);
            allocate_camera_view_matrices(c.camera,s.view_matrices,c.camera.twist);
        }
        s.camera_session.path_speed=0.000125f;s.camera_session.path_parameter=0.125f;
    }
    CameraFrameWorld view(){return camera_frame_world(*state,splines,track);}
};
Bytes state_bytes(const Fixture& fixture){
    const auto& s=*fixture.state;Bytes out;
    for(unsigned i=0;i<4;++i){const auto& c=s.controls.controllers[i];
        auto bytes=camera_bytes(c.camera,s.camera_lifecycle[i]);out.insert(out.end(),bytes.begin(),bytes.end());
        add(out,static_cast<std::uint32_t>(c.active));add(out,c.vehicle);
        const auto& p=s.camera_presentation[i];add(out,p.up_axis);add(out,p.forward_axis);for(const auto& v:p.planes)add(out,v);
    }
    for(unsigned i=0;i<6;++i){auto snapshot=s.vehicles.camera_snapshot(i);CameraState dummy;
        Bytes c(camera_test::camera_size),v(camera_test::vehicle_size);camera_test::transfer(c,v,dummy,snapshot,false);
        out.insert(out.end(),v.begin(),v.end());for(auto value:s.vehicles.physics[i].camera_effects.values)add(out,value);
    }
    add(out,s.controls.current);add(out,s.current_slot);add(out,s.selected_camera_preset);
    add(out,s.camera_session.authored_cycle_index);add(out,s.camera_session.path_speed);add(out,s.camera_session.path_parameter);
    add(out,s.frame_projection);add(out,s.inverse_depth_scale);add(out,s.depth_offset);
    add(out,s.light_animation.phase_fast);add(out,s.light_animation.phase_slow);add(out,s.light_vertex_offset);
    add(out,s.light_fade.target);add(out,s.light_fade.current);add(out,s.light_fade.blend);add(out,s.light_fade.last_gamma);
    for(auto value:s.light_fade.ramp)add(out,static_cast<std::uint32_t>(value));
    add(out,s.light_selection.active_count);for(auto value:s.light_selection.ordered)add(out,value?*value+1:0);
    for(const auto& light:s.lights.records){const auto bytes=std::bit_cast<std::array<std::uint8_t,sizeof(light)>>(light);out.insert(out.end(),bytes.begin(),bytes.end());}
    out.insert(out.end(),s.light_vertices.begin(),s.light_vertices.end());
    const auto& q=s.contact_cast;add(out,q.start);add(out,q.end);add(out,q.step);add(out,q.direction);add(out,q.center);add(out,q.surface);
    add(out,q.motion_payload);add(out,q.step_length);add(out,q.travel_limit);add(out,q.remaining);add(out,q.radius);add(out,q.radius_squared);
    add(out,q.polygon?*q.polygon+1:0);add(out,q.hit_flag);add(out,q.polygon_list);add(out,q.mode);
    add(out,s.contact_workspace.query_mass);add(out,s.contact_workspace.query_motion);add(out,s.contact_workspace.last_normal);
    const auto viewport=std::bit_cast<std::array<std::uint8_t,sizeof(ViewportState)>>(s.viewport);out.insert(out.end(),viewport.begin(),viewport.end());
    return out;
}
struct Stages final:CameraFrameStages {
    RaceSessionState& s;CameraPathInput input;float parameter{},speed{};unsigned order{};bool path{};
    Stages(RaceSessionState& state,CameraPathInput keys):s(state),input(keys),parameter(s.camera_session.path_parameter),speed(s.camera_session.path_speed),
        path(s.controls.controllers[s.controls.current].camera.mode==3){}
    void stage_entities_442500(const CameraFrameSnapshot& first) override{
        require(order++==0,"entity stage order");
        if(path)camera_advance_path(parameter,speed,input);
        require(s.camera_session.path_parameter==parameter&&s.camera_session.path_speed==speed,"first view did not advance exactly once");
        require(first.camera().position.x==s.controls.controllers[s.controls.current].camera.position.x,"entity stage snapshot stale");
    }
    void prepare_world_42e870() override{require(order++==1,"world preparation stage order");
        require(s.viewport.applied_selector==s.viewport.requested_selector,"viewport selector must run before world preparation");
        reset_view_matrix_cursor(s.view_matrices);}
    void consume_model_views(const CameraFrameSnapshot&) override{require(order++==2,"model consumer stage order");}
    void verify(){
        require(order==3,"normal route omitted required stages");if(path)camera_advance_path(parameter,speed,input);
        require(s.camera_session.path_parameter==parameter&&s.camera_session.path_speed==speed,"second view did not advance exactly once");
    }
};
void source_frame(Fixture& f,unsigned index){
    auto& s=*f.state;auto world=f.view();camera_frame_begin_outer(world);
    // Deterministic test observations, not a replacement world simulation.
    s.vehicles.physics[0].velocity={float(index%7),0,float(index%11)};
    s.vehicles.race[0].progress=0.002f*float(index);
    camera_frame_before_world(world);
    // Stand-in for the world owner AFTER405210: camera must not write stale
    // physical observations back after this update.
    s.vehicles.physics[0].position.x+=3;
    const float physical=s.vehicles.physics[0].position.x;
    const CameraPathInput keys{index%17==0,index%29==0};Stages stages(s,keys);
    const auto fast=s.light_animation.phase_fast,slow=s.light_animation.phase_slow;
    const auto snapshot=camera_frame_normal_views(world,keys,stages,nullptr);stages.verify();
    require(s.light_animation.phase_fast==((fast+32)&8191)&&s.light_animation.phase_slow==((slow+2)&8191),"light cadence differs");
    require(s.vehicles.physics[0].position.x==physical,"camera overwrote new world observations");
    require(snapshot.active_lights()==s.light_selection.active_count,"snapshot did not capture final lighting");++source_frames;
}
using Transcript=std::vector<Bytes>;
Transcript cadence(const SplineSet& path,const contact::TrackGeometry& track,const Bytes& pmd,unsigned rate,bool jitter,std::uint32_t mode){
    Fixture f(path,track,pmd);f.state->controls.controllers[0].camera.mode=mode;
    Transcript result;std::uint64_t elapsed{};unsigned completed{},presentations{};
    constexpr std::uint64_t ticks_per_second=7920000,source_interval=ticks_per_second/modern_camera_source_frame_hz;
    constexpr unsigned count=180;
    while(completed<count){
        elapsed+=ticks_per_second/rate;
        if(jitter&&presentations%5==0)elapsed+=ticks_per_second/rate;
        while(completed<count&&std::uint64_t(completed+1)*source_interval<=elapsed){source_frame(f,completed++);result.push_back(state_bytes(f));}
        const auto before=state_bytes(f);auto world=f.view();const auto cached=camera_frame_snapshot(world);
        static_assert(std::is_same_v<decltype(cached.camera()),const CameraState&>);
        const auto remembered=cached.camera().position;
        for(unsigned i=0;i<3;++i){const auto another=camera_frame_snapshot(world);
            require(camera_bytes(another.camera(),f.state->camera_lifecycle[another.controller()])==camera_bytes(cached.camera(),f.state->camera_lifecycle[cached.controller()]),"GPU capture changed snapshot");
            require(another.world_to_view()==cached.world_to_view()&&another.camera().position.x==remembered.x,"GPU output not stable");++gpu_reads;
        }
        require(state_bytes(f)==before,"GPU read advanced persistent source state");++presentations;
    }
    return result;
}
void controls(const SplineSet& path,const contact::TrackGeometry& track,const Bytes& pmd){
    Fixture f(path,track,pmd);auto& s=*f.state;auto world=f.view();
    require(&world.vehicles==&s.vehicles&&&world.controls==&s.controls&&&world.contact_workspace==&s.contact_workspace&&
        &world.contact_cast==&s.contact_cast&&&world.frame_projection==&s.frame_projection&&&world.session==&s.camera_session&&
        world.light_vertices.data()==s.light_vertices.data()&&&world.overlays==&s.overlays,"session view duplicated owners");
    s.controls.current=2;s.controls.controllers[2].camera.angular_acceleration={1,2,3,4};
    s.controls.controllers[0].camera.angular_acceleration={5,6,7,8};camera_frame_begin_outer(world);
    require(s.controls.controllers[2].camera.angular_acceleration.x==0&&s.controls.controllers[0].camera.angular_acceleration.x==5,"outer reset changed wrong controller");
    s.controls.current=0;auto& first=s.controls.controllers[0].camera;
    s.frame_projection.fill(123);s.contact_workspace.query_mass=9;camera_frame_before_world(world);
    require(s.frame_projection[0]==123,"projection was published without a source writer");
    require(s.contact_workspace.query_mass==9,"bypassed camera changed shared query mass");
    s.controls.controllers[1].active=1;auto& last=s.controls.controllers[1].camera;
    first.requested_lens=2;first.pending_lens=1;last.requested_lens=last.lens;last.pending_lens=1;
    camera_frame_before_world(world);
    require(s.frame_projection==last.projection&&s.inverse_depth_scale==last.inverse_depth_scale&&s.depth_offset==last.depth_offset,"unchanged later lens writer failed to publish shared projection");
    s.controls.controllers[1].active=0;first.mode=11;first.pending_follow_setup=1;
    const auto position=s.vehicles.physics[0].position;camera_frame_before_world(world);
    require(s.vehicles.race[0].saved_camera_mode==5&&s.selected_camera_preset==2,"camera preset was not written to live vehicle state");
    require(s.vehicles.physics[0].camera_effects.values[0]==0,"camera reset did not reach physical owner");
    require(s.vehicles.physics[0].position.x==position.x,"camera settings publication changed physical position");
    const auto snapshot=camera_frame_snapshot(world);const auto captured=snapshot.camera().position.x;
    first.position.x+=10;require(snapshot.camera().position.x==captured,"GPU snapshot retained mutable owner reference");
    // Near, stationary real track cast reaches4215B0 and must publish mass/query
    // even though zero travel legitimately gives no hit.
    first.mode=1;first.position={position.x,position.y,position.z,position.w};first.velocity={};first.collision_bypass_ticks=0;
    s.contact_workspace.query_mass=9;s.contact_cast.radius=17;camera_frame_before_world(world);
    require(s.contact_workspace.query_mass==1&&s.contact_cast.radius==125&&s.contact_cast.radius_squared==15625&&s.contact_cast.mode==1,"live camera cast did not update shared query owner");
}
void sequential_barriers(const std::filesystem::path& root,const SplineSet& path,const contact::TrackGeometry& track,const Bytes& pmd){
    const auto prefix=[](Fixture& f,unsigned index){auto& s=*f.state;auto& c=s.controls.controllers[index];auto& v=s.vehicles.physics[c.vehicle];
        c.camera.previous_target=c.camera.target;auto& orbit=s.vehicles.camera_settings[c.vehicle].orbit_offset;
        orbit.x=0;orbit.y=100;orbit.z=0;
        // This fixture has identity vehicle axes. Model-independent stores at404D09..404E19.
        c.camera.position.x=v.position.x;c.camera.position.y=v.position.y+100;c.camera.position.z=v.position.z;
    };
    for(const auto mode:{6u,7u}){Fixture actual(path,track,pmd),expected(path,track,pmd);
        for(auto* fixture:{&actual,&expected}){auto& s=*fixture->state;s.controls.controllers[3].camera.mode=mode;
            if(mode==7)s.camera_presentation[3].spline.reset();}
        actual.state->controls.controllers[3].active=1;
        auto e=expected.view();camera_frame_before_world(e); //complete the earlier active controller
        expected.state->controls.controllers[3].active=1;expected.state->controls.current=3;
        if(mode==6)prefix(expected,3);
        auto a=actual.view();bool rejected=false;try{camera_frame_before_world(a);}catch(const FormatError&){rejected=true;}
        require(rejected&&state_bytes(actual)==state_bytes(expected),"late missing model/route did not retain the exact sequential controller state");
    }
    AssetStore assets(root);ModelResources models(assets,root);const auto handle=models.resolve(models.load("car1"));require(handle.has_value(),"sequential legacy model handle");
    Fixture actual(path,track,pmd),expected(path,track,pmd);
    for(auto* fixture:{&actual,&expected}){auto& s=*fixture->state;
        for(unsigned i=0;i<2;++i){s.controls.controllers[i].camera.mode=6;s.vehicles.models[i].model=i?models.size()+10:*handle;}}
    actual.state->controls.controllers[1].active=1;
    auto e=camera_frame_world(*expected.state,path,track,&models);camera_frame_before_world(e);
    expected.state->controls.controllers[1].active=1;expected.state->controls.current=1;prefix(expected,1);
    auto a=camera_frame_world(*actual.state,path,track,&models);bool rejected=false;try{camera_frame_before_world(a);}catch(const std::out_of_range&){rejected=true;}
    require(rejected&&state_bytes(actual)==state_bytes(expected),"late invalid model identity lost complete earlier controller or current mounted prefix");
}
void dispatcher_comparison(const SplineSet& path,const contact::TrackGeometry& track,const Bytes& pmd){
    // Compare the new session adapter against the separately source-qualified
    //405210 helper, including four cameras sharing a target. This does not turn
    // this native composition check into a new instruction-emulation result.
    constexpr std::array<std::uint32_t,14> modes{1,2,3,4,5,8,9,10,11,12,13,14,15,16};
    for(unsigned n=0;n<84;++n){Fixture actual(path,track,pmd),expected(path,track,pmd);
        for(auto* f:{&actual,&expected}){auto& s=*f->state;s.current_slot=n%6;
            s.camera_bank.records[0]={{1000,2000,3000,1},{2000,2000,3000,1},{0,1,0,0},2,1,0,0};
            for(unsigned i=0;i<6;++i)s.vehicles.camera_settings[i].adaptive_distance=n%2;
            for(unsigned i=0;i<4;++i){auto& c=s.controls.controllers[i];c.active=1;c.vehicle=n%3?i:n%6;
                c.camera.mode=modes[n%modes.size()];c.camera.pending_follow_setup=1;c.camera.collision_bypass_ticks=600;
                if(n%3==0){s.camera_lifecycle[i].pending_record=0;s.camera_lifecycle[i].pending_mode=c.camera.mode;}
            }
        }
        auto world=actual.view();camera_frame_before_world(world);
        auto& e=*expected.state;std::array<CameraVehicle,6> observed;std::array<CameraRaceContext,6> races;CameraEffectTargets targets;
        for(unsigned i=0;i<6;++i)observed[i]=e.vehicles.camera_snapshot(i);
        for(unsigned i=0;i<6;++i){const auto other=(observed[i].adaptive_distance&&i<5)?i+1:i;
            races[i]={observed[other].progress,observed[other].speed,e.clock.course_length};targets.vehicles[i]=&e.vehicles.physics[i];targets.active[i]=1;}
        auto rig=camera_controls_rig(e.controls,e.camera_lifecycle,e.current_slot);
        camera_tick(rig,observed,races,{0,-1,0,0},e.camera_bank,e.model_draws.template_pose,e.selected_camera_preset,camera_vehicle_reset_adapter(targets),{},e.shared_model_identity);
        for(unsigned i=0;i<6;++i)e.vehicles.store_camera_settings(i,observed[i]);
        for(unsigned i=0;i<4;++i)require(camera_bytes(actual.state->controls.controllers[i].camera,actual.state->camera_lifecycle[i])==
            camera_bytes(e.controls.controllers[i].camera,e.camera_lifecycle[i]),"session dispatcher differs from qualified helper");
        for(unsigned i=0;i<6;++i){const auto a=actual.state->vehicles.camera_snapshot(i),b=e.vehicles.camera_snapshot(i);
            require(std::bit_cast<std::array<std::uint8_t,sizeof(VehicleCameraSettings)>>(actual.state->vehicles.camera_settings[i])==
                std::bit_cast<std::array<std::uint8_t,sizeof(VehicleCameraSettings)>>(e.vehicles.camera_settings[i])&&a.camera_mode==b.camera_mode,
                "session dispatcher changed per-vehicle settings relative to helper");
            require(actual.state->vehicles.physics[i].camera_effects.values==e.vehicles.physics[i].camera_effects.values,"session reset owner differs");
        }
        require(actual.state->selected_camera_preset==e.selected_camera_preset&&actual.state->controls.current==e.controls.current,"session global selector differs");
    }
}
void source_collision_positions(const std::filesystem::path& mesh_directory,const std::filesystem::path& corpus){
    std::ifstream in(corpus,std::ios::binary);
    const auto read=[&](void* target,std::size_t size){in.read(static_cast<char*>(target),static_cast<std::streamsize>(size));if(!in)throw FormatError("truncated source collision corpus");};
    const auto word=[&](){std::uint32_t value;read(&value,4);return value;};
    std::array<char,8> magic;read(magic.data(),8);require(std::string_view(magic.data(),8)=="SGNCC001","wrong collision corpus");
    while(const auto length=word()){
        if(length>256)throw FormatError("bad source collision mesh name");std::string name(length,'\0');read(name.data(),length);
        if(name.find_first_of("/\\:")!=name.npos)throw FormatError("source collision path outside fixture meshes");
        const auto pmd=read_file(mesh_directory/name);const SplineSet path(pmd);const auto track=contact::decode_track_collision(pmd);Fixture f(path,track,pmd);
        const auto count=word();if(count>10000)throw FormatError("too many collision fixtures");
        for(std::uint32_t index=0;index<count;++index){const auto kind=word();CameraSweep sweep;CameraVector expected;
            read(&sweep.position,16);read(&sweep.velocity,16);read(&sweep.radius,4);const auto hit=word();read(&expected,16);
            if(kind==0)continue;if(kind!=1||hit||sweep.radius!=125)throw FormatError("bad403460 collision fixture");
            auto& s=*f.state;auto& c=s.controls.controllers[0].camera;c={};s.camera_lifecycle[0]={};
            c.mode=1;c.position=sweep.position;c.velocity=sweep.velocity;
            s.vehicles.physics[0].position={sweep.position.x,sweep.position.y,sweep.position.z,sweep.position.w};
            auto world=f.view();camera_frame_before_world(world);
            require(std::bit_cast<std::array<std::uint32_t,4>>(c.position)==std::bit_cast<std::array<std::uint32_t,4>>(expected),"new persistent cast differs from actual403460 position");
            require(s.contact_workspace.query_mass==1&&s.contact_cast.mode==1,"camera cast failed to publish persistent scalar/mode");++collision_cases;
        }
    }
    require(in.peek()==std::char_traits<char>::eof()&&collision_cases==960,"incomplete original collision fixture coverage");
}
void mounted_bindings(const std::filesystem::path& root,const SplineSet& path,const contact::TrackGeometry& track,const Bytes& pmd){
    AssetStore assets(root);ModelResources models(assets,root);
    const auto first=models.resolve(models.load("car1")),second=models.resolve(models.load("car16"));
    require(first&&second,"mounted fixture model handles");
    Fixture actual(path,track,pmd),expected(path,track,pmd);
    for(auto* fixture:{&actual,&expected}){
        auto& s=*fixture->state;
        for(unsigned i=0;i<6;++i)s.vehicles.models[i].model=i%2?second:first;
        for(unsigned i=0;i<4;++i){auto& c=s.controls.controllers[i];c.active=1;c.vehicle=i%2;c.camera.mode=6;
            c.camera.position.w=std::bit_cast<float>(0xa1234567u+i);c.camera.up.w=std::bit_cast<float>(0xb1234567u+i);
            c.camera.target.w=std::bit_cast<float>(0xc1234567u+i);c.camera.collision_bypass_ticks=i;
        }
        // Deliberately invalid render detail must not be consulted by404D01.
        s.selected_lod.fill(0xffffffffu);
    }
    auto world=camera_frame_world(*actual.state,path,track,&models);
    require(world.models==&models,"mounted binder did not borrow resource owner");
    for(unsigned iteration=0;iteration<8;++iteration){
        for(const auto handle:{*first,*second}){
            auto& model=models.at(handle);auto& pose=model.part(model.source_lod_index(0),0).transform;
            pose.up={0.4f+float(iteration)*0.1f,-0.7f,1.1f};pose.forward={-0.6f,0.8f,float(handle)+0.3f};
        }
        if(iteration==3)for(auto* fixture:{&actual,&expected}){
            auto& s=*fixture->state;s.controls.controllers[3].camera.mode=11;
            s.camera_lifecycle[3].pending_record=0;s.camera_lifecycle[3].pending_mode=6;
        }
        camera_frame_before_world(world);
        auto& s=*expected.state;std::array<CameraVehicle,6> observed;
        for(unsigned i=0;i<6;++i)observed[i]=s.vehicles.camera_snapshot(i);
        for(unsigned i=0;i<4;++i){auto& c=s.controls.controllers[i];auto& v=observed[c.vehicle];
            const bool pending=s.camera_lifecycle[i].pending_record.has_value();camera_apply_record(c.camera,s.camera_lifecycle[i],v,s.camera_bank,s.model_draws.template_pose,s.shared_model_identity);
            if(pending){s.frame_projection=c.camera.projection;s.inverse_depth_scale=c.camera.inverse_depth_scale;s.depth_offset=c.camera.depth_offset;}
            auto& model=models.at(*s.vehicles.models[c.vehicle].model);const auto& pose=model.part(model.source_lod_index(0),0).transform;
            camera_mounted_tick(c.camera,v,{{pose.up.x,pose.up.y,pose.up.z,std::bit_cast<float>(pose.up_w)},
                {pose.forward.x,pose.forward.y,pose.forward.z,std::bit_cast<float>(pose.forward_w)}});
            camera_integrate_velocity(c.camera);camera_integrate_angles(c.camera);
        }
        s.controls.current=0;for(unsigned i=0;i<6;++i)s.vehicles.store_camera_settings(i,observed[i]);
        require(state_bytes(actual)==state_bytes(expected),"mounted dispatcher lost live model, shared target, pending mode or unrelated owner");
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw sgn::FormatError("usage: camera frame tests track PMD source collision corpus");
    const auto pmd=sgn::read_file(argv[1]);const SplineSet path(pmd);const auto track=contact::decode_track_collision(pmd);
    require(path.find(1).has_value(),"actual PMD has no camera path");controls(path,track,pmd);dispatcher_comparison(path,track,pmd);
    mounted_bindings(std::filesystem::path(argv[1]).parent_path().parent_path(),path,track,pmd);
    sequential_barriers(std::filesystem::path(argv[1]).parent_path().parent_path(),path,track,pmd);
    source_collision_positions(std::filesystem::path(argv[1]).parent_path(),argv[2]);
    for(const auto mode:{3u,11u}){
        const auto reference=cadence(path,track,pmd,60,false,mode);
        for(auto rate:{30u,120u,144u,165u,240u,360u}){const auto actual=cadence(path,track,pmd,rate,false,mode);
            require(actual==reference,"display rate changed source-frame state");for(const auto& frame:actual)exact_words+=frame.size()/4;}
        const auto irregular=cadence(path,track,pmd,144,true,mode);require(irregular==reference,"jitter changed source-frame state");
        for(const auto& frame:irregular)exact_words+=frame.size()/4;
    }
    std::cout<<"{\"checks\":"<<checks<<",\"source_frames\":"<<source_frames<<",\"gpu_reads\":"<<gpu_reads
        <<",\"exact_state_words\":"<<exact_words<<",\"source_collision_cases\":"<<collision_cases<<",\"dispatcher_cases\":84,\"modern_source_hz\":"<<modern_camera_source_frame_hz<<",\"source_cadence_claim\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"camera frame: "<<e.what()<<'\n';return 1;}
