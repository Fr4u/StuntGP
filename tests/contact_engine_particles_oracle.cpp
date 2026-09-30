#include "world_template_fixture.hpp"
#include "sgn/contact_engine_particles.hpp"
#include "contact_emitter_harness.hpp"
#include "vehicle_oracle_state.hpp"

namespace {
using namespace sgn::retail;using namespace contact::emitter_oracle;
struct Model final:contact::effects::EngineParticleModel {
    std::array<ModelTransform,8> transforms{};std::array<std::uint16_t,8> flags{};std::array<unsigned,8> identities{};unsigned count{},lod{};
    std::uint32_t part_count()const override{return count;}
    unsigned selected_lod()const override{return lod;}
    contact::effects::EngineParticlePart part(unsigned selected,unsigned index)override{
        if(selected>=4||index<17||index>18)throw std::runtime_error("engine fixture part range");const auto slot=selected*2+index-17;return {flags[slot],transforms.at(identities[slot])};
    }
};
}
int main(int argc,char** argv){
    return run(argc,argv,"SGNEP001",[](Record& b,contact::effects::EffectParticleBank& bank,contact::effects::EffectParticleFrameAudit& audit){
        VehicleParameters parameters;VehicleState car;sgn::retail::oracle::transfer(b,parameters,car,true);contact::effects::VehicleParticleState state;state.field_7628=word(b,0x7628);
        contact::effects::EffectStaging staging;contact::takeoff_oracle::transfer(std::span(b).subspan(0x7a00,0xd0),staging.particle,true);staging.initialized=word(b,parameters_at+28);
        Model model;static_assert(sizeof(ModelTransform)==80);std::memcpy(model.transforms.data(),b.data()+0x7b00,640);std::memcpy(model.flags.data(),b.data()+0x7d80,16);std::memcpy(model.identities.data(),b.data()+0x7d90,32);
        model.count=word(b,parameters_at+20);model.lod=word(b,parameters_at+24);
        contact::takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at,0x3400),bank,true);bank.active_signal=word(b,parameters_at+8);VehicleRandom random{word(b,parameters_at+4)};
        WorldGravity gravity;gravity.inverse_factor=std::bit_cast<float>(word(b,parameters_at+16));
        contact::effects::emit_engine_particles(parameters,car,state,gravity,model,staging,random,bank,sgn::retail::fixture::cold_world_template,&audit);
        sgn::retail::oracle::transfer(b,parameters,car,false);contact::takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at,0x3400),bank,false);
        contact::takeoff_oracle::transfer(std::span(b).subspan(0x7a00,0xd0),staging.particle,false);word(b,parameters_at+28,staging.initialized);
        word(b,0x7628,state.field_7628);word(b,parameters_at+4,random.state);word(b,parameters_at+8,bank.active_signal);
    });
}
