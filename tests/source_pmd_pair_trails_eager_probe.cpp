#include "sgn/model_resources.hpp"
#include <cmath>
#include <iostream>
using namespace sgn;using namespace sgn::retail;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("expected eager pair mode, stage and retail root");const std::string_view mode=argv[1],stage=argv[2];
    if((mode!="early"&&mode!="disabled"&&mode!="active")||(stage!="a"&&stage!="b"))throw std::runtime_error("unknown eager pair probe");
    AssetStore assets(argv[3]);ModelResources models(assets,argv[3]);const auto handle=*models.resolve(models.load("car1sh"));auto& model=models.at(handle);
    //Verify the real model has usable part0 before demonstrating the unrelated part23 restriction.
    const auto& first=model.part(0,0).transform;
    if(!std::isfinite(first.forward.x)||!std::isfinite(first.forward.y)||!std::isfinite(first.forward.z)||
       !std::isfinite(first.up.x)||!std::isfinite(first.up.y)||!std::isfinite(first.up.z))throw std::runtime_error("car1sh part0 is not finite");
    VehicleState physical{};VehicleParameters parameters{};VehicleTrailStorage storage{};VehicleMatrix basis=vehicle_identity();VehicleTrailInputs inputs{};VehicleTrailProjection projection{};
    physical.camera_effects.values[4]=5;physical.camera_effects.values[5]=7;physical.camera_effects.values[6]=11;physical.camera_effects.values[7]=13;
    inputs.field_28a0=mode=="disabled"?0u:1u;const auto settings=initial_vehicle_trail_settings();const auto tick=mode=="early"?59u:60u;
    try{const auto trail_model=model.trail_model(); //exact old producer, frozen baseline implementation
        const VehicleTrailBindings binding{physical,parameters,storage,basis,inputs,trail_model,projection,settings,tick};
        if(stage=="a")update_vehicle_pair_a_trails(binding);else update_vehicle_pair_b_trails(binding);
    }catch(const FormatError& e){if(std::string_view(e.what())!="vehicle trails need source LOD-zero parts 0..23")throw;
        if(physical.camera_effects.values[4]!=5||physical.camera_effects.values[5]!=7||physical.camera_effects.values[6]!=11||physical.camera_effects.values[7]!=13)throw std::runtime_error("unexpected baseline counter writes");
        std::cout<<"RED eager pair "<<stage<<' '<<mode<<": "<<e.what()<<"; car1sh part0 verified; counters unchanged\n";return 1;}
    std::cout<<"baseline unexpectedly reached pair stage\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
