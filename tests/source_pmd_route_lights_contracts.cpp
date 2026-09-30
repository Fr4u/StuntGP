#include "source_pmd_brake_lights_fixture.hpp"
#include <limits>
namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::brake_test;
struct State {
    VehicleState physical{};VehicleRaceState race{};VehicleTrailInputs trails{};VehicleModelEffects effects{};
    std::array<float,128> lights{};std::array<unsigned,128> ambience{};unsigned adjacent{0xdeadb33f},points{128};
    State(){for(unsigned i=0;i<8;++i)physical.camera_effects.values[i]=100+i;effects.route_envelope=0xcafebabe;ambience.fill(0x3e800000);}
    VehicleRoutePresentation route()const{return {lights,ambience,adjacent,points};}
    void run(Fixture& f){update_vehicle_route_lights(physical,race,trails,effects,f.models,f.handle,route());}
};
std::set<unsigned> flag_bytes(SourcePmdModelView m){
    std::set<unsigned> out;
    for(unsigned raw=0;raw<4;++raw)for(unsigned index:{10u,5u}){const auto at=m.part(m.source_lod_index(raw),index).flags().location().offset;out.insert(at);out.insert(at+1);}
    return out;
}
void seed_flags(SourcePmdModelView m,std::uint16_t value){for(unsigned raw=0;raw<4;++raw)for(unsigned index:{10u,5u})m.part(m.source_lod_index(raw),index).flags().set(value);}
void check_flags(SourcePmdModelView m,std::uint16_t value){for(unsigned raw=0;raw<4;++raw)for(unsigned index:{10u,5u})check(m.part(m.source_lod_index(raw),index).flags().value()==value,"route rawslot flags mismatch");}
void authored_retail(const std::filesystem::path& root){
    unsigned calls{},shadows{};std::size_t untouched{};
    for(unsigned car=1;car<=20;++car){const auto name="car"+std::to_string(car);Fixture f(root,name);const auto m=f.view();
        ModelResources legacy(f.assets,root);const auto handle=*legacy.resolve(legacy.load(name));auto& old=legacy.at(handle);
        for(unsigned mode=0;mode<8;++mode){State a;a.trails.field_28a0=mode%4;
            a.lights.fill(mode%2?0.f:1.f);
            if(mode==4)a.lights.fill(std::bit_cast<float>(0x7fc12345u));
            if(mode==5)a.lights.fill(-0.f);
            if(mode==6)a.lights.fill(std::numeric_limits<float>::infinity());
            if(mode==7){a.race.spline_index=0u-128u;a.ambience[0]=0x3f800000;a.adjacent=0x89abcdef;}
            State b=a;m.current_detail_level().set(0xffffffff);old.current_detail_level()=mode%4;
            seed_flags(m,0x7bad);
            for(unsigned raw=0;raw<4;++raw)for(unsigned index:{10u,5u})old.source_part(old.source_lod_index(raw),index).flags=0x7bad;
            const auto before=snapshot(f.bank,m.allocation());const auto allowed=flag_bytes(m);a.run(f);
            update_vehicle_route_lights(b.physical,b.race,b.trails,b.effects,legacy,handle,b.route());
            check(a.physical.camera_effects.values==b.physical.camera_effects.values&&a.trails.field_28a0==b.trails.field_28a0&&a.effects.route_envelope==b.effects.route_envelope,"authored route legacy differential");
            for(unsigned raw=0;raw<4;++raw)for(unsigned index:{10u,5u})check(m.part(m.source_lod_index(raw),index).flags().value()==old.source_part(old.source_lod_index(raw),index).flags,"authored route flag differential");
            check(m.current_detail_level().value()==0xffffffff,"route read/repaired poisoned selectedLOD");untouched+=footprint(before,snapshot(f.bank,m.allocation()),allowed);++calls;
        }
        Fixture shadow(root,name+"sh");State s;s.points=0;const auto sm=shadow.view();sm.current_detail_level().set(0xffffffff);sm.descriptor().field<unsigned>(0x58).set(0xffffffff);
        for(unsigned raw=0;raw<4;++raw)sm.descriptor().field<unsigned>(0x48+raw*4).set(0xffffffff);
        const auto before=snapshot(shadow.bank,sm.allocation());s.run(shadow);
        check(before==snapshot(shadow.bank,sm.allocation())&&s.effects.route_envelope==0xcafebabe&&s.physical.camera_effects.values[4]==104,"shadow did not early return before unused fields");++shadows;
    }
    std::cout<<"authored_retail_models=40 positive_calls="<<calls<<" shadow_early_returns="<<shadows<<" unchanged_bytes="<<untouched<<'\n';
}
void gates(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto d=m.descriptor();State s;s.lights.fill(1);
    for(unsigned raw=0;raw<4;++raw)d.field<unsigned>(0x48+raw*4).set(0xffffffff);
    d.field<unsigned>(0x58).set(0xffffffff);m.current_detail_level().set(0xffffffff);
    for(unsigned count:{0u,10u}){d.field<unsigned>(0x24).set(count);s.points=0;s.run(f);check(s.effects.route_envelope==0xcafebabe&&s.physical.camera_effects.values[4]==104,"count gate read route or reset scalar");}
    d.field<unsigned>(0x24).set(25);s.points=128;s.trails.field_28a0=1;s.run(f);
    check(s.physical.camera_effects.values[4]==104&&s.effects.route_envelope==s.ambience[0],"unchanged-on touched parts or reset counters");
    s.trails.field_28a0=0;s.lights.fill(0);s.effects.route_envelope=0;s.run(f);check(s.effects.route_envelope==s.ambience[0],"unchanged-off touched parts");
    s.points=0;s.effects.route_envelope=123;rejects<FormatError>([&]{s.run(f);});check(s.effects.route_envelope==123,"invalid divisor wrote envelope");
}
void write_order(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto d=m.descriptor();State s;s.lights.fill(1);seed_flags(m,0x7bad);
    const auto p0a=m.part(0,10).flags(),p0b=m.part(0,5).flags(),p2=m.part(2,10).flags();
    d.field<unsigned>(0x4c).set(0xffffffff);rejects<FormatError>([&]{s.run(f);});
    check(s.trails.field_28a0==1&&s.physical.camera_effects.values[4]==0&&s.physical.camera_effects.values[7]==0,"on scalar prefix not retained");
    check(p0a.value()==1&&p0b.value()==1&&p2.value()==0x7bad&&s.effects.route_envelope==0xcafebabe,"rawslot order/envelope tail wrong");
    Fixture tail(root);const auto tm=tail.view();const auto td=tm.descriptor();const auto part_start=td.field<unsigned>(0x48).value();
    const auto last_flag=tm.part(2,24).flags();last_flag.set(0x7bad);
    //Raw0 part5 still fits actual part74; part10 is past the section. Source
    //writes part10 first, so the part5 flag must remain untouched on host failure.
    td.field<unsigned>(0x48).set(part_start+69*64);State t;t.lights.fill(1);rejects<FormatError>([&]{t.run(tail);});
    check(last_flag.value()==0x7bad&&t.effects.route_envelope==0xcafebabe,"part5 written before part10");
}
void boundaries(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();State s;s.race.spline_index=0u-128u;s.lights.fill(1);s.ambience[0]=0;s.trails.field_28a0=2;seed_flags(m,0x7bad);
    s.run(f);check(s.effects.route_envelope==s.adjacent&&s.trails.field_28a0==0&&s.physical.camera_effects.values[4]==104,"index128 off did not read ambience0 bits");check_flags(m,0);
    s.ambience[0]=0x3f800000;s.run(f);check(s.effects.route_envelope==s.adjacent&&s.trails.field_28a0==1&&s.physical.camera_effects.values[4]==0,"index128 on/helper reset wrong");check_flags(m,1);
    s.ambience[0]=0x7fc54321;s.physical.camera_effects.values[4]=77;s.run(f);check(s.trails.field_28a0==0&&s.physical.camera_effects.values[4]==77,"unordered FCOMP did not take off path");
    s.points=0x2000000;s.race.spline_index=0u-s.points;s.lights[0]=1;s.ambience[0]=0x10203040;s.run(f);
    check(s.effects.route_envelope==0x10203040,"SHL32 overflow before DIV lost");
}
void aliases_lifetime(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto d=m.descriptor();const auto lane1=d.field<unsigned>(0x4c).value();const auto shared=m.part(1,10).flags();
    for(unsigned raw=0;raw<4;++raw)d.field<unsigned>(0x48+raw*4).set(lane1);m.current_detail_level().set(0xffffffff);
    State s;s.lights.fill(1);s.run(f);check(shared.value()==1,"shared rawslot flag did not observe write");
    s.lights.fill(0);s.run(f);check(shared.value()==0,"shared rawslot flag did not observe clear");
    bool rejected=false;std::thread worker([&]{try{s.run(f);}catch(const SourceAllocationError&){rejected=true;}});worker.join();check(rejected,"foreign thread route read accepted");
    f.bank.release_42eaa0();rejects<SourceAllocationError>([&]{s.run(f);});rejects<SourceAllocationError>([&]{shared.set(1);});
    f.bank.begin_42ea80();(void)f.models.load_source(f.bank,"car1");rejects<SourceAllocationError>([&]{shared.set(1);});
    std::optional<SourcePmdModelView> dead;{Fixture temporary(root);dead.emplace(temporary.view());}
    rejects<SourceAllocationError>([&]{update_vehicle_route_lights(s.physical,s.race,s.trails,s.effects,*dead,s.route());});
    rejects<std::out_of_range>([&]{update_vehicle_route_lights(s.physical,s.race,s.trails,s.effects,f.models,std::size_t(-1),s.route());});
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected route contract and retail root");const std::string_view mode=argv[1];
    if(mode=="retail40")authored_retail(argv[2]);else if(mode=="gates")gates(argv[2]);else if(mode=="write_order")write_order(argv[2]);
    else if(mode=="boundaries")boundaries(argv[2]);else if(mode=="aliases_lifetime")aliases_lifetime(argv[2]);else throw std::runtime_error("unknown route contract");
    std::cout<<"authored_route_contract="<<mode<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"route contract: "<<e.what()<<'\n';return 1;}
