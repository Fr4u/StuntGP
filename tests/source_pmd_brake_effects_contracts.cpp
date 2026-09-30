#include "source_pmd_brake_effects_fixture.hpp"
using namespace sgn;using namespace sgn::brake_effects_test;
namespace {
void retail40(const std::filesystem::path& root){
    const auto nan=std::bit_cast<float>(0x7fc12345u);
    const std::array<std::array<float,4>,4> profiles{{{0,0,0,0},{1,2,2.0000002f,255},{-2,3,1e12f,.01f},{-0.0f,nan,17,257}}};
    unsigned calls{},shadows{};std::uint64_t intact{};
    for(unsigned id=1;id<=20;++id){const auto name="car"+std::to_string(id);Fixture f(root,name);ModelResources legacy(f.assets,root);const auto handle=*legacy.resolve(legacy.load(name));auto& old=legacy.at(handle);const auto model=f.view();
        for(unsigned raw=0;raw<4;++raw){model.current_detail_level().set(raw);old.current_detail_level()=raw;
            for(const auto profile:profiles){VehicleState a{},b{};for(unsigned i=0;i<4;++i)a.wheels[i].braking_signal=b.wheels[i].braking_signal=profile[i];
                for(unsigned slot=0;slot<4;++slot)for(unsigned i=6;i<=9;++i){model.part(model.source_lod_index(slot),i).flags().set(0xbeef);old.source_part(old.source_lod_index(slot),i).flags=0xbeef;}
                const auto before=snapshot(f.bank,model.allocation());const auto allowed=footprint(model,a);const auto physical=snapshot(a);
                update_vehicle_brake_effects(a,f.models,f.handle);update_vehicle_brake_effects(b,legacy,handle);check(strengths(a)==strengths(b),"retail strength differs");check_vehicle(a,physical);
                for(unsigned slot=0;slot<4;++slot)for(unsigned i=6;i<=9;++i)check(model.part(model.source_lod_index(slot),i).flags().value()==old.source_part(old.source_lod_index(slot),i).flags,"retail flags differ");
                for(unsigned i=6;i<=9;++i){const auto part=model.part(model.source_lod_index(raw),i);const auto value=part.transform().value();const auto reference=old.source_part(old.source_lod_index(raw),i);
                    check(std::memcmp(&value,&reference.transform,80)==0,"retail complete transform differs");const auto attributes=old.source_attributes(old.source_lod_index(raw),i);check(part.attribute_count().value()==attributes.size(),"retail count differs");
                    for(unsigned j=0;j<attributes.size();++j)for(unsigned lane=0;lane<8;++lane)check(part.attribute_byte(j,lane).value()==attributes[j][lane],"retail attribute differs");}
                intact+=brake_test::footprint(before,snapshot(f.bank,model.allocation()),allowed);++calls;
            }
        }
        Fixture shadow(root,name+"sh");VehicleState state{};rejects<FormatError>([&]{update_vehicle_brake_effects(state,shadow.models,shadow.handle);});++shadows;
    }
    check(calls==320&&shadows==20,"retail brake effects coverage");std::cout<<"{\"car_calls\":"<<calls<<",\"shadow_rejections\":"<<shadows<<",\"unchanged_pmd_bytes\":"<<intact<<"}\n";
}
void alias_order(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto b=m.bytes();m.current_detail_level().set(1);m.descriptor().field<unsigned>(0x24).set(0);
    const auto base=section(m,0x44);unsigned identity{};
    for(unsigned i:{1u,2u,3u,4u,6u,7u,8u,9u}){pointer(m,m.part(1,i),8,base+identity*80);++identity;}
    for(unsigned i=0;i<8;++i)for(unsigned lane=0;lane<20;++lane)b.field<unsigned>(base+i*80+lane*4).set(0x12340000u+i*32+lane);
    pointer(m,m.part(1,2),8,m.part(1,6).transform().location().offset);
    pointer(m,m.part(1,8),8,m.part(1,7).transform().location().offset);
    const auto attrs=section(m,0x24);for(unsigned i=0;i<16;++i)b.field<std::uint8_t>(attrs+i).set(0xa5);
    for(unsigned i=6;i<=8;++i){const auto part=m.part(1,i);part.attribute_count().set(2);b.field<std::uint16_t>(part.location().offset+0x26).set(0xbeef);pointer(m,part,0x34,attrs);}
    //Observe the dependency before wheel2 can overwrite the shared destination.
    VehicleState chain{};chain.wheels[0].braking_signal=12;chain.wheels[1].braking_signal=24;
    const auto chain_before=snapshot(f.bank,m.allocation());const auto chain_allowed=footprint(m,chain);
    update_vehicle_brake_effects(chain,f.models,f.handle);
    for(unsigned at=32;at<64;at+=4)check(b.field<unsigned>(base+5*80+at).value()==b.field<unsigned>(base+at).value(),"later wheel did not observe earlier aliased source writes");
    for(unsigned i=0;i<16;++i)check(b.field<std::uint8_t>(attrs+i).value()==(i%8==2?22:i%8==3?0x60:0xa5),"second wheel alpha alias was masked by a later write");
    brake_test::footprint(chain_before,snapshot(f.bank,m.allocation()),chain_allowed);
    VehicleState s{};for(unsigned i=0;i<3;++i)s.wheels[i].braking_signal=12.0f*static_cast<float>(i+1);
    const auto allowed=footprint(m,s);const auto before=snapshot(f.bank,m.allocation());update_vehicle_brake_effects(s,f.models,f.handle);
    for(unsigned at=32;at<64;at+=4){check(b.field<unsigned>(base+4*80+at).value()==b.field<unsigned>(base+at).value(),"first basis copy lost");check(b.field<unsigned>(base+5*80+at).value()==b.field<unsigned>(base+2*80+at).value(),"later shared destination did not win");}
    for(unsigned i=0;i<16;++i)check(b.field<std::uint8_t>(attrs+i).value()==(i%8==2?34:i%8==3?0x60:0xa5),"overlapping attributes lost last writer or unowned byte");
    check(m.part_count()==0,"logical count was repaired");brake_test::footprint(before,snapshot(f.bank,m.allocation()),allowed);
    m.descriptor().field<unsigned>(0x54).set(m.descriptor().field<unsigned>(0x4c).value());m.current_detail_level().set(3);m.part(1,7).flags().set(0xbeef);
    s={};s.wheels[1].braking_signal=3;update_vehicle_brake_effects(s,f.models,f.handle);check(m.part(1,7).flags().value()==1,"current raw alias not observed");
}
void current_preamble(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();VehicleState s{};s.wheels[0].braking_signal=12;
    for(unsigned raw=0;raw<3;++raw)m.part(raw,6).flags().set(0xbeef);
    m.current_detail_level().set(4);rejects<FormatError>([&]{update_vehicle_brake_effects(s,f.models,f.handle);});
    for(unsigned raw=0;raw<3;++raw)check(m.part(raw,6).flags().value()==0,"current LOD was preflighted before three clears");check(s.wheels[0].braking_signal==12,"invalid LOD changed strength");
    for(unsigned raw=0;raw<3;++raw)m.part(raw,6).flags().set(0xbeef);const auto old=m.descriptor().field<unsigned>(0x4c).value();m.descriptor().field<unsigned>(0x4c).set(0xffffffffu);
    rejects<FormatError>([&]{update_vehicle_brake_effects(s,f.models,f.handle);});m.descriptor().field<unsigned>(0x4c).set(old);
    check(m.part(0,6).flags().value()==0&&m.part(1,6).flags().value()==0xbeef&&m.part(2,6).flags().value()==0xbeef,"raw slot clears lost source prefix");
}
void inactive_pointers(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();m.current_detail_level().set(0);
    for(unsigned i:{1u,2u,3u,4u,6u,7u,8u,9u})pointer(m,m.part(0,i),8,0xffffffffu);
    for(unsigned i=6;i<=9;++i){const auto part=m.part(0,i);part.flags().set(0xbeef);part.attribute_count().set(0xffff);pointer(m,part,0x34,0xffffffffu);}
    VehicleState s{};s.wheels[1].braking_signal=-0.0f;s.wheels[2].braking_signal=std::bit_cast<float>(0x7fc12345u);const auto values=strengths(s);
    update_vehicle_brake_effects(s,f.models,f.handle);check(strengths(s)==values,"inactive strength bits changed");
    check(m.part(0,6).flags().value()==0,"missing initial clear");for(unsigned i=7;i<=9;++i)check(m.part(0,i).flags().value()==0xbeef,"inactive flag changed");
}
void guard_prefix(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();m.current_detail_level().set(0);const auto part=m.part(0,6),source=m.part(0,1);const auto destination=part.transform().location().offset,origin=source.transform().location().offset;
    VehicleState s{};s.wheels[0].braking_signal=3;pointer(m,part,8,0xffffffffu);pointer(m,source,8,0xffffffffu);
    rejects<FormatError>([&]{update_vehicle_brake_effects(s,f.models,f.handle);});check(s.wheels[0].braking_signal==3&&part.flags().value()==0,"destination guard prefix");
    pointer(m,part,8,destination);rejects<FormatError>([&]{update_vehicle_brake_effects(s,f.models,f.handle);});check(s.wheels[0].braking_signal==3,"source guard prefix");pointer(m,source,8,origin);
    part.attribute_count().set(1);pointer(m,part,0x34,0xffffffffu);rejects<FormatError>([&]{update_vehicle_brake_effects(s,f.models,f.handle);});
    check(s.wheels[0].braking_signal==1&&part.flags().value()==1,"attribute guard must follow decay and flag");
    const auto result=part.transform().value(),expected=source.transform().value();check(std::memcmp(&result.forward,&expected.forward,32)==0,"attribute guard lost basis writes");
    part.attribute_count().set(0);s.wheels[0].braking_signal=std::numeric_limits<float>::infinity();update_vehicle_brake_effects(s,f.models,f.handle);check(std::isinf(s.wheels[0].braking_signal)&&part.flags().value()==1,"zero count converted or dereferenced attributes");
    part.attribute_count().set(1);bool conversion=false;try{update_vehicle_brake_effects(s,f.models,f.handle);}catch(const FormatError& e){conversion=std::string_view(e.what())=="brake alpha outside source finite int64 domain";}check(conversion,"conversion did not precede invalid attribute pointer");
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);VehicleState s{};f.bank.release_42eaa0();rejects<SourceAllocationError>([&]{update_vehicle_brake_effects(s,f.models,f.handle);});
    rejects<std::out_of_range>([&]{update_vehicle_brake_effects(s,other.models,std::size_t(-1));});update_vehicle_brake_effects(s,other.models,other.handle);
    bool caught=false;std::thread worker([&]{try{VehicleState t{};update_vehicle_brake_effects(t,other.models,other.handle);}catch(const SourceAllocationError&){caught=true;}});worker.join();check(caught,"foreign thread brake effects accepted");
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected brake effects contract and retail root");const std::string_view mode=argv[1];
    if(mode=="retail40")retail40(argv[2]);else if(mode=="alias_order")alias_order(argv[2]);else if(mode=="current_preamble")current_preamble(argv[2]);else if(mode=="inactive_pointers")inactive_pointers(argv[2]);else if(mode=="guard_prefix")guard_prefix(argv[2]);else if(mode=="lifetime")lifetime(argv[2]);else throw std::runtime_error("unknown brake effects contract");
    std::cout<<"passed "<<mode<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
