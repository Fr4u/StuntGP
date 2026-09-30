#include "source_pmd_brake_lights_fixture.hpp"
#include "sgn/contact_engine_particles.hpp"
#include "contact_takeoff_state.hpp"
#include "world_template_fixture.hpp"
#include <cstring>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::brake_test;using namespace contact::effects;
struct Capture {
    std::array<std::optional<unsigned>,2> slots;
    std::uint32_t counter{},initialized{},random{},active{};Bytes state;
    bool operator==(const Capture&)const=default;
};
VehicleParameters parameters(){VehicleParameters p{};p.rpm_max=6000;return p;}
VehicleState vehicle(){VehicleState c{};c.clutch=.5f;c.rpm=1000;c.position={113,217,319};c.basis_x={1,0,0};c.up={0,1,0};c.basis_z={0,0,1};return c;}
Capture run(ModelResources& models,std::size_t handle,const VehicleState& car=vehicle(),const VehicleParameters& p=parameters(),bool direct_legacy=false){
    VehicleParticleState state{};state.field_7628=1000;WorldGravity gravity{};gravity.inverse_factor=1;
    EffectStaging staging{};VehicleRandom random{0x12345};EffectParticleBank bank{};EffectParticleFrameAudit audit{};
    Capture result;
    if(direct_legacy){EngineParticleModelView old(models.at(handle),models.current_detail_level(handle));
        result.slots=emit_engine_particles(p,car,state,gravity,old,staging,random,bank,fixture::cold_world_template,&audit);
    }else result.slots=emit_engine_particles(p,car,state,gravity,models,handle,staging,random,bank,fixture::cold_world_template,&audit);
    result.counter=state.field_7628;result.initialized=staging.initialized;result.random=random.state;result.active=bank.active_signal;
    result.state.resize(0xd0+0x3400);contact::takeoff_oracle::transfer(std::span(result.state).first(0xd0),staging.particle,false);
    contact::takeoff_oracle::transfer_bank(std::span(result.state).subspan(0xd0),bank,false);return result;
}
void retail40(const std::filesystem::path& root){
    unsigned cars{},shadows{},calls{};std::uint64_t unchanged{};
    for(unsigned n=1;n<=20;++n){const auto name="car"+std::to_string(n);Fixture f(root,name);ModelResources legacy(f.assets,root);
        const auto old_handle=*legacy.resolve(legacy.load(name));auto& old=legacy.at(old_handle);auto model=f.view();
        rejects<FormatError>([&]{(void)f.models.at(f.handle);});rejects<FormatError>([&]{(void)legacy.source_at(old_handle);});
        for(unsigned slot=0;slot<4;++slot){model.current_detail_level().set(slot);old.current_detail_level()=slot;
            const auto lod=model.source_lod_index(slot),legacy_lod=old.source_lod_index(slot);
            for(unsigned side=0;side<2;++side){const auto first=static_cast<std::uint16_t>(side?0:4),second=static_cast<std::uint16_t>(side?4:0);
                model.part(lod,17).flags_02().set(first);model.part(lod,18).flags_02().set(second);
                old.source_part_flags_02(legacy_lod,17)=first;old.source_part_flags_02(legacy_lod,18)=second;
                const auto before=snapshot(f.bank,model.allocation());const auto actual=run(f.models,f.handle),expected=run(legacy,old_handle);
                check(actual==expected,"registry canonical/legacy retail differential");check(actual.slots[side].has_value()&&!actual.slots[1-side],"registry selected emitter missing");
                check(before==snapshot(f.bank,model.allocation()),"registry emitter changed PMD");unchanged+=before.size();++calls;
            }
        }++cars;
        Fixture shadow(root,name+"sh");auto inactive=vehicle();inactive.clutch=1;shadow.view().current_detail_level().set(0xffffffffu);
        const auto result=run(shadow.models,shadow.handle,inactive);check(!result.slots[0]&&!result.slots[1]&&result.counter==1000,"shadow early gate read invalid LOD");++shadows;
    }
    check(cars==20&&shadows==20&&calls==160,"retail registry coverage count");
    std::cout<<"{\"cars\":"<<cars<<",\"shadow_early_returns\":"<<shadows<<",\"registry_pairs\":"<<calls<<",\"unchanged_pmd_bytes\":"<<unchanged<<"}\n";
}
void owner_current(const std::filesystem::path& root){
    Fixture f(root);auto first=f.view();const auto second_handle=*f.models.resolve(f.models.load_source(f.bank,"car2"));check(second_handle!=f.handle,"second actual handle missing");
    auto second=f.models.source_at(second_handle);first.descriptor().field<unsigned>(0x24).set(17);second.descriptor().field<unsigned>(0x24).set(17);
    first.part(0,17).flags_02().set(4);first.part(1,17).flags_02().set(0);second.part(0,17).flags_02().set(0);
    first.current_detail_level().set(0);second.current_detail_level().set(0);
    check(run(f.models,f.handle).slots[0].has_value()&&!run(f.models,second_handle).slots[0],"registry ignored selected handle");
    first.current_detail_level().set(1);check(!run(f.models,f.handle).slots[0],"registry cached previous descriptor5C");
    first.descriptor().field<unsigned>(0x54).set(first.descriptor().field<unsigned>(0x48).value());first.current_detail_level().set(3);
    const auto old=run(f.models,f.handle);check(old.slots[0].has_value(),"registry rawslot3 alias ignored");
    const auto point=first.part(0,17).transform();auto transformed=point.value();transformed.position.x+=137;point.set(transformed);
    check(run(f.models,f.handle).state!=old.state,"registry retained stale position copy");
    check(first.part_count()==17&&second.part_count()==17,"registry repaired logical count");
}
void gates(const std::filesystem::path& root){
    Fixture f(root);auto m=f.view();m.descriptor().field<unsigned>(0x24).set(17);m.current_detail_level().set(4);
    auto inactive=vehicle();inactive.clutch=1;inactive.turbo_rpm=2;auto invalid=parameters();invalid.rpm_max=0;
    check(!run(f.models,f.handle,inactive,invalid).slots[0],"early gate evaluated divisor or selectedLOD");
    rejects<FormatError>([&]{(void)run(f.models,f.handle,vehicle(),invalid);});m.current_detail_level().set(0);
    auto part=m.part(0,17);m.bytes().field<unsigned>(part.location().offset+8).set(0xffffffffu);part.flags_02().set(0);part.flags().set(4);
    check(!run(f.models,f.handle).slots[0],"registry read transform before flags02");part.flags_02().set(4);
    rejects<FormatError>([&]{(void)run(f.models,f.handle);});
    rejects<std::out_of_range>([&]{(void)run(f.models,std::size_t(-1));});
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);auto m=f.view();f.bank.release_42eaa0();rejects<SourceAllocationError>([&]{(void)run(f.models,f.handle);});
    f.bank.begin_42ea80();const auto fresh=*f.models.resolve(f.models.load_source(f.bank,"car1"));
    rejects<SourceAllocationError>([&]{(void)run(f.models,f.handle);});check(run(f.models,fresh).slots[0].has_value(),"fresh registry allocation rejected");
    check(run(other.models,other.handle).slots[0].has_value(),"independent registry bank invalidated");
    bool caught=false;std::thread foreign([&]{try{(void)run(other.models,other.handle);}catch(const SourceAllocationError&){caught=true;}});foreign.join();check(caught,"foreign-thread registry read accepted");
    rejects<SourceAllocationError>([&]{(void)m.part_count();});
}
void legacy_bound(const std::filesystem::path& root){
    AssetStore assets(root);ModelResources models(assets,root);const auto handle=*models.resolve(models.load("car1"));auto& old=models.at(handle);
    unsigned session_lod=77;old.bind_current_detail_level(session_lod);check(&old.current_detail_level()==&session_lod,"legacy current LOD not existing Session owner");
    old.source_part_flags_02(0,17)=4;old.source_part_flags_02(0,18)=0;old.source_part_flags_02(1,17)=0;old.source_part_flags_02(1,18)=0;
    session_lod=0;const auto a=run(models,handle);check(a.slots[0].has_value()&&a==run(models,handle,vehicle(),parameters(),true),"legacy branch differs from previous adapter");
    session_lod=1;const auto b=run(models,handle);check(!b.slots[0]&&!b.slots[1]&&b==run(models,handle,vehicle(),parameters(),true),"legacy branch ignored bound current owner");
    session_lod=4;rejects<std::out_of_range>([&]{(void)run(models,handle);});
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected registry engine contract and retail root");const std::string_view mode=argv[1];
    if(mode=="retail40")retail40(argv[2]);else if(mode=="owner_current")owner_current(argv[2]);else if(mode=="gates")gates(argv[2]);
    else if(mode=="lifetime")lifetime(argv[2]);else if(mode=="legacy_bound")legacy_bound(argv[2]);else throw std::runtime_error("unknown registry engine contract");
    std::cout<<"passed "<<mode<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
