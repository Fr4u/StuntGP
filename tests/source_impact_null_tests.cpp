#include "sgn/retail_trail_render.hpp"
#include "sgn/retail_time_trial.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using namespace sgn;
using namespace sgn::retail;
using namespace sgn::retail::contact;
namespace {
using List=effects::ImpactParticleDrawList;
unsigned checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const FormatError&){caught=true;}check(caught,"expected explicit invalid source domain");}
template<class T>void append(std::vector<std::uint8_t>& out,const T& value){
    const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(value));
}
struct Fixture {
    TrailProfileBank profiles=initial_trail_profile_bank();
    SourceTrailPolygon staging{};
    SourceSpritePacketStorage sprites{};
    std::array<SourceScreenPolygon,256> screen{};
    std::uint32_t screen_count{};
    ViewportState viewport;
    CameraState camera;
    CameraLifecycle lifecycle;
    CameraPresentationState presentation;
    ControllerOverlayTransition overlay;
    VehicleMatrix basis=vehicle_identity();
    effects::ImpactParticles particles;
    List list;
    TrailRenderBindings bindings(){return {profiles,staging,sprites,{MutableScreenPolygonBaseReader::fixed_fixture(screen),screen_count},viewport,{camera,lifecycle,presentation,overlay},basis,0};}
    Fixture(){
        viewport.clip_screen={0,640,0,480};viewport.clip_expanded={-100,740,-100,580};
        camera.camera_to_world_rotation=vehicle_identity();lifecycle.clip_far=10000;
        presentation.forward_axis={0,0,1,0};
        presentation.planes={CameraVector{1,0,-1,0},CameraVector{-1,0,-1,0},CameraVector{0,1,-1,0},CameraVector{0,-1,-1,0}};
    }
    std::vector<std::uint8_t> output()const{std::vector<std::uint8_t> out;append(out,staging);append(out,sprites);append(out,screen);append(out,screen_count);return out;}
    std::vector<std::uint8_t> inputs()const{std::vector<std::uint8_t> out;append(out,particles);append(out,list);return out;}
    void clear_output(){staging={};sprites={};screen={};screen_count=0;}
    void draw(){append_impact_particle_trails(bindings(),particles,list);}
};
void codec(){
    check(List::null_value==0xffffffffu,"normalized NULL value");
    check(List::encode_slot(List::null_value)==0&&List::decode_slot(0)==List::null_value,"slot NULL roundtrip");
    check(List::encode_cursor(List::null_value)==0&&List::decode_cursor(0)==List::null_value,"cursor NULL roundtrip");
    for(unsigned i=0;i<32;++i){const auto pointer=0x612da4u+i*0x440u;
        check(List::encode_slot(i)==pointer&&List::decode_slot(pointer)==i,"all32 source particle identities");
        check(i!=List::null_value&&pointer!=0,"NULL must not collide with a valid particle");
    }
    for(unsigned i=0;i<=32;++i){const auto pointer=0x61b5a4u+i*4u;
        check(List::encode_cursor(i)==pointer&&List::decode_cursor(pointer)==i,"all33 source cursor positions");
        check(i!=List::null_value&&pointer!=0,"NULL must not collide with published-empty list");
    }
    check(List::encode_slot(0)==0x612da4u&&List::encode_cursor(0)==0x61b5a4u,"normalized zero identities");
    for(auto value:{32u,33u,0x80000000u,0xfffffffeu})rejects([&]{(void)List::encode_slot(value);});
    for(auto value:{33u,34u,0x80000000u,0xfffffffeu})rejects([&]{(void)List::encode_cursor(value);});
    for(auto value:{1u,0x612da3u,0x612da5u,0x61b5a4u,0xffffffffu})rejects([&]{(void)List::decode_slot(value);});
    for(auto value:{1u,0x61b5a3u,0x61b5a5u,0x61b625u,0xffffffffu})rejects([&]{(void)List::decode_cursor(value);});
    List initial;check(initial.cursor_is_null()&&std::all_of(initial.slots.begin(),initial.slots.end(),[](auto v){return v==List::null_value;}),"PE NULL initialization");
    static_assert(sizeof(List)==33*sizeof(std::uint32_t));
}
void consume(){
    auto f=std::make_unique<Fixture>();
    f->list.slots.fill(0x80000000u);f->particles.particles[0].type=-1;
    f->sprites.header_bytes=0xffffff00u;f->screen_count=0x80000000u;
    const auto before=f->output(),inputs=f->inputs();f->draw();
    check(f->output()==before&&f->inputs()==inputs,"NULL cursor accessed invalid slots/profiles/output spans");
    f->list.count=0;const auto empty_input=f->inputs();f->draw();
    check(f->output()==before&&f->inputs()==empty_input&&!f->list.cursor_is_null(),"listbase/count0 is a distinct no-draw state");
    f->clear_output();f->list.count=1;f->list.slots[0]=List::null_value;
    const auto empty=f->output();rejects([&]{f->draw();});
    check(f->output()==empty,"first reached NULL slot produced output");
    auto& particle=f->particles.particles[0];particle={};particle.type=0;particle.lifetime=30;particle.scale=.5f;
    particle.field_430=6;particle.field_434=7;
    std::array<VehicleVector,64> ring{};for(unsigned i=0;i<64;++i)ring[i]={float(i),float(i),100,float(i+9)};
    particle.reserved=std::bit_cast<std::array<std::uint32_t,256>>(ring);
    f->list.slots[0]=0;const auto valid_input=f->inputs();f->draw();const auto once=f->output();
    check(f->sprites.header_bytes>0&&once!=empty&&f->inputs()==valid_input,"particle0 must really draw, never map to NULL");
    const auto headers=f->sprites.header_bytes;
    f->clear_output();f->list.count=2;f->list.slots[0]=List::null_value;f->list.slots[1]=0;
    const auto mixed_input=f->inputs();rejects([&]{f->draw();});
    check(f->output()==once&&f->inputs()==mixed_input,"reverse valid-then-NULL lost earlier source outputs or preflighted whole list");
    check(f->sprites.header_bytes==headers,"partial source appends count changed");
    f->clear_output();f->list.slots[0]=0;f->list.slots[1]=List::null_value;
    rejects([&]{f->draw();});check(f->output()==empty,"reverse NULL-then-valid must stop before valid particle");
    f->list.slots[0]=32;f->list.slots[1]=0;
    rejects([&]{f->draw();});check(f->output()==once,"late invalid identity discarded already appended geometry");
    std::cout<<"NULL_RENDER_ORDER_PASS valid_particle_headers="<<headers<<" partial_headers="<<f->sprites.header_bytes<<" rollback=0\n";
}
void rearm(){
    auto bank=std::make_unique<effects::ImpactParticles>();List list;
    solver::Workspace work;ContactCastState query;VehicleRandom random;TrackGeometry track;VehicleTrailProjection projection;
    const auto profiles=effects::initial_impact_trail_profiles();
    const auto run=[&]{effects::update_impact_particles({*bank,list,work,query,random,track,{},projection,profiles});};
    bank->throttle=1;run();check(list.count==0&&!list.cursor_is_null()&&List::encode_cursor(list.count)==0x61b5a4u&&bank->throttle==0,"415E08 did not rearm empty list afterNULL");
    check(std::all_of(list.slots.begin(),list.slots.end(),[](auto v){return v==List::null_value;}),"empty rearm replaced retained NULL slots");
    bank->emitted=1;bank->particles[0].lifetime=1;bank->particles[0].type=0;bank->particles[0].decay=1;
    run();check(list.count==1&&list.slots[0]==0&&List::encode_slot(list.slots[0])==0x612da4u&&bank->emitted==0,"expiry tick did not publish actual particle0");
    check(list.slots[1]==List::null_value,"producer overwrote unvisited suffix");
    run();check(list.count==0&&list.slots[0]==0&&list.slots[1]==List::null_value,"next empty publication confused retained particle0 andNULL");
}
}
int main()try{codec();consume();rearm();std::cout<<"IMPACT_NULL_PASS checks="<<checks<<" original_runs=0 native_backend_runs=0\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
