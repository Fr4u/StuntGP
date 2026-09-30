#include "source_pmd_brake_lights_fixture.hpp"
#include <limits>
namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::brake_test;
void authored_retail(const std::filesystem::path& root){
    std::size_t bytes{};unsigned calls{},shadow_rejections{};
    for(unsigned car=1;car<=20;++car){const auto name="car"+std::to_string(car);Fixture f(root,name);ModelResources legacy(f.assets,root);
        const auto handle=*legacy.resolve(legacy.load(name));auto& old=legacy.at(handle);const auto m=f.view();
        for(unsigned slot=0;slot<4;++slot){m.current_detail_level().set(slot);old.current_detail_level()=slot;
            for(const std::int32_t signal:{0,32,152,256,std::numeric_limits<std::int32_t>::min(),-1}){
                VehicleState a{},b{};VehicleTrailInputs trails{};VehicleModelEffects ae{},be{};
                a.brake_signal=b.brake_signal=signal;trails.field_28a0=calls%3;ae.cached_brake=be.cached_brake=calls%2?0:120;
                const auto p=m.part(m.source_lod_index(slot),5);const auto before=snapshot(f.bank,m.allocation());const auto allowed=allowed_writes(p,true);
                update_vehicle_brake_lights(a,trails,ae,f.models,f.handle);update_vehicle_brake_lights(b,trails,be,legacy,handle);
                check(a.brake_signal==b.brake_signal&&ae.cached_brake==be.cached_brake,"authored scalar legacy differential");
                const auto lp=old.source_part(old.source_lod_index(slot),5);const auto la=old.source_attributes(old.source_lod_index(slot),5);
                check(p.flags().value()==lp.flags&&p.attributes().size()==la.size(),"authored part legacy differential");
                for(unsigned i=0;i<la.size();++i)check(p.attributes().at(i).value()==la[i],"authored attributes legacy differential");
                bytes+=footprint(before,snapshot(f.bank,m.allocation()),allowed);++calls;
            }
        }
        Fixture shadow(root,name+"sh");VehicleState s{};VehicleTrailInputs t{};VehicleModelEffects e{};
        rejects<FormatError>([&]{update_vehicle_brake_lights(s,t,e,shadow.models,shadow.handle);});++shadow_rejections;
    }
    std::cout<<"authored_retail_models=40 positive_calls="<<calls<<" shadow_part5_rejections="<<shadow_rejections<<" unchanged_bytes="<<bytes<<'\n';
}
void alias_and_pointer(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto d=m.descriptor();const auto original0=d.field<unsigned>(0x48).value();
    const auto lane1=d.field<unsigned>(0x4c).value();d.field<unsigned>(0x54).set(lane1);m.current_detail_level().set(3);
    const auto p=m.part(m.source_lod_index(3),5);const auto offset=p.location().offset;const auto other=m.part(1,6);
    const auto other_at=other.location().offset;const auto bytes=m.bytes();
    // Point part5 at another part's current attribute range. Both views alias.
    check(other.attribute_count().value()>0,"alias fixture needs an attribute");
    bytes.field<unsigned>(offset+0x34).set(bytes.field<unsigned>(other_at+0x34).value());p.attribute_count().set(1);
    const auto cached=other.attribute_byte(0,2);const auto before=snapshot(f.bank,m.allocation());
    VehicleState s{};s.brake_signal=200;VehicleTrailInputs t{};VehicleModelEffects e{};
    update_vehicle_brake_lights(s,t,e,f.models,f.handle);check(cached.value()==168&&other.attribute_byte(0,3).value()==0x60,"current pointer alias not updated");
    (void)footprint(before,snapshot(f.bank,m.allocation()),allowed_writes(p,true));
    // Change descriptor5C after the first call, with no new adapter construction.
    m.current_detail_level().set(0);check(d.field<unsigned>(0x48).value()==original0,"rawslot0 mutated");
    const auto p0=m.part(m.source_lod_index(0),5);p0.flags().set(0xbeef);s.brake_signal=0;e.cached_brake=0;
    update_vehicle_brake_lights(s,t,e,f.models,f.handle);check(p0.flags().value()==0&&p.flags().value()==1,"cached selectedLOD used");
    rejects<FormatError>([&]{(void)p.attribute_byte(1,2);});rejects<FormatError>([&]{(void)p.attribute_byte(0,8);});
}
void order_and_gates(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto p=m.part(0,5);const auto at=p.location().offset;
    const auto ptr=m.bytes().field<unsigned>(at+0x34);ptr.set(0xffffffff);p.attribute_count().set(0xffff);
    VehicleState s{};VehicleTrailInputs t{};VehicleModelEffects e{};s.brake_signal=152;e.cached_brake=120;
    // Equal cache skips both count and malformed pointer; flag still changes.
    update_vehicle_brake_lights(s,t,e,m);check(s.brake_signal==120&&p.flags().value()==1,"cache-equal ordering");
    // Zero low16 count permits an unreferenced invalid pointer, even with upper16!=0.
    m.bytes().field<unsigned>(at+0x24).set(0xabcd0000);s.brake_signal=121;e.cached_brake=0;
    update_vehicle_brake_lights(s,t,e,m);check(s.brake_signal==89&&e.cached_brake==89,"zero-count ordering");
    check(m.bytes().field<unsigned>(at+0x24).value()==0xabcd0000,"count high16 overwritten");
    p.attribute_count().set(1);s.brake_signal=80;e.cached_brake=7;p.flags().set(0);
    rejects<FormatError>([&]{update_vehicle_brake_lights(s,t,e,m);});
    check(s.brake_signal==48&&e.cached_brake==48&&p.flags().value()==1,"source prefix lost on host pointer rejection");
    m.current_detail_level().set(4);s.brake_signal=64;e.cached_brake=7;
    rejects<FormatError>([&]{update_vehicle_brake_lights(s,t,e,m);});check(s.brake_signal==32&&e.cached_brake==7,"invalid LOD prefix ordering");
    m.current_detail_level().set(0);p.attribute_count().set(0);s.brake_signal=std::numeric_limits<std::int32_t>::min();
    update_vehicle_brake_lights(s,t,e,m);check(s.brake_signal==2147483616,"signed SUB32 overflow not source wrapping");
    s.brake_signal=-1;t.field_28a0=2;update_vehicle_brake_lights(s,t,e,m);check(s.brake_signal==0,"signed clamp/exact gate failed");
    s.brake_signal=1;t.field_28a0=1;update_vehicle_brake_lights(s,t,e,m);check(s.brake_signal==120,"exact-one floor failed");
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);const auto m=f.view();const auto p=m.part(0,5);const auto byte=p.attribute_byte(0,2);
    f.bank.release_42eaa0();rejects<SourceAllocationError>([&]{byte.set(12);});VehicleState s{};VehicleTrailInputs t{};VehicleModelEffects e{};
    rejects<SourceAllocationError>([&]{update_vehicle_brake_lights(s,t,e,f.models,f.handle);});
    const auto ov=other.view();ov.part(0,5).attribute_byte(0,2).set(45);check(ov.part(0,5).attribute_byte(0,2).value()==45,"cross-bank alias");
    f.bank.begin_42ea80();(void)f.models.load_source(f.bank,"car1");rejects<SourceAllocationError>([&]{byte.set(67);});
    bool rejected=false;std::thread worker([&]{try{ov.part(0,5).attribute_byte(0,2).set(89);}catch(const SourceAllocationError&){rejected=true;}});worker.join();check(rejected,"foreign-thread mutation accepted");
    std::optional<SourcePmdField<std::uint8_t>> dead;{Fixture temporary(root);dead.emplace(temporary.view().part(0,5).attribute_byte(0,2));}
    rejects<SourceAllocationError>([&]{dead->set(90);});rejects<std::out_of_range>([&]{update_vehicle_brake_lights(s,t,e,other.models,std::size_t(-1));});
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected brake contract and retail root");const std::string_view mode=argv[1];
    if(mode=="retail40")authored_retail(argv[2]);else if(mode=="alias_pointer")alias_and_pointer(argv[2]);
    else if(mode=="order_gates")order_and_gates(argv[2]);else if(mode=="lifetime")lifetime(argv[2]);else throw std::runtime_error("unknown brake contract");
    std::cout<<"authored_brake_contract="<<mode<<" errors=0 activation=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"brake contract: "<<e.what()<<'\n';return 1;}
