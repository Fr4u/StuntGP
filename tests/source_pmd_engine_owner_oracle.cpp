#include "world_template_fixture.hpp"
#include "sgn/contact_engine_particles.hpp"
#include "source_pmd_engine_harness.hpp"
#include "vehicle_oracle_state.hpp"
#include "source_pmd_brake_lights_fixture.hpp"
#include <memory>

namespace {
using namespace sgn::retail;using namespace contact::emitter_oracle;
using sgn::brake_test::check;
struct Binding {
    sgn::brake_test::Fixture owner;std::array<std::uint32_t,8> positions{},parts{};unsigned slots{};
    Binding(const std::filesystem::path& root,const std::string& name,const Record& b):owner(root,name){
        auto model=owner.view();slots=(model.maximum_detail_level()+1)*2;check(slots<=8,"engine physical slot limit");
        for(unsigned i=0;i<slots;++i){auto part=model.part(i/2,17+i%2);parts[i]=part.location().offset;positions[i]=part.transform().location().offset;
            const auto transform=part.transform().value();check(std::memcmp(&transform,b.data()+0x7b00+i*80,80)==0,"engine corpus transform differs from actual PMD");}
        for(unsigned i=0;i<slots;++i){const auto identity=word(b,0x7d90+i*4);check(identity<slots,"engine transform alias outside actual slots");
            std::uint16_t flags;std::memcpy(&flags,b.data()+0x7d80+i*2,2);
            auto bytes=model.bytes().subview(parts[i],64);bytes.field<std::uint16_t>(2).set(flags);bytes.field<std::uint32_t>(8).set(positions[identity]);}
        model.descriptor().field<std::uint32_t>(0x24).set(word(b,parameters_at+20));model.current_detail_level().set(word(b,parameters_at+24));
    }
};
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("expected engine corpus, retail root and original scenario model map");
    std::ifstream mapping(argv[3]);std::vector<std::string> names;std::string name;
    while(std::getline(mapping,name)){if(!name.empty()&&name.back()=='\r')name.pop_back();if(name.empty())throw std::runtime_error("empty engine model mapping");names.push_back(name);}
    if(names.size()!=512)throw std::runtime_error("expected all512 original model mappings");
    std::unique_ptr<Binding> binding;std::uint64_t count17{},writes17{},untouched{};
    const int result=run(2,argv,"SGNEP001",[&](Record& b,contact::effects::EffectParticleBank& bank,contact::effects::EffectParticleFrameAudit& audit,unsigned scenario,unsigned step){
        if(step==0)binding=std::make_unique<Binding>(argv[2],names.at(scenario),b);
        auto model=binding->owner.view();check(model.part_count()==word(b,parameters_at+20)&&model.current_detail_level().value()==word(b,parameters_at+24),"engine model descriptor continuation drift");
        const auto before=sgn::brake_test::snapshot(binding->owner.bank,model.allocation());
        const auto old_bank=std::vector<std::uint8_t>(b.begin()+bank_at,b.end());
        VehicleParameters parameters;VehicleState car;sgn::retail::oracle::transfer(b,parameters,car,true);contact::effects::VehicleParticleState state;state.field_7628=word(b,0x7628);
        contact::effects::EffectStaging staging;contact::takeoff_oracle::transfer(std::span(b).subspan(0x7a00,0xd0),staging.particle,true);staging.initialized=word(b,parameters_at+28);
        contact::takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at,0x3400),bank,true);bank.active_signal=word(b,parameters_at+8);VehicleRandom random{word(b,parameters_at+4)};
        WorldGravity gravity;gravity.inverse_factor=std::bit_cast<float>(word(b,parameters_at+16));
        contact::effects::emit_engine_particles(parameters,car,state,gravity,binding->owner.models,binding->owner.handle,staging,random,bank,sgn::retail::fixture::cold_world_template,&audit);
        sgn::retail::oracle::transfer(b,parameters,car,false);contact::takeoff_oracle::transfer_bank(std::span(b).subspan(bank_at,0x3400),bank,false);
        contact::takeoff_oracle::transfer(std::span(b).subspan(0x7a00,0xd0),staging.particle,false);word(b,parameters_at+28,staging.initialized);
        word(b,0x7628,state.field_7628);word(b,parameters_at+4,random.state);word(b,parameters_at+8,bank.active_signal);
        check(before==sgn::brake_test::snapshot(binding->owner.bank,model.allocation()),"engine changed canonical PMD payload");untouched+=before.size();
        if(model.part_count()==17){++count17;if(!std::equal(old_bank.begin(),old_bank.end(),b.begin()+bank_at))++writes17;}
    });
    if(result==0)std::cout<<"{\"count17_records\":"<<count17<<",\"count17_bank_writes\":"<<writes17<<",\"untouched_pmd_bytes\":"<<untouched<<"}\n";
    return result;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
