#include "sgn/vehicle_model_effects.hpp"
#include <iostream>
using namespace sgn;using namespace sgn::retail;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("expected eager launch mode and retail root");const std::string_view mode=argv[1];
    if(mode!="unavailable"&&mode!="disabled")throw std::runtime_error("unknown eager launch mode");
    AssetStore assets(argv[2]);ModelResources models(assets,argv[2]);const auto handle=*models.resolve(models.load("car1sh"));auto& model=models.at(handle);
    VehicleState physical{};VehicleParameters parameters{};VehicleTrailStorage storage{};VehicleMatrix basis=vehicle_identity();VehicleTrailInputs inputs{};VehicleTrailProjection projection{};
    VehicleModelEffects effects{};effects.enabled=7;VehicleAnchorState anchor{};anchor.effect_available[0]=mode=="unavailable"?2u:1u;
    contact::effects::ImpactParticles particles{};VehicleRandom random{};const auto settings=initial_vehicle_trail_settings();VehicleModelEffectResourceView view(model,0);
    try{const auto trail_model=model.trail_model(); //frozen genuine eager producer used by the pre-migration callers
        const VehicleTrailBindings b{physical,parameters,storage,basis,inputs,trail_model,projection,settings,90};
        update_vehicle_launch_effect(b,effects,anchor,view,particles,random);
    }catch(const FormatError& e){if(std::string_view(e.what())!="vehicle trails need source LOD-zero parts 0..23")throw;
        if(effects.enabled!=7||particles.emitted)throw std::runtime_error("unexpected frozen launch writes");
        std::cout<<"RED frozen eager launch "<<mode<<": "<<e.what()<<"; enabled7 unchanged before owner\n";return 1;}
    std::cout<<"baseline unexpectedly reached launch owner\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
