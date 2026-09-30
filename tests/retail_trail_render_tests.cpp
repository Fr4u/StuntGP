#include "sgn/retail_trail_render.hpp"
#include "sgn/retail_time_trial.hpp"
#include <cstring>
#include <iostream>
#include <memory>
using namespace sgn::retail;
int main()try{
    unsigned checks=0;const auto check=[&](bool value){++checks;if(!value)throw std::runtime_error("trail render contract");};
    auto profiles=initial_trail_profile_bank();const auto impact=impact_trail_parameters(profiles);const auto vehicle=vehicle_trail_settings(profiles);
    check(impact[0].capacity==7&&!impact[0].project_to_screen&&impact[4].capacity==3&&impact[4].project_to_screen);
    check(vehicle.speed.capacity==16&&!vehicle.speed.project_to_screen&&vehicle.pair_a.capacity==5&&vehicle.pair_b.capacity==9);
    check(trail_shape(profiles,1)[0].x==-35&&trail_shape(profiles,2)[2].x==50);
    const auto rejects=[&](auto action){bool caught=false;try{action();}catch(const sgn::FormatError&){caught=true;}check(caught);};
    rejects([&]{(void)trail_shape(profiles,0);});rejects([&]{(void)trail_shape(profiles,99);});
    auto duplicate=profiles;duplicate.shapes[1].identity=1;rejects([&]{(void)trail_shape(duplicate,1);});
    auto sprites=std::make_unique<SourceSpritePacketStorage>();auto screen=std::make_unique<std::array<SourceScreenPolygon,256>>();unsigned count=0;
    SourceTrailPolygon staging{};ViewportState viewport;viewport.clip_screen={0,640,0,480};viewport.clip_expanded={-100,740,-100,580};
    CameraState camera;camera.camera_to_world_rotation=vehicle_identity();CameraLifecycle lifecycle;lifecycle.clip_far=10000;CameraPresentationState presentation;presentation.forward_axis={0,0,1,0};
    presentation.planes={CameraVector{1,0,-1,0},CameraVector{-1,0,-1,0},CameraVector{0,1,-1,0},CameraVector{0,-1,-1,0}};ControllerOverlayTransition overlay;
    const auto basis=vehicle_identity();TrailRenderBindings b{profiles,staging,*sprites,{MutableScreenPolygonBaseReader::fixed_fixture(*screen),count},viewport,{camera,lifecycle,presentation,overlay},basis,0};
    std::array<VehicleVector,64> ring{};for(unsigned j=0;j<64;++j)ring[j]={float(j),float(j),100,float(j+9)};
    append_retail_trail(b,profiles.profiles[0],ring,6,7,.5f,.5f);check(sprites->header_bytes==15*8&&count==0);
    *sprites={};staging={};b.split_setting=1;append_retail_trail(b,profiles.profiles[0],ring,6,7,.5f,.5f);check(sprites->header_bytes==8&&count==0);
    b.split_setting=0;*sprites={};for(unsigned j=0;j<5;++j)ring[j]={320+float(j)*4,240+float(j)*2,.5f,float(j)};ring[5]={0,0,100,1};
    append_retail_trail(b,profiles.profiles[2],ring,4,5,.5f,.5f);check(count==5&&sprites->header_bytes==8);
    const auto old_count=count,old_headers=sprites->header_bytes;auto invalid=profiles.profiles[2];invalid.stride=0;
    rejects([&]{append_retail_trail(b,invalid,ring,4,5,.5f,.5f);});check(count==old_count&&sprites->header_bytes==old_headers);
    std::cout<<"{\"checks\":"<<checks<<",\"errors\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
