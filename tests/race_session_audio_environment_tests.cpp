#include "sgn/playable_driver.hpp"
#include "sgn/playable_camera.hpp"
#include "sgn/playable_scene.hpp"
#include "sgn/presentation_interpolation.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include <bit>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <windows.h>

using namespace sgn;
using namespace sgn::retail;
namespace {
struct Checks {
    unsigned count{},sessions{},camera_samples{},region_changes{},cpu_view_ticks{},snapshot_reads{},maintenance_calls{},retained_snapshot_checks{};
    void operator()(bool ok,const char* message){if(!ok)throw FormatError(message);++count;}
};
class Images final:public TextureReplacementDevice {
    Image image_;
public:
    explicit Images(const AssetStore& assets):image_(assets.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(std::uint32_t slot)const override{
        if(slot!=15||image_.width>65535)throw FormatError("audio Session overlay target");return static_cast<std::uint16_t>(image_.width);
    }
    void upload_to_destination(std::uint32_t slot,const Image& image)override{
        if(slot!=15)throw FormatError("audio Session overlay upload target");image_=image;
    }
};
template<class T> void append(Bytes& out,const T& value){
    static_assert(std::is_trivially_copyable_v<T>);const auto* first=reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(),first,first+sizeof(value));
}
Bytes audio_snapshot(const RaceSessionState& s){
    Bytes out;append(out,s.audio_environment);append(out,s.audio_driver);append(out,s.audio_maintenance);
    append(out,s.audio_stream);append(out,s.audio_level);append(out,s.music);
    append(out,s.audio_initialization.device);append(out,s.audio_initialization.primary);
    append(out,s.audio_initialization.spatial_enabled);append(out,s.audio_initialization.spatial_capability);
    return out;
}
Bytes environment_snapshot(const AudioEnvironmentState& state){Bytes out;append(out,state);return out;}
std::uint64_t presentation_digest(const PresentationSnapshot& snapshot){
    //Diagnostic hash of every retained render value/vertex, not a provenance
    //hash. The outer runner pins the actual executable and all source files.
    std::uint64_t hash=14695981039346656037ull;
    const auto bytes=[&](const void* data,std::size_t size){const auto* p=static_cast<const std::uint8_t*>(data);
        for(std::size_t i=0;i<size;++i){hash^=p[i];hash*=1099511628211ull;}};
    const auto value=[&]<class T>(const T& v){static_assert(std::is_trivially_copyable_v<T>);bytes(&v,sizeof(v));};
    value(snapshot.tick);value(snapshot.camera_continuity);value(snapshot.view);value(snapshot.objects.size());
    for(const auto& object:snapshot.objects){
        value(object.identity);value(object.continuity);value(object.mesh);value(object.pose);
        value(object.parts.size());for(const auto& part:object.parts)value(part);
        value(object.model_to_view.size());for(const auto& matrix:object.model_to_view)value(matrix);
        if(!object.geometry)throw FormatError("snapshot lost its geometry owner");
        const auto& g=*object.geometry;value(g.model.vertices.size());value(g.model.surfaces.size());
        bytes(g.model.vertices.data(),g.model.vertices.size()*sizeof(Vertex));
        bytes(g.model.surfaces.data(),g.model.surfaces.size()*sizeof(Surface));
        value(g.model.minimum);value(g.model.maximum);value(g.model.kind);value(g.model.lod_count);value(g.model.parts);
        for(const auto& name:g.model.textures){value(name.size());bytes(name.data(),name.size());}
        value(g.materials.size());for(const auto& material:g.materials)value(material);
    }
    return hash;
}
void pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
std::uint32_t selected_at(const AudioEnvironmentState& state,Vec3 position){
    //Use the separately source-qualified geometric predicate to compute the
    //expected selection; this test checks real Session ownership and cadence.
    for(std::uint32_t i=1;i<state.regions.size();++i){
        if(state.regions[i].type==0xffffffffu)return 0;
        if(audio_environment_contains(state.regions[i],position))return i;
    }
    throw FormatError("authored environment list has no terminator");
}
void session_case(const std::filesystem::path& root,SessionMode mode,std::string level,Checks& check){
    RaceSessionLaunch launch;launch.mode=mode;launch.level_name=level;launch.master_gain=0;
    launch.music_percent=100;RaceSession session(root,launch);auto& s=session.state();++check.sessions;
    auto world=audio_environment_world(session);
    check(&world.state==&s.audio_environment&&&world.driver==&s.audio_driver&&
        &world.spatial_enabled==&s.audio_initialization.spatial_enabled&&&world.device==static_cast<AudioEnvironmentDevice*>(&session.audio_device()),
        "environment view does not borrow the actual Session owners");
    check(s.audio_driver.initialized==1&&s.audio_initialization.spatial_enabled==1&&s.audio_driver.samples[1].spatial.value!=0,
        "real Session has no initialized native spatial sample");
    auto expected=initial_audio_environment_state();AudioEnvironmentWorld expected_world{expected,s.audio_driver,s.audio_initialization.spatial_enabled,session.audio_device()};
    load_audio_environments(session.assets().resource("config\\levels\\"+canonical(level)+".cfg"),expected_world);
    check(environment_snapshot(expected)==environment_snapshot(s.audio_environment),"Session loader differs from selected authored configuration");
    check(s.audio_environment.count>1&&s.audio_environment.count<32,"test requires authored environment regions");
    AudioPropertyId unavailable{99};
    check(session.audio_device().query_audio_property(s.audio_driver.samples[1].spatial,audio_property_interface,unavailable)==
        std::bit_cast<std::int32_t>(0x80004002u)&&unavailable.value==0,"XAudio2 reported invented EAX support");

    const auto original_camera0=s.controls.controllers[0].camera,original_camera3=s.controls.controllers[3].camera;
    const auto original_current=s.controls.current;
    const Vec3 outside{1000000,1000000,1000000};
    const auto move=[&](unsigned controller,Vec3 point){
        s.controls.current=controller;auto& p=s.controls.controllers[controller].camera.position;p={point.x,point.y,point.z,p.w};
        const auto previous=s.audio_environment.current_region;const auto wanted=selected_at(s.audio_environment,point);
        const auto prior=s.audio_environment.properties;const auto mask=s.audio_environment.supported_properties;
        maintain_session_audio(session);++check.maintenance_calls;++check.camera_samples;
        check(s.audio_environment.current_region==wanted,"live current-controller camera selected wrong region");
        check(s.audio_environment.supported_properties==mask,"unsupported native backend published EAX support");
        if(previous!=wanted){++check.region_changes;
            AudioEnvironmentState expected_change=s.audio_environment;expected_change.properties=prior;
            AudioEnvironmentWorld expected_change_world{expected_change,s.audio_driver,s.audio_initialization.spatial_enabled,session.audio_device()};
            (void)set_audio_environment(expected_change_world,s.audio_environment.regions[wanted].type);
            check(expected_change.properties==s.audio_environment.properties,"live region change missed its original preset");
        }
        const auto selected_snapshot=environment_snapshot(s.audio_environment);maintain_session_audio(session);++check.maintenance_calls;
        check(environment_snapshot(s.audio_environment)==selected_snapshot,"unchanged region mutated environment state");
    };
    for(std::uint32_t i=1;i<s.audio_environment.count;++i){
        const auto p=s.audio_environment.regions[i].position;
        move(0,{p.x,p.y,p.z});move(0,outside);
        //Controller0 deliberately remains outside while current controller3
        //enters a region. Hardcoding the primary camera would fail this case.
        move(3,{p.x,p.y,p.z});move(3,outside);
    }
    const auto p=s.audio_environment.regions[1].position;
    s.audio_initialization.spatial_enabled=0;const auto disabled=s.audio_environment.properties;
    move(3,{p.x,p.y,p.z});check(s.audio_environment.properties==disabled,"disabled audio rewrote environment preset");
    s.audio_initialization.spatial_enabled=1;
    s.controls.controllers[0].camera=original_camera0;s.controls.controllers[3].camera=original_camera3;s.controls.current=original_current;

    Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());PlayableDriver driver(session,textures,{});
    PlayableScene scene(session);PresentationInterpolator interpolation;ImmutablePresentationSnapshot retained;std::uint64_t retained_digest{};
    for(unsigned tick=0;tick<360;++tick){
        pump();playable_camera_begin_tick(session);driver.tick({});
        const auto& camera=s.controls.controllers.at(s.controls.current).camera;
        const auto wanted=selected_at(s.audio_environment,{camera.position.x,camera.position.y,camera.position.z});
        maintain_session_audio(session);++check.maintenance_calls;
        check(s.audio_environment.current_region==wanted,"actual world/camera tick did not update environment");
        const auto before=audio_snapshot(s);scene.update_after_tick(session);++check.cpu_view_ticks;
        const auto snapshot=scene.snapshot();
        check(snapshot&&snapshot->tick==s.simulation_tick&&!snapshot->objects.empty(),"CPU scene failed to publish current source tick");
        const auto current_digest=presentation_digest(*snapshot);
        interpolation.publish(snapshot);
        check(audio_snapshot(s)==before,"CPU normal-view preparation mutated audio owners");
        if(retained){check(presentation_digest(*retained)==retained_digest,"new source tick mutated previously published render geometry");++check.retained_snapshot_checks;}
        const auto random=s.random.state,simulation_tick=s.simulation_tick;
        for(unsigned repeat=0;repeat<12;++repeat){
            const auto current=scene.snapshot();
            check(current==snapshot&&current->view.retail_viewport.has_value()&&std::isfinite(current->view.eye.x),"immutable CPU presentation read changed publication");
            check(scene.view().eye.x==current->view.eye.x&&!scene.instances().empty()&&!scene.live_views().empty(),"legacy Present readers differ from snapshot");
            const auto& draw=interpolation.sample(double(repeat)/11.0);
            check(draw.instances.size()==draw.live_views.size()&&!draw.instances.empty()&&std::isfinite(draw.view.eye.x),"CPU Present consumer produced invalid draw data");
            ++check.snapshot_reads;
        }
        check(audio_snapshot(s)==before&&s.random.state==random&&s.simulation_tick==simulation_tick,"repeated presentation reads advanced source state");
        check(presentation_digest(*snapshot)==current_digest,"CPU Present interpolation mutated its current immutable snapshot");
        retained=snapshot;retained_digest=current_digest;
    }
    check(s.audio_environment.supported_properties==0,"real Session ended with fabricated EAX support");
    check(session.audio_device().health()==0,"native audio device failed during Session test");
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw FormatError("expected retail asset root");Checks check;
    session_case(argv[1],SessionMode::normal_race,"TRACK3",check);
    session_case(argv[1],SessionMode::time_trial,"TRACK8",check);
    check(check.region_changes>=8,"insufficient real Session region transitions");
    std::cout<<"{\"passed\":true,\"checks\":"<<check.count<<",\"sessions\":"<<check.sessions
        <<",\"camera_samples\":"<<check.camera_samples<<",\"region_changes\":"<<check.region_changes
        <<",\"cpu_view_ticks\":"<<check.cpu_view_ticks<<",\"snapshot_reads\":"<<check.snapshot_reads
        <<",\"maintenance_calls\":"<<check.maintenance_calls
        <<",\"retained_snapshot_checks\":"<<check.retained_snapshot_checks
        <<",\"interpolation_samples\":"<<check.snapshot_reads
        <<",\"gpu_used\":false,\"dxgi_present_executed\":false,\"eax_dsp\":false,\"master_gain\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
