#include "sgn/model_resources.hpp"
#include <iostream>
using namespace sgn;using namespace sgn::retail;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("expected eager probe mode and retail root");const std::string_view mode=argv[1];if(mode!="early_tick"&&mode!="reset_slow")throw std::runtime_error("unknown eager probe");
    AssetStore assets(argv[2]);ModelResources models(assets,argv[2]);const auto handle=*models.resolve(models.load("car1sh"));auto& model=models.at(handle);
    VehicleState physical{};VehicleParameters parameters{};VehicleTrailStorage storage{};VehicleMatrix basis=vehicle_identity();VehicleTrailInputs inputs{};VehicleTrailProjection projection{};
    physical.speed=mode=="early_tick"?100.0f:0.0f;physical.camera_effects.values[2]=5;physical.camera_effects.values[3]=7;
    const auto settings=initial_vehicle_trail_settings();const auto tick=mode=="early_tick"?19u:20u;
    try{
        //Exact eager producer expression, compiled against the sealed baseline.
        const auto trail_model=model.trail_model();
        update_vehicle_speed_trail({physical,parameters,storage,basis,inputs,trail_model,projection,settings,tick});
    }catch(const FormatError& e){
        if(physical.camera_effects.values[2]!=5||physical.camera_effects.values[3]!=7)throw std::runtime_error("unexpected baseline writes before model rejection");
        std::cout<<"RED eager "<<mode<<": "<<e.what()<<"; counters remain 5,7\n";return 1;
    }
    std::cout<<"baseline unexpectedly reached source gate\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
